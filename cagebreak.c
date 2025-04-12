// Copyright 2020 - 2025, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#define _DEFAULT_SOURCE

#include "config.h"

#include <fontconfig/fontconfig.h>
#include <getopt.h>
#include <grp.h>
#include <pango.h>
#include <pango/pangocairo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#if CG_HAS_XWAYLAND
#include <wlr/types/wlr_xcursor_manager.h>
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
		// Drop ancillary groups
		gid_t gid = getgid();
		setgroups(1, &gid);
		// Set gid before uid
#ifdef linux
		if(setgid(getgid()) != 0 || setuid(getuid()) != 0) {
#else
		if(setregid(getgid(), getgid()) != 0 ||
		   setreuid(getuid(), getuid()) != 0) {
#endif
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
	        " -c <path>\t Load configuration file from <path>\n"
	        " -e\t\t Enable socket\n"
	        " -h\t\t Display this help message\n"
	        " -s\t\t Show information about the current setup and exit\n"
	        " -v\t\t Show the version number and exit\n"
	        " --bs\t\t \"bad security\": Enable features with potential "
	        "security implications (see man page)\n",
	        cage);
}

static bool
parse_args(struct cg_server *server, int argc, char *argv[],
           char **config_path) {
	int c, option_index;
	server->enable_socket = false;
	static struct option long_options[] = {{"bs", no_argument, 0, 0},
	                                       {0, 0, 0, 0}};
#ifndef __clang_analyzer__
	while((c = getopt_long(argc, argv, "c:hvse", long_options,
	                       &option_index)) != -1) {
		switch(c) {
		case 0:
			server->bs = true;
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
		case 'c':
			if(optarg != NULL) {
				*config_path = strdup(optarg);
				optarg = NULL;
			}
			break;
		case 'e':
			server->enable_socket = true;
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
#endif

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
	uint32_t line_length = 64;
	char *line = calloc(line_length, sizeof(char));
	for(unsigned int line_num = 1;; ++line_num) {
#ifndef __clang_analyzer__
		while((fgets(line + strlen(line), line_length - strlen(line),
		             config_file) != NULL) &&
		      (strcspn(line, "\n") == line_length - 1)) {
			line_length *= 2;
			line = reallocarray(line, line_length, sizeof(char));
			if(line == NULL) {
				wlr_log(WLR_ERROR, "Could not allocate buffer for reading "
				                   "configuration file.");
				fclose(config_file);
				return 2;
			}
		}
#endif
		if(strlen(line) == 0) {
			break;
		}
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
				free(line);
				return -1;
			}
		}
		memset(line, 0, line_length * sizeof(char));
	}
	free(line);
	fclose(config_file);
	return 0;
}

