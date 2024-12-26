// Copyright 2020 - 2024, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#define _POSIX_C_SOURCE 200812L

#include "config.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
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
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_viewporter.h>
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

#include "../idle_inhibit_v1.h"
#include "../input_manager.h"
#include "../keybinding.h"
#include "../output.h"
#include "../parse.h"
#include "../seat.h"
#include "../server.h"
#include "../xdg_shell.h"
#if CG_HAS_XWAYLAND
#include "../xwayland.h"
#endif

#include "fuzz-lib.h"

struct cg_server server;
struct wlr_xdg_shell *xdg_shell;

struct wlr_xwayland *xwayland;
#if CG_HAS_XWAYLAND
struct wlr_xcursor_manager *xcursor_manager;
#endif

static bool
drop_permissions(void) {
	if(getuid() != geteuid() || getgid() != getegid()) {
		if(setuid(getuid()) != 0 || setgid(getgid()) != 0) {
			wlr_log(WLR_ERROR, "Unable to drop root, refusing to start");
			return false;
		}
	}

	if(setuid(0) != -1) {
		wlr_log(WLR_ERROR, "Unable to drop root (we shouldn't be able to "
		                   "restore it after setuid), refusing to start");
		return false;
	}

	return true;
}

void
cleanup(void) {
	server.running = false;
#if CG_HAS_XWAYLAND
	if(xwayland != NULL) {
		wlr_xwayland_destroy(xwayland);
	}
	if(xcursor_manager != NULL) {
		wlr_xcursor_manager_destroy(xcursor_manager);
	}
#endif
	wl_display_destroy_clients(server.wl_display);

	for(unsigned int i = 0; server.modes[i] != NULL; ++i) {
		free(server.modes[i]);
	}
	free(server.modes);

	keybinding_list_free(server.keybindings);

	seat_destroy(server.seat);
	/* This function is not null-safe, but we only ever get here
	   with a proper wl_display. */
	wlr_output_layout_destroy(server.output_layout);
}

