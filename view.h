// Copyright 2020 - 2024, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#ifndef CG_VIEW_H
#define CG_VIEW_H

#include "config.h"

#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_compositor.h>

struct cg_server;
struct wlr_box;

enum cg_view_type {
	CG_XDG_SHELL_VIEW,
#if CG_HAS_XWAYLAND
	CG_XWAYLAND_VIEW,
#endif
};

struct cg_view {
	struct cg_workspace *workspace;
	struct cg_server *server;
	struct wl_list link; // server::views
	struct wlr_surface *wlr_surface;
	struct cg_tile *tile;
	struct wlr_scene_tree *scene_tree;

	struct wl_listener destroy;
	struct wl_listener unmap;
	struct wl_listener map;

	/* The view has a position in output coordinates. */
	int ox, oy;

	enum cg_view_type type;
	const struct cg_view_impl *impl;

	uint32_t id;
};

struct cg_view_impl {
	pid_t (*get_pid)(const struct cg_view *view);
	char *(*get_title)(const struct cg_view *view);
	bool (*is_primary)(const struct cg_view *view);
	void (*activate)(struct cg_view *view, bool activate);
	void (*close)(struct cg_view *view);
	void (*maximize)(struct cg_view *view, int width, int height);
	void (*destroy)(struct cg_view *view);
};

struct cg_tile *
view_get_tile(const struct cg_view *view);
bool
view_is_primary(const struct cg_view *view);
bool
view_is_visible(const struct cg_view *view);
void
view_activate(struct cg_view *view, bool activate);
void
view_unmap(struct cg_view *view);
void
view_maximize(struct cg_view *view, struct cg_tile *tile);
void
view_map(struct cg_view *view, struct wlr_surface *surface,
         struct cg_workspace *ws);
void
view_destroy(struct cg_view *view);
void
view_init(struct cg_view *view, enum cg_view_type type,
          const struct cg_view_impl *impl, struct cg_server *server);
struct cg_view *
view_get_prev_view(struct cg_view *view);

#endif