char *
get_config_file(char *udef_path) {
	if(udef_path != NULL) {
		return strdup(udef_path);
	}
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
	struct wl_event_source *sigpipe_source = NULL;
	struct wlr_backend *backend = NULL;
	struct wlr_compositor *compositor = NULL;
	struct wlr_subcompositor *subcompositor = NULL;
	struct wlr_data_device_manager *data_device_manager = NULL;
	struct wlr_server_decoration_manager *server_decoration_manager = NULL;
	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_manager = NULL;
	struct wlr_export_dmabuf_manager_v1 *export_dmabuf_manager = NULL;
	struct wlr_screencopy_manager_v1 *screencopy_manager = NULL;
	struct wlr_data_control_manager_v1 *data_control_manager = NULL;
	struct wlr_viewporter *viewporter = NULL;
	struct wlr_xdg_output_manager_v1 *output_manager = NULL;
	struct wlr_xdg_shell *xdg_shell = NULL;
	wl_list_init(&server.input_config);
	wl_list_init(&server.output_config);
	wl_list_init(&server.output_priorities);
	wl_list_init(&server.xdg_decorations);

	int ret = 0;
	server.bs = 0;
	server.message_config.enabled = true;

	char *config_path = NULL;
	if(!parse_args(&server, argc, argv, &config_path)) {
		goto end;
	}

#ifdef DEBUG
	wlr_log_init(WLR_DEBUG, NULL);
#else
	wlr_log_init(WLR_ERROR, NULL);
#endif

	server.modes = malloc(4 * sizeof(char *));
	server.modecursors = malloc(4 * sizeof(char *));
	if(!server.modes || !server.modecursors) {
		if(server.modes != NULL) {
			free(server.modes);
			server.modes = NULL;
		}
		if(server.modecursors != NULL) {
			free(server.modecursors);
			server.modecursors = NULL;
		}
		wlr_log(WLR_ERROR, "Error allocating mode arrays");
		goto end;
	}

	/* Wayland requires XDG_RUNTIME_DIR to be set. */
	if(!getenv("XDG_RUNTIME_DIR")) {
		wlr_log(WLR_INFO, "XDG_RUNTIME_DIR is not set in the environment");
	}

	server.wl_display = wl_display_create();
	if(!server.wl_display) {
		wlr_log(WLR_ERROR, "Cannot allocate a Wayland display");
		free(server.modes);
		server.modes = NULL;
		server.modecursors = NULL;
		goto end;
	}

	server.xcursor_size = XCURSOR_SIZE;
	const char *env_cursor_size = getenv("XCURSOR_SIZE");
	if(env_cursor_size && strlen(env_cursor_size) > 0) {
		errno = 0;
		char *end;
		unsigned size = strtoul(env_cursor_size, &end, 10);
		if(!*end && errno == 0) {
			server.xcursor_size = size;
		}
	}

	server.running = true;

	server.modes[0] = strdup("top");
	server.modes[1] = strdup("root");
	server.modes[2] = strdup("resize");
	server.modes[3] = NULL;

	server.modecursors[0] = NULL;
	server.modecursors[1] = strdup("cell");
	server.modecursors[2] = NULL;
	server.modecursors[3] = NULL;
	if(server.modes[0] == NULL || server.modes[1] == NULL ||
	   server.modes[2] == NULL || server.modecursors[1] == NULL) {
		wlr_log(WLR_ERROR, "Error allocating default modes");
		goto end;
	}

	server.nws = 1;
	server.views_curr_id = 1;
	server.tiles_curr_id = 1;
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
	server.message_config.anchor = CG_MESSAGE_TOP_RIGHT;

	event_loop = wl_display_get_event_loop(server.wl_display);
	sigint_source =
	    wl_event_loop_add_signal(event_loop, SIGINT, handle_signal, &server);
	sigterm_source =
	    wl_event_loop_add_signal(event_loop, SIGTERM, handle_signal, &server);
	sigalrm_source =
	    wl_event_loop_add_signal(event_loop, SIGALRM, handle_signal, &server);
	sigpipe_source =
	    wl_event_loop_add_signal(event_loop, SIGPIPE, handle_signal, &server);
	server.event_loop = event_loop;

	backend = wlr_backend_autocreate(event_loop, &server.session);
	server.headless_backend = wlr_headless_backend_create(event_loop);
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
	if(!server.renderer) {
		wlr_log(WLR_ERROR, "Unable to create the wlroots renderer");
		ret = 1;
		goto end;
	}

	server.allocator =
	    wlr_allocator_autocreate(server.backend, server.renderer);
	if(!server.allocator) {
		wlr_log(WLR_ERROR, "Unable to create the wlroots allocator");
		ret = 1;
		goto end;
	}

	wlr_renderer_init_wl_display(server.renderer, server.wl_display);

	server.bg_color = (float[4]){0, 0, 0, 1};
	wl_list_init(&server.outputs);
	wl_list_init(&server.disabled_outputs);

	server.output_layout = wlr_output_layout_create(server.wl_display);
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
	if(!server.scene) {
		wlr_log(WLR_ERROR, "Unable to create scene");
		ret = 1;
		goto end;
	}
	server.scene_output_layout =
	    wlr_scene_attach_output_layout(server.scene, server.output_layout);

	compositor = wlr_compositor_create(server.wl_display, 6, server.renderer);
	if(!compositor) {
		wlr_log(WLR_ERROR, "Unable to create the wlroots compositor");
		ret = 1;
		goto end;
	}

	subcompositor = wlr_subcompositor_create(server.wl_display);
	if(!subcompositor) {
		wlr_log(WLR_ERROR, "Unable to create the wlroots subcompositor");
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

	server.seat = seat_create(&server);
	if(!server.seat) {
		wlr_log(WLR_ERROR, "Unable to create the seat");
		ret = 1;
		goto end;
	}

	server.idle_inhibit_v1 = wlr_idle_inhibit_v1_create(server.wl_display);
	server.idle = wlr_idle_notifier_v1_create(server.wl_display);
	if(!server.idle_inhibit_v1) {
		wlr_log(WLR_ERROR, "Cannot create the idle inhibitor");
		ret = 1;
		goto end;
	}
	server.new_idle_inhibitor_v1.notify = handle_idle_inhibitor_v1_new;
	wl_signal_add(&server.idle_inhibit_v1->events.new_inhibitor,
	              &server.new_idle_inhibitor_v1);
	wl_list_init(&server.inhibitors);

	xdg_shell = wlr_xdg_shell_create(server.wl_display, 5);
	if(!xdg_shell) {
		wlr_log(WLR_ERROR, "Unable to create the XDG shell interface");
		ret = 1;
		goto end;
	}
	server.new_xdg_shell_toplevel.notify = handle_xdg_shell_toplevel_new;
	wl_signal_add(&xdg_shell->events.new_toplevel,
	              &server.new_xdg_shell_toplevel);

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
	if(!viewporter) {
		wlr_log(WLR_ERROR, "Unable to create the viewporter interface");
		ret = 1;
		goto end;
	}

	export_dmabuf_manager =
	    wlr_export_dmabuf_manager_v1_create(server.wl_display);
	if(!export_dmabuf_manager) {
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
		wlr_log(WLR_ERROR,
		        "Unable to create the primary selection device manager");
		ret = 1;
		goto end;
	}

	server.gamma_control =
	    wlr_gamma_control_manager_v1_create(server.wl_display);
	if(!server.gamma_control) {
		wlr_log(WLR_ERROR, "Unable to create the gamma control manager");
		ret = 1;
		goto end;
	}
	server.gamma_control_set_gamma.notify =
	    handle_output_gamma_control_set_gamma;
	wl_signal_add(&server.gamma_control->events.set_gamma,
	              &server.gamma_control_set_gamma);

#if CG_HAS_XWAYLAND
	server.xwayland = wlr_xwayland_create(server.wl_display, compositor, true);
	if(!server.xwayland) {
		wlr_log(WLR_ERROR, "Cannot create XWayland server");
		ret = 1;
		goto end;
	}
	server.new_xwayland_surface.notify = handle_xwayland_surface_new;
	wl_signal_add(&server.xwayland->events.new_surface,
	              &server.new_xwayland_surface);

	if(setenv("DISPLAY", server.xwayland->display_name, true) < 0) {
		wlr_log_errno(WLR_ERROR, "Unable to set DISPLAY for XWayland. Clients "
		                         "may not be able to connect");
	} else {
		wlr_log(WLR_DEBUG, "XWayland is running on display %s",
		        server.xwayland->display_name);
	}

	struct wlr_xcursor *xcursor = wlr_xcursor_manager_get_xcursor(
	    server.seat->xcursor_manager, DEFAULT_XCURSOR, 1);

	if(xcursor) {
		struct wlr_xcursor_image *image = xcursor->images[0];
		wlr_xwayland_set_cursor(server.xwayland, image->buffer,
		                        image->width * 4, image->width, image->height,
		                        image->hotspot_x, image->hotspot_y);
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
		wlr_log_errno(WLR_ERROR, "Unable to set WAYLAND_DISPLAY. Clients may "
		                         "not be able to connect");
	} else {
		fprintf(stdout,
		        "Cagebreak " CG_VERSION " is running on Wayland display %s\n",
		        socket);
	}

#if CG_HAS_XWAYLAND
	wlr_xwayland_set_seat(server.xwayland, server.seat->seat);
#endif

	if(show_info) {
		char *msg = server_show_info(&server);
		if(msg != NULL) {
			fprintf(stdout, "%s", msg);
			free(msg);
		} else {
			wlr_log(WLR_ERROR, "Failed to get info on cagebreak setup\n");
		}
		exit(0);
	}

	{ // config_file should only be visible as long as it is valid
		int conf_ret = 1;
		char *config_file = get_config_file(config_path);
		if(config_file == NULL) {
			wlr_log(WLR_ERROR, "Unable to get path to config file");
			ret = 1;
			goto end;
		} else {
			conf_ret = set_configuration(&server, config_file);
			free(config_file);
		}

		// Configuration file not found
		if(conf_ret == 1) {
			char *default_conf = "/etc/xdg/cagebreak/config";
			wlr_log(WLR_INFO, "Loading default configuration file: \"%s\"",
			        default_conf);
			conf_ret = set_configuration(&server, default_conf);
		}

		if(conf_ret != 0 || !server.running) {
			ret = 1;
			goto end;
		}
	}

	{
		struct wl_list tmp_list;
		wl_list_init(&tmp_list);
		wl_list_insert_list(&tmp_list, &server.outputs);
		wl_list_init(&server.outputs);
		struct cg_output *output, *output_tmp;
		wl_list_for_each_safe(output, output_tmp, &tmp_list, link) {
			wl_list_remove(&output->link);
			output_insert(&server, output);
			output_configure(&server, output);
		}
		server.curr_output =
		    wl_container_of(server.outputs.next, server.curr_output, link);
	}

	/* Place the cursor to the top left of the output layout. */
	wlr_cursor_warp(server.seat->cursor, NULL, 0, 0);

	wl_display_run(server.wl_display);

#if CG_HAS_XWAYLAND
	if(server.xwayland != NULL) {
		wlr_xwayland_destroy(server.xwayland);
	}
#endif

	wl_display_destroy_clients(server.wl_display);

end:
#ifndef __clang_analyzer__
	if(server.modecursors) {
		for(unsigned int i = 0; server.modes[i] != NULL; ++i) {
			free(server.modecursors[i]);
		}
		free(server.modecursors);
	}
#endif

	if(server.modes) {
		for(unsigned int i = 0; server.modes[i] != NULL; ++i) {
			free(server.modes[i]);
		}
		free(server.modes);
	}

	if(config_path) {
		free(config_path);
	}

	struct cg_output_config *output_config, *output_config_tmp;
	wl_list_for_each_safe(output_config, output_config_tmp,
	                      &server.output_config, link) {
		wl_list_remove(&output_config->link);
		free(output_config->output_name);
		free(output_config);
	}

	struct cg_input_config *input_config, *input_config_tmp;
	wl_list_for_each_safe(input_config, input_config_tmp, &server.input_config,
	                      link) {
		wl_list_remove(&input_config->link);
		if(input_config->identifier != NULL) {
			free(input_config->identifier);
		}
		free(input_config);
	}

	if(server.keybindings != NULL) {
		keybinding_list_free(server.keybindings);
	}

	if(server.message_config.font != NULL) {
		free(server.message_config.font);
	}
	server.running = false;
	if(server.seat != NULL) {
		seat_destroy(server.seat);
	}

	if(sigint_source != NULL) {
		wl_event_source_remove(sigint_source);
		wl_event_source_remove(sigterm_source);
		wl_event_source_remove(sigalrm_source);
		wl_event_source_remove(sigpipe_source);
	}

	/* This function is not null-safe, but we only ever get here
	   with a proper wl_display. */
	if(server.wl_display != NULL) {
		wl_display_destroy(server.wl_display);
	}

	if(server.allocator != NULL) {
		wlr_allocator_destroy(server.allocator);
	}
	if(server.renderer != NULL) {
		wlr_renderer_destroy(server.renderer);
	}
	if(server.scene != NULL) {
		wlr_scene_node_destroy(&server.scene->tree.node);
	}

	if(server.input != NULL) {
		free(server.input);
	}
	pango_cairo_font_map_set_default(NULL);
	cairo_debug_reset_static_data();
	FcFini();

	return ret;
}