int
LLVMFuzzerInitialize(int *argc, char ***argv) {
	struct wlr_backend *backend = NULL;
	struct wlr_compositor *compositor = NULL;
	struct wlr_data_device_manager *data_device_manager = NULL;
	struct wlr_server_decoration_manager *server_decoration_manager = NULL;
	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_manager = NULL;
	struct wlr_export_dmabuf_manager_v1 *export_dmabuf_manager = NULL;
	struct wlr_screencopy_manager_v1 *screencopy_manager = NULL;
	struct wlr_data_control_manager_v1 *data_control_manager = NULL;
	struct wlr_viewporter *viewporter = NULL;
	struct wlr_xdg_output_manager_v1 *output_manager = NULL;
	struct wlr_gamma_control_manager_v1 *gamma_control_manager = NULL;
	struct wlr_xdg_shell *xdg_shell = NULL;
#if CG_HAS_XWAYLAND
	struct wlr_xwayland *xwayland = NULL;
#endif
	wl_list_init(&server.input_config);
	wl_list_init(&server.output_config);
	wl_list_init(&server.output_priorities);
	wl_list_init(&server.outputs);
	wl_list_init(&server.disabled_outputs);

	int ret = 0;

#ifdef DEBUG
	wlr_log_init(WLR_DEBUG, NULL);
#else
	wlr_log_init(WLR_ERROR, NULL);
#endif

	server.modes = malloc(4 * sizeof(char *));
	server.modecursors = malloc(4 * sizeof(char *));

	if(!server.modes || ! server.modecursors) {
		wlr_log(WLR_ERROR, "Error allocating mode array");
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
		free(server.modecursors);
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

	server.event_loop = wl_display_get_event_loop(server.wl_display);
	backend = wlr_multi_backend_create(server.event_loop);
	if(!backend) {
		wlr_log(WLR_ERROR, "Unable to create the wlroots multi backend");
		ret = 1;
		goto end;
	}
	server.backend = backend;

	struct wlr_backend *headless_backend =
	    wlr_headless_backend_create(server.event_loop);
	if(!headless_backend) {
		wlr_log(WLR_ERROR, "Unable to create the wlroots headless backend");
		ret = 1;
		goto end;
	}
	wlr_headless_add_output(headless_backend, 600, 300);

	if(!wlr_multi_backend_add(backend, headless_backend)) {
		wlr_log(WLR_ERROR,
		        "Unable to insert headless backend into multi backend");
		ret = 1;
		goto end;
	};

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

	server.bg_color = calloc(4, sizeof(float *));
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

	server.idle = wlr_idle_notifier_v1_create(server.wl_display);
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

	xdg_shell = wlr_xdg_shell_create(server.wl_display, 3);
	if(!xdg_shell) {
		wlr_log(WLR_ERROR, "Unable to create the XDG shell interface");
		ret = 1;
		goto end;
	}
	server.new_xdg_shell_toplevel.notify = handle_xdg_shell_toplevel_new;
	wl_signal_add(&xdg_shell->events.new_surface,
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

	if(setenv("DISPLAY", xwayland->display_name, true) < 0) {
		wlr_log_errno(WLR_ERROR, "Unable to set DISPLAY for XWayland.",
		              "Clients may not be able to connect");
	} else {
		wlr_log(WLR_DEBUG, "XWayland is running on display %s",
		        xwayland->display_name);
	}

	struct wlr_xcursor *xcursor = wlr_xcursor_manager_get_xcursor(
	    server.seat->xcursor_manager, DEFAULT_XCURSOR, 1);

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
		fprintf(stderr,
		        "Cagebreak " CG_VERSION " is running on Wayland display %s\n",
		        socket);
	}

#if CG_HAS_XWAYLAND
	wlr_xwayland_set_seat(xwayland, server.seat->seat);
#endif

	/* Place the cursor to the top left of the output layout. */
	wlr_cursor_warp(server.seat->cursor, NULL, 0, 0);

	if(!drop_permissions()) {
		ret = 1;
		goto end;
	}

	/* Place the cursor to the topl left of the output layout. */
	wlr_cursor_warp(server.seat->cursor, NULL, 0, 0);
	atexit(cleanup);
	return 0;
end:
	cleanup();
	return ret;
}

void
add_output_callback(struct wlr_backend *backend, void *data) {
	long *dims = data;
	wlr_headless_add_output(backend, dims[0], dims[1]);
}

void
create_output(char *line, struct cg_server *server) {
	char *widthstr = strtok_r(NULL, ";", &line);
	long dims[2] = {600, 200};
	if(widthstr != NULL) {
		dims[0] = strtol(widthstr, NULL, 10);
		if(line[0] != '\0') {
			++line;
		}
	}
	char *heightstr = strtok_r(NULL, ";", &line);
	if(heightstr != NULL) {
		dims[1] = strtol(heightstr, NULL, 10);
	}
	long max_dim = 10000;
	if(dims[0] > max_dim || dims[0] <= 0) {
		wlr_log(WLR_ERROR, "height out of range.");
		return;
	} else if(dims[1] > max_dim || dims[1] <= 0) {
		wlr_log(WLR_ERROR, "width out of range.");
		return;
	}
	wlr_multi_for_each_backend(server->backend, add_output_callback, dims);
}

void
destroy_output(char *line, struct cg_server *server) {
	if(wl_list_length(&server->outputs) < 2) {
		return;
	}
	char *outpnstr = strtok_r(NULL, ";", &line);
	long outpn = 0;
	if(outpnstr != NULL) {
		outpn = strtol(outpnstr, NULL, 10);
	}
	outpn = outpn % wl_list_length(&server->outputs);
	struct cg_output *it;
	wl_list_for_each(it, &server->outputs, link) {
		if(outpn == 0) {
			break;
		} else {
			--outpn;
		}
	}
}
