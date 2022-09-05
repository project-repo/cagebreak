/*
 * Cagebreak: A Wayland tiling compositor.
 *
 * Copyright (C) 2018-2020 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200812L

#include "config.h"

#include <fontconfig/fontconfig.h>
#include <pango.h>
#include <pango/pangocairo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-client.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_server_decoration.h>
#if CG_HAS_XWAYLAND
#include <wlr/types/wlr_xcursor_manager.h>
#endif
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#if CG_HAS_XWAYLAND
#include <wlr/xwayland.h>
#endif

#include "idle_inhibit_v1.h"
#include "input_manager.h"
#include "ipc_server.h"
#include "keybinding.h"
#include "message.h"
#include "output.h"
#include "parse.h"
#include "seat.h"
#include "server.h"
#include "workspace.h"
#include "xdg_shell.h"
#if CG_HAS_XWAYLAND
#include "xwayland.h"
#endif

#ifndef WAIT_ANY
#define WAIT_ANY -1
#endif

bool show_info = false;

void
set_sig_handler(int sig, void (*action)(int)) {
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_handler = action;
	sigemptyset(&act.sa_mask);
	if(sigaction(sig, &act, NULL)) {
		wlr_log(WLR_ERROR, "Error setting signal handler: %s\n",
		        strerror(errno));
	}
}

static bool
drop_permissions(void) {
	if(getuid() != geteuid() || getgid() != getegid()) {
		// Set gid before uid
		if(setgid(getgid()) != 0 || setuid(getuid()) != 0) {
			wlr_log(WLR_ERROR, "Unable to drop root, refusing to start");
			return false;
		}
	}

	if(setgid(0) != -1 || setuid(0) != -1) {
		wlr_log(WLR_ERROR, "Unable to drop root (we shouldn't be able to "
		                   "restore it after setuid), refusing to start");
		return false;
	}

	return true;
}

static int
handle_signal(int signal, void *const data) {
	struct cg_server *server = data;

	switch(signal) {
	case SIGINT:
		/* Fallthrough */
	case SIGTERM:
		display_terminate(server);
		return 0;
	case SIGALRM: {
		struct cg_output *output;
		wl_list_for_each(output, &server->outputs, link) {
			message_clear(output);
		}
		return 0;
	}
	default:
		return 0;
	}
}

static void
usage(FILE *file, const char *const cage) {
	fprintf(file,
	        "Usage: %s [OPTIONS]\n"
	        "\n"
	        " -r\t Rotate the output 90 degrees clockwise, specify up to three "
	        "times\n"
	        " -h\t Display this help message\n"
	        " -v\t Show the version number and exit\n"
	        " -s\t Show information about the current setup and exit\n",
	        cage);
}

static bool
parse_args(struct cg_server *server, int argc, char *argv[]) {
	int c;
	while((c = getopt(argc, argv, "rhvs")) != -1) {
		switch(c) {
		case 'r':
			server->output_transform++;
			if(server->output_transform > WL_OUTPUT_TRANSFORM_270) {
				server->output_transform = WL_OUTPUT_TRANSFORM_NORMAL;
			}
			break;
		case 'h':
			usage(stdout, argv[0]);
			return false;
		case 'v':
			fprintf(stdout, "Cagebreak version " CG_VERSION "\n");
			exit(0);
		case 's':
			show_info = true;
			break;
		default:
			usage(stderr, argv[0]);
			return false;
		}
	}

	if(optind > argc) {
		usage(stderr, argv[0]);
		return false;
	}

	return true;
}

/* Parse config file. Lines longer than "max_line_size" are ignored */
int
set_configuration(struct cg_server *server,
                  const char *const config_file_path) {
	FILE *config_file = fopen(config_file_path, "r");
	if(config_file == NULL) {
		wlr_log(WLR_ERROR, "Could not open config file \"%s\"",
		        config_file_path);
		return 1;
	}
	char line[MAX_LINE_SIZE * sizeof(char)];
	for(unsigned int line_num = 1;
	    fgets(line, MAX_LINE_SIZE, config_file) != NULL; ++line_num) {
		line[strcspn(line, "\n")] = '\0';
		if(*line != '\0' && *line != '#') {
			char *errstr;
			if(parse_rc_line(server, line, &errstr) != 0) {
				wlr_log(WLR_ERROR, "Error in config file \"%s\", line %d\n",
				        config_file_path, line_num);
				fclose(config_file);
				if(errstr != NULL) {
					free(errstr);
				}
				return -1;
			}
		}
	}
	fclose(config_file);
	return 0;
}

