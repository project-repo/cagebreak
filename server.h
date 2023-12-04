// Copyright 2020 - 2023, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#ifndef CG_SERVER_H
#define CG_SERVER_H

#include "config.h"
#include "ipc_server.h"
#include "message.h"

#include <wayland-server-core.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>

struct cg_seat;
struct cg_output;
struct keybinding_list;
struct wlr_output_layout;
struct wlr_idle_inhibit_manager_v1;
struct cg_output_config;
struct cg_input_manager;

struct cg_server {
	struct wl_display *wl_display;
	struct wl_event_loop *event_loop;

	struct cg_seat *seat;
	struct cg_input_manager *input;
	struct wlr_backend *backend;
	struct wlr_idle_notifier_v1 *idle;
	struct wlr_idle_inhibit_manager_v1 *idle_inhibit_v1;
	struct wl_listener new_idle_inhibitor_v1;
	struct wl_list inhibitors;

	struct wlr_output_layout *output_layout;
	struct wlr_scene_output_layout *scene_output_layout;
	struct wl_list disabled_outputs;
	struct wl_list outputs;
	struct cg_output *curr_output;
	struct wl_listener new_output;
	struct wl_list output_priorities;
	struct wlr_session *session;

	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;

	struct wl_listener xdg_toplevel_decoration;
	struct wl_listener new_xdg_shell_surface;
#if CG_HAS_XWAYLAND
	struct wl_listener new_xwayland_surface;
	struct wlr_xwayland *xwayland;
#endif

	struct keybinding_list *keybindings;
	struct wl_list output_config;
	struct wl_list input_config;
	struct cg_message_config message_config;

	struct cg_ipc_handle ipc;

	bool enable_socket;
	bool bs;
	bool running;
	char **modes;
	uint16_t nws;
	float *bg_color;
	uint32_t views_curr_id;
	uint32_t tiles_curr_id;
	uint32_t xcursor_size;
};

void
display_terminate(struct cg_server *server);
int
get_mode_index_from_name(char *const *modes, const char *mode_name);
char *
server_show_info(struct cg_server *server);

#endif
