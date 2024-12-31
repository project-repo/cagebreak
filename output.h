// Copyright 2020 - 2024, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#ifndef CG_OUTPUT_H
#define CG_OUTPUT_H

#include <wayland-server-core.h>
#include <wlr/util/box.h>

struct cg_server;
struct cg_view;
struct wlr_output;
struct wlr_surface;

enum output_role {
	OUTPUT_ROLE_PERIPHERAL,
	OUTPUT_ROLE_PERMANENT,
	OUTPUT_ROLE_DEFAULT
};

struct cg_output {
	struct cg_server *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_rect *bg;
	struct wlr_scene_output *scene_output;

	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener frame;
	struct cg_workspace **workspaces;
	struct wl_list messages;
	struct wlr_box layout_box;
	int curr_workspace;
	int priority;
	enum output_role role;
	bool destroyed;
	char *name;

	struct wl_list link; // cg_server::outputs
};

struct cg_output_priorities {
	char *ident;
	int priority;
	struct wl_list link;
};

enum output_status { OUTPUT_ENABLE, OUTPUT_DISABLE, OUTPUT_DEFAULT };

struct cg_output_config {
	enum output_status status;
	enum output_role role;
	struct wlr_box pos;
	char *output_name;
	float refresh_rate;
	float scale;
	int priority;
	int angle;           // enum wl_output_transform, -1 signifies "unspecified"
	struct wl_list link; // cg_server::output_config
};

typedef void (*cg_surface_iterator_func_t)(struct cg_output *output,
                                           struct wlr_surface *surface,
                                           struct wlr_box *box,
                                           void *user_data);
struct wlr_box
output_get_layout_box(struct cg_output *output);
void
handle_new_output(struct wl_listener *listener, void *data);
void
output_configure(struct cg_server *server, struct cg_output *output);
void
output_set_window_title(struct cg_output *output, const char *title);
void
output_make_workspace_fullscreen(struct cg_output *output, uint32_t ws);
int
output_get_num(const struct cg_output *output);
void
handle_output_gamma_control_set_gamma(struct wl_listener *listener, void *data);
void
output_insert(struct cg_server *server, struct cg_output *output);
#endif
