/*
 * Cagebreak: A Wayland tiling compositor.
 *
 * Copyright (C) 2018-2020 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

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
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_output_damage.h>
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

#include "../idle_inhibit_v1.h"
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

static bool
parse_args(struct cg_server *server, int argc, char *argv[]) {
	server->output_transform = WL_OUTPUT_TRANSFORM_NORMAL;
#ifdef DEBUG
	server->debug_damage_tracking = false;
#endif
	return true;
}

void
cleanup() {
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
	wl_display_destroy(server.wl_display);
	wlr_output_layout_destroy(server.output_layout);
}

int
LLVMFuzzerInitialize(int *argc, char ***argv) {
	struct wl_event_loop *event_loop = NULL;
	struct wlr_backend *backend = NULL;
	struct wlr_renderer *renderer = NULL;
	struct wlr_compositor *compositor = NULL;
	struct wlr_data_device_manager *data_device_manager = NULL;
	struct wlr_server_decoration_manager *server_decoration_manager = NULL;
	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_manager = NULL;
	struct wlr_export_dmabuf_manager_v1 *export_dmabuf_manager = NULL;
	struct wlr_screencopy_manager_v1 *screencopy_manager = NULL;
	struct wlr_xdg_output_manager_v1 *output_manager = NULL;
	struct wlr_gamma_control_manager_v1 *gamma_control_manager = NULL;
	int ret = 0;

	if(!parse_args(&server, *argc, *argv)) {
		return 1;
	}

#ifdef DEBUG
	wlr_log_init(WLR_DEBUG, NULL);
#else
	wlr_log_init(WLR_ERROR, NULL);
#endif

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

	server.modes = malloc(4 * sizeof(char *));
	server.modes[0] = strdup("top");
	server.modes[1] = strdup("root");
	server.modes[2] = strdup("resize");
	server.modes[3] = NULL;

	server.nws = 1;
	server.message_timeout = 2;

	event_loop = wl_display_get_event_loop(server.wl_display);
	server.event_loop = event_loop;

	backend = wlr_multi_backend_create(server.wl_display);
	if(!backend) {
		wlr_log(WLR_ERROR, "Unable to create the wlroots multi backend");
		ret = 1;
		goto end;
	}
	server.backend = backend;

	struct wlr_backend *headless_backend =
	    wlr_headless_backend_create(server.wl_display);
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

	wl_list_init(&server.output_config);

	renderer = wlr_backend_get_renderer(backend);
	wlr_renderer_init_wl_display(renderer, server.wl_display);

	server.bg_color = malloc(4 * sizeof(float));
	server.bg_color[0] = 0;
	server.bg_color[1] = 0;
	server.bg_color[2] = 0;
	server.bg_color[3] = 1;
	wl_list_init(&server.outputs);
	wl_list_init(&server.disabled_outputs);

	server.output_layout = wlr_output_layout_create();
	if(!server.output_layout) {
		wlr_log(WLR_ERROR, "Unable to create output layout");
		ret = 1;
		goto end;
	}

	data_device_manager = wlr_data_device_manager_create(server.wl_display);
	if(!data_device_manager) {
		wlr_log(WLR_ERROR, "Unable to create the data device manager");
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

	gamma_control_manager =
	    wlr_gamma_control_manager_v1_create(server.wl_display);
	if(!gamma_control_manager) {
		wlr_log(WLR_ERROR, "Unable to create the gamma control manager");
		ret = 1;
		goto end;
	}

	compositor = wlr_compositor_create(server.wl_display, renderer);
	if(!compositor) {
		wlr_log(WLR_ERROR, "Unable to create the wlroots compositor");
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

	if(wlr_xcursor_manager_load(xcursor_manager, 1)) {
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

	/* Place the cursor to the topl left of the output layout. */
	wlr_cursor_warp(server.seat->cursor, NULL, 0, 0);
	atexit(cleanup);
	// server.wl_display->run = 1;
	return 0;
end:
	cleanup();
	return ret;
}

void
move_cursor(char *line, struct cg_server *server) {
	return; // TODO
	long del = 0;
	char *delstr = strtok_r(NULL, ";", &line);
	enum wlr_axis_orientation orientation =
	    (*(line++) == '0') ? WLR_AXIS_ORIENTATION_VERTICAL
	                       : WLR_AXIS_ORIENTATION_HORIZONTAL;
	if(delstr == NULL) {
		return;
	}
	del = strtol(delstr, NULL, 10);
	struct wlr_event_pointer_axis event = {.device = NULL,
	                                       .time_msec = 0,
	                                       .source = WLR_AXIS_SOURCE_WHEEL,
	                                       .orientation = orientation,
	                                       .delta = del * 15,
	                                       .delta_discrete = del};
	// TODO dispatch_cursor_axis(server->seat->cursor, &event);
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
add_input_device_callback(struct wlr_backend *backend, void *data) {
	enum wlr_input_device_type *type = data;
	wlr_headless_add_input_device(backend, *type);
}

void
create_input_device(char *line, struct cg_server *server) {
	enum wlr_input_device_type type;
	if(*line != '\0') {
		if(strncmp(line, "kbd", 3) == 0) {
			type = WLR_INPUT_DEVICE_KEYBOARD;
			wlr_multi_for_each_backend(server->backend,
			                           add_input_device_callback, &type);
		} else if(strncmp(line, "ptr", 3) == 0) {
			type = WLR_INPUT_DEVICE_POINTER;
			wlr_multi_for_each_backend(server->backend,
			                           add_input_device_callback, &type);
		} else if(strncmp(line, "tch", 3) == 0) {
			type = WLR_INPUT_DEVICE_TOUCH;
			wlr_multi_for_each_backend(server->backend,
			                           add_input_device_callback, &type);
		}
	}
}

void
destroy_input_device(char *line, struct cg_server *server) {
	long devn = 0;
	if(line[0] != '\0') {
		devn = strtol(line + 1, NULL, 10);
	}
	if(line != NULL) {
		if(strncmp(line, "k", 1) == 0) {
			if(wl_list_empty(&server->seat->keyboard_groups)) {
				return;
			}
			devn = devn % wl_list_length(&server->seat->keyboard_groups);
			struct cg_keyboard_group *group, *group_tmp;
			wl_list_for_each_safe(group, group_tmp,
			                      &server->seat->keyboard_groups, link) {
				if(devn == 0) {
					wl_list_remove(&group->link);
					wlr_keyboard_group_destroy(group->wlr_group);
					wl_event_source_remove(group->key_repeat_timer);
					free(group);
					break;
				}
				--devn;
			}
		} else if(strncmp(line, "p", 1) == 0) {
			if(wl_list_empty(&server->seat->pointers)) {
				return;
			}
			devn = devn % wl_list_length(&server->seat->pointers);
			struct cg_pointer *pointer, *pointer_tmp;
			wl_list_for_each_safe(pointer, pointer_tmp, &server->seat->pointers,
			                      link) {
				if(devn == 0) {
					pointer->destroy.notify(&pointer->destroy, NULL);
					break;
				}
				--devn;
			}
		} else if(strncmp(line, "t", 1) == 0) {
			if(wl_list_empty(&server->seat->touch)) {
				return;
			}
			devn = devn % wl_list_length(&server->seat->touch);
			struct cg_touch *touch, *touch_tmp;
			wl_list_for_each_safe(touch, touch_tmp, &server->seat->touch,
			                      link) {
				if(devn == 0) {
					touch->destroy.notify(&touch->destroy, NULL);
					break;
				}
				--devn;
			}
		}
	}
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
	it->damage_destroy.notify(&it->damage_destroy, NULL);
}
