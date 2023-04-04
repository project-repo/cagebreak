// Copyright 2020 - 2023, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#ifndef CG_XWAYLAND_H
#define CG_XWAYLAND_H

#include "view.h"

struct cg_xwayland_view {
	struct cg_view view;
	struct wlr_xwayland_surface *xwayland_surface;

	struct wl_listener destroy;
	struct wl_listener unmap;
	struct wl_listener map;
	struct wl_listener request_fullscreen;
};

struct cg_xwayland_view *
xwayland_view_from_view(struct cg_view *view);
bool
xwayland_view_should_manage(const struct cg_view *view);
void
handle_xwayland_surface_new(struct wl_listener *listener, void *data);

#endif