char *
get_config_file() {
	const char *config_home_path = getenv("XDG_CONFIG_HOME");
	char *addition = "/cagebreak/config";
	if(config_home_path == NULL || config_home_path[0] == '\0') {
		config_home_path = getenv("HOME");
		if(config_home_path == NULL || config_home_path[0] == '\0') {
			return NULL;
		}
		addition = "/.config/cagebreak/config";
	}
	char *config_path = malloc(
	    (strlen(config_home_path) + strlen(addition) + 1) * sizeof(char));
	if(!config_path) {
		wlr_log(WLR_ERROR, "Failed to allocate space for configuration path");
		return NULL;
	}
	sprintf(config_path, "%s%s", config_home_path, addition);
	return config_path;
}

int
main(int argc, char *argv[]) {
	struct cg_server server = {0};
	struct wl_event_loop *event_loop = NULL;
	struct wl_event_source *sigint_source = NULL;
	struct wl_event_source *sigterm_source = NULL;
	struct wl_event_source *sigalrm_source = NULL;
	struct wlr_backend *backend = NULL;
	struct wlr_compositor *compositor = NULL;
	struct wlr_data_device_manager *data_device_manager = NULL;
	struct wlr_server_decoration_manager *server_decoration_manager = NULL;
	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_manager = NULL;
	struct wlr_export_dmabuf_manager_v1 *export_dmabuf_manager = NULL;
	struct wlr_screencopy_manager_v1 *screencopy_manager = NULL;
	struct wlr_data_control_manager_v1 *data_control_manager = NULL;
	struct wlr_viewporter *viewporter = NULL;
	struct wlr_presentation *presentation = NULL;
	struct wlr_xdg_output_manager_v1 *output_manager = NULL;
	struct wlr_gamma_control_manager_v1 *gamma_control_manager = NULL;
	struct wlr_xdg_shell *xdg_shell = NULL;
#if CG_HAS_XWAYLAND
	struct wlr_xwayland *xwayland = NULL;
	struct wlr_xcursor_manager *xcursor_manager = NULL;
#endif
	int ret = 0;

	if(!parse_args(&server, argc, argv)) {
		return 1;
	}

#ifdef DEBUG
	wlr_log_init(WLR_DEBUG, NULL);
#else
	wlr_log_init(WLR_ERROR, NULL);
#endif

	wl_list_init(&server.input_config);
	wl_list_init(&server.output_config);
	wl_list_init(&server.output_priorities);

	server.modes = malloc(4 * sizeof(char *));
	if(!server.modes) {
		wlr_log(WLR_ERROR, "Error allocating mode array");
		return -1;
	}

	/* Wayland requires XDG_RUNTIME_DIR to be set. */
	if(!getenv("XDG_RUNTIME_DIR")) {
		wlr_log(WLR_ERROR, "XDG_RUNTIME_DIR is not set in the environment");
		return 1;
	}

	server.wl_display = wl_display_create();
	if(!server.wl_display) {
		wlr_log(WLR_ERROR, "Cannot allocate a Wayland display");
		return 1;
	}

	server.running = true;

	server.modes[0] = strdup("top");
	server.modes[1] = strdup("root");
	server.modes[2] = strdup("resize");
	server.modes[3] = NULL;
	if(server.modes[0] == NULL || server.modes[1] == NULL ||
	   server.modes[2] == NULL) {
		wlr_log(WLR_ERROR, "Error allocating default modes");
		return 1;
	}

	server.nws = 1;
	server.views_curr_id = 0;
	server.tiles_curr_id = 0;
	server.message_config.fg_color[0] = 0.0;
	server.message_config.fg_color[1] = 0.0;
	server.message_config.fg_color[2] = 0.0;
	server.message_config.fg_color[3] = 1.0;

	server.message_config.bg_color[0] = 0.9;
	server.message_config.bg_color[1] = 0.85;
	server.message_config.bg_color[2] = 0.85;
	server.message_config.bg_color[3] = 1.0;

	server.message_config.display_time = 2;
	server.message_config.font = strdup("pango:Monospace 10");

	event_loop = wl_display_get_event_loop(server.wl_display);
	sigint_source =
	    wl_event_loop_add_signal(event_loop, SIGINT, handle_signal, &server);
	sigterm_source =
	    wl_event_loop_add_signal(event_loop, SIGTERM, handle_signal, &server);
	sigalrm_source =
	    wl_event_loop_add_signal(event_loop, SIGALRM, handle_signal, &server);
	server.event_loop = event_loop;

	backend = wlr_backend_autocreate(server.wl_display);
	if(!backend) {
		wlr_log(WLR_ERROR, "Unable to create the wlroots backend");
		ret = 1;
		goto end;
	}
	server.backend = backend;

	if(!drop_permissions()) {
		ret = 1;
		goto end;
	}

	server.keybindings = keybinding_list_init();
	if(server.keybindings == NULL || server.keybindings->keybindings == NULL) {
		wlr_log(WLR_ERROR, "Unable to allocate keybindings");
		ret = 1;
		goto end;
	}

	server.renderer = wlr_renderer_autocreate(backend);
	if (!server.renderer) {
		wlr_log(WLR_ERROR, "Unable to create the wlroots renderer");
		ret = 1;
		goto end;
	}

	server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
	if (!server.allocator) {
		wlr_log(WLR_ERROR, "Unable to create the wlroots allocator");
		ret = 1;
		goto end;
	}

	wlr_renderer_init_wl_display(server.renderer, server.wl_display);

	server.bg_color = (float[4]){0, 0, 0, 1};
	wl_list_init(&server.outputs);
	wl_list_init(&server.disabled_outputs);

	server.output_layout = wlr_output_layout_create();
	if(!server.output_layout) {
		wlr_log(WLR_ERROR, "Unable to create output layout");
		ret = 1;
		goto end;
	}

	if(ipc_init(&server) != 0) {
		wlr_log(WLR_ERROR, "Failed to initialize IPC");
		ret = 1;
		goto end;
	}

	server.scene = wlr_scene_create();
	if (!server.scene) {
		wlr_log(WLR_ERROR, "Unable to create scene");
		ret = 1;
		goto end;
	}
	wlr_scene_attach_output_layout(server.scene, server.output_layout);

	compositor = wlr_compositor_create(server.wl_display, server.renderer);
	if(!compositor) {
		wlr_log(WLR_ERROR, "Unable to create the wlroots compositor");
		ret = 1;
		goto end;
	}

	data_device_manager = wlr_data_device_manager_create(server.wl_display);
	if(!data_device_manager) {
		wlr_log(WLR_ERROR, "Unable to create the data device manager");
		ret = 1;
		goto end;
	}

	server.input = input_manager_create(&server);

	data_control_manager =
	    wlr_data_control_manager_v1_create(server.wl_display);
	if(!data_control_manager) {
		wlr_log(WLR_ERROR, "Unable to create the data control manager");
		ret = 1;
		goto end;
	}

	/* Configure a listener to be notified when new outputs are
	 * available on the backend. We use this only to detect the
	 * first output and ignore subsequent outputs. */
	server.new_output.notify = handle_new_output;
	wl_signal_add(&backend->events.new_output, &server.new_output);

	server.seat = seat_create(&server, backend);
	if(!server.seat) {
		wlr_log(WLR_ERROR, "Unable to create the seat");
		ret = 1;
		goto end;
	}

	server.idle = wlr_idle_create(server.wl_display);
	if(!server.idle) {
		wlr_log(WLR_ERROR, "Unable to create the idle tracker");
		ret = 1;
		goto end;
	}

	server.idle_inhibit_v1 = wlr_idle_inhibit_v1_create(server.wl_display);
	if(!server.idle_inhibit_v1) {
		wlr_log(WLR_ERROR, "Cannot create the idle inhibitor");
		ret = 1;
		goto end;
	}
	server.new_idle_inhibitor_v1.notify = handle_idle_inhibitor_v1_new;
	wl_signal_add(&server.idle_inhibit_v1->events.new_inhibitor,
	              &server.new_idle_inhibitor_v1);
	wl_list_init(&server.inhibitors);

	xdg_shell = wlr_xdg_shell_create(server.wl_display);
	if(!xdg_shell) {
		wlr_log(WLR_ERROR, "Unable to create the XDG shell interface");
		ret = 1;
		goto end;
	}
	server.new_xdg_shell_surface.notify = handle_xdg_shell_surface_new;
	wl_signal_add(&xdg_shell->events.new_surface,
	              &server.new_xdg_shell_surface);

	xdg_decoration_manager =
	    wlr_xdg_decoration_manager_v1_create(server.wl_display);
	if(!xdg_decoration_manager) {
		wlr_log(WLR_ERROR, "Unable to create the XDG decoration manager");
		ret = 1;
		goto end;
	}
	wl_signal_add(&xdg_decoration_manager->events.new_toplevel_decoration,
	              &server.xdg_toplevel_decoration);
	server.xdg_toplevel_decoration.notify = handle_xdg_toplevel_decoration;

	server_decoration_manager =
	    wlr_server_decoration_manager_create(server.wl_display);
	if(!server_decoration_manager) {
		wlr_log(WLR_ERROR, "Unable to create the server decoration manager");
		ret = 1;
		goto end;
	}
	wlr_server_decoration_manager_set_default_mode(
	    server_decoration_manager, WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);

	viewporter = wlr_viewporter_create(server.wl_display);
	if (!viewporter) {
		wlr_log(WLR_ERROR, "Unable to create the viewporter interface");
		ret = 1;
		goto end;
	}

	presentation = wlr_presentation_create(server.wl_display, server.backend);
	if (!presentation) {
		wlr_log(WLR_ERROR, "Unable to create the presentation interface");
		ret = 1;
		goto end;
	}
	wlr_scene_set_presentation(server.scene, presentation);

	export_dmabuf_manager = wlr_export_dmabuf_manager_v1_create(server.wl_display);
	if (!export_dmabuf_manager) {
		wlr_log(WLR_ERROR, "Unable to create the export DMABUF manager");
		ret = 1;
		goto end;
	}


	screencopy_manager = wlr_screencopy_manager_v1_create(server.wl_display);
	if(!screencopy_manager) {
		wlr_log(WLR_ERROR, "Unable to create the screencopy manager");
		ret = 1;
		goto end;
	}

	output_manager = wlr_xdg_output_manager_v1_create(server.wl_display,
	                                                  server.output_layout);
	if(!output_manager) {
		wlr_log(WLR_ERROR, "Unable to create the output manager");
		ret = 1;
		goto end;
	}

	if(!wlr_primary_selection_v1_device_manager_create(server.wl_display)) {
		wlr_log(WLR_ERROR, "Unable to create the primary selection device manager");
		ret = 1;
		goto end;
	}

	gamma_control_manager =
	    wlr_gamma_control_manager_v1_create(server.wl_display);
	if(!gamma_control_manager) {
		wlr_log(WLR_ERROR, "Unable to create the gamma control manager");
		ret = 1;
		goto end;
	}

#if CG_HAS_XWAYLAND
	xwayland = wlr_xwayland_create(server.wl_display, compositor, true);
	if(!xwayland) {
		wlr_log(WLR_ERROR, "Cannot create XWayland server");
		ret = 1;
		goto end;
	}
	server.new_xwayland_surface.notify = handle_xwayland_surface_new;
	wl_signal_add(&xwayland->events.new_surface, &server.new_xwayland_surface);

	xcursor_manager = wlr_xcursor_manager_create(DEFAULT_XCURSOR, XCURSOR_SIZE);
	if(!xcursor_manager) {
		wlr_log(WLR_ERROR, "Cannot create XWayland XCursor manager");
		ret = 1;
		goto end;
	}

	if(setenv("DISPLAY", xwayland->display_name, true) < 0) {
		wlr_log_errno(WLR_ERROR, "Unable to set DISPLAY for XWayland.",
		              "Clients may not be able to connect");
	} else {
		wlr_log(WLR_DEBUG, "XWayland is running on display %s",
		        xwayland->display_name);
	}

	if(!wlr_xcursor_manager_load(xcursor_manager, 1)) {
		wlr_log(WLR_ERROR, "Cannot load XWayland XCursor theme");
	}
	struct wlr_xcursor *xcursor =
	    wlr_xcursor_manager_get_xcursor(xcursor_manager, DEFAULT_XCURSOR, 1);
	if(xcursor) {
		struct wlr_xcursor_image *image = xcursor->images[0];
		wlr_xwayland_set_cursor(xwayland, image->buffer, image->width * 4,
		                        image->width, image->height, image->hotspot_x,
		                        image->hotspot_y);
	}
#endif

	const char *socket = wl_display_add_socket_auto(server.wl_display);
	if(!socket) {
		wlr_log_errno(WLR_ERROR, "Unable to open Wayland socket");
		ret = 1;
		goto end;
	}

	if(!wlr_backend_start(backend)) {
		wlr_log(WLR_ERROR, "Unable to start the wlroots backend");
		ret = 1;
		goto end;
	}

	if(setenv("WAYLAND_DISPLAY", socket, true) < 0) {
		wlr_log_errno(WLR_ERROR, "Unable to set WAYLAND_DISPLAY.",
		              "Clients may not be able to connect");
	} else {
		wlr_log(WLR_DEBUG,
		        "Cagebreak " CG_VERSION " is running on Wayland display %s",
		        socket);
	}

#if CG_HAS_XWAYLAND
	wlr_xwayland_set_seat(xwayland, server.seat->seat);
#endif

	if(show_info) {
		char *msg = server_show_info(&server);
		if(msg != NULL) {
			fprintf(stderr, "%s", msg);
			free(msg);
		} else {
			wlr_log(WLR_ERROR, "Failed to get info on cagebreak setup\n");
		}
		exit(0);
	}

	{ // config_file should only be visible as long as it is valid
		char *config_file = get_config_file();
		if(config_file == NULL) {
			wlr_log(WLR_ERROR, "Unable to get path to config file");
			ret = 1;
			goto end;
		}
		int conf_ret = set_configuration(&server, config_file);

		// Configurtion file not found
		if(conf_ret == 1) {
			char *default_conf = "/etc/xdg/cagebreak/config";
			wlr_log(WLR_ERROR, "Loading default configuration file: \"%s\"",
			        default_conf);
			conf_ret = set_configuration(&server, default_conf);
		}

		if(conf_ret != 0) {
			free(config_file);
			ret = 1;
			goto end;
		}
		free(config_file);
	}

	{
		struct wl_list tmp_list;
		wl_list_init(&tmp_list);
		wl_list_insert_list(&tmp_list, &server.outputs);
		wl_list_init(&server.outputs);
		struct cg_output *output, *output_tmp;
		wl_list_for_each_safe(output, output_tmp, &tmp_list, link) {
			output_configure(&server, output);
		}
		server.curr_output =
		    wl_container_of(server.outputs.next, server.curr_output, link);
	}

	/* Place the cursor to the top left of the output layout. */
	wlr_cursor_warp(server.seat->cursor, NULL, 0, 0);

	wl_display_run(server.wl_display);

#if CG_HAS_XWAYLAND
	wlr_xwayland_destroy(xwayland);
	wlr_xcursor_manager_destroy(xcursor_manager);
#endif
	wl_display_destroy_clients(server.wl_display);

end:
#if CG_HAS_FANALYZE
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-double-free"
#endif
	for(unsigned int i = 0; server.modes[i] != NULL; ++i) {
		free(server.modes[i]);
	}
	free(server.modes);

	struct cg_output_config *output_config, *output_config_tmp;
	wl_list_for_each_safe(output_config, output_config_tmp,
	                      &server.output_config, link) {
		wl_list_remove(&output_config->link);
		free(output_config->output_name);
		free(output_config);
	}

	keybinding_list_free(server.keybindings);
	wl_event_source_remove(sigint_source);
	wl_event_source_remove(sigterm_source);
	wl_event_source_remove(sigalrm_source);

	seat_destroy(server.seat);
	/* This function is not null-safe, but we only ever get here
	   with a proper wl_display. */
	wl_display_destroy(server.wl_display);
	wlr_output_layout_destroy(server.output_layout);

	free(server.input);
	free(server.message_config.font);
	pango_cairo_font_map_set_default(NULL);
	cairo_debug_reset_static_data();
	FcFini();

	return ret;
}
#if CG_HAS_FANALYZE
#pragma GCC diagnostic pop
#endif
