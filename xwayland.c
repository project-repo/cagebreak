/*
 * Cagebreak: A Wayland tiling compositor.
 *
 * Copyright (C) 2020-2022 The Cagebreak Authors
 * Copyright (C) 2018-2020 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */
#include "config.h"

#include <X11/Xutil.h>
#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#if CG_HAS_XWAYLAND
#include <wlr/xwayland.h>
#endif

#include "output.h"
#include "server.h"
#include "view.h"
#include "workspace.h"
#include "xwayland.h"

struct cg_xwayland_view *
xwayland_view_from_view(struct cg_view *view) {
	return (struct cg_xwayland_view *)view;
}

static const struct cg_xwayland_view *
xwayland_view_from_const_view(const struct cg_view *view) {
	return (const struct cg_xwayland_view *)view;
}

bool
xwayland_view_should_manage(const struct cg_view *view) {
	const struct cg_xwayland_view *xwayland_view =
	    xwayland_view_from_const_view(view);
	struct wlr_xwayland_surface *xwayland_surface =
	    xwayland_view->xwayland_surface;
	return !xwayland_surface->override_redirect;
}

static pid_t
get_pid(const struct cg_view *view) {
	struct wlr_xwayland_surface *surf =
		wlr_xwayland_surface_from_wlr_surface(view->wlr_surface);
	return surf->pid;
}

static bool
is_primary(const struct cg_view *view) {
	const struct cg_xwayland_view *xwayland_view =
	    xwayland_view_from_const_view(view);
	struct wlr_xwayland_surface *parent =
	    xwayland_view->xwayland_surface->parent;
	return parent == NULL;
}

static void
activate(struct cg_view *view, bool activate) {
	struct cg_xwayland_view *xwayland_view = xwayland_view_from_view(view);
	wlr_xwayland_surface_activate(xwayland_view->xwayland_surface, activate);
	if(activate) {
		wlr_xwayland_surface_restack(xwayland_view->xwayland_surface, NULL,
		                             XCB_STACK_MODE_ABOVE);
	}
}

static void
close(struct cg_view *view) {
	struct cg_xwayland_view *xwayland_view = xwayland_view_from_view(view);
	wlr_xwayland_surface_close(xwayland_view->xwayland_surface);
}

static void
maximize(struct cg_view *view, int width, int height) {
	struct cg_xwayland_view *xwayland_view = xwayland_view_from_view(view);
	xcb_size_hints_t *hints =
	    xwayland_view->xwayland_surface->size_hints;

	if(hints != NULL && hints->flags & PMaxSize) {
		if(width > hints->max_width) {
			width = hints->max_width;
		}

		if(height > hints->max_height) {
			height = hints->max_height;
		}
	}

	struct wlr_box output_box;
	wlr_output_layout_get_box(view->server->output_layout, view->server->curr_output->wlr_output,&output_box);
	wlr_xwayland_surface_configure(xwayland_view->xwayland_surface,
	                               view->ox + output_box.x,
	                               view->oy + output_box.y, width, height);
	wlr_xwayland_surface_set_maximized(xwayland_view->xwayland_surface, true);
}

static void
destroy(struct cg_view *view) {
	struct cg_xwayland_view *xwayland_view = xwayland_view_from_view(view);
	free(xwayland_view);
}

static void
handle_xwayland_surface_request_fullscreen(struct wl_listener *listener,
                                           void *data) {
	struct cg_xwayland_view *xwayland_view =
	    wl_container_of(listener, xwayland_view, request_fullscreen);
	struct wlr_xwayland_surface *xwayland_surface =
	    xwayland_view->xwayland_surface;
	wlr_xwayland_surface_set_fullscreen(xwayland_view->xwayland_surface,
	                                    xwayland_surface->fullscreen);
}

static void
handle_xwayland_surface_unmap(struct wl_listener *listener, void *_data) {
	struct cg_xwayland_view *xwayland_view =
	    wl_container_of(listener, xwayland_view, unmap);
	struct cg_view *view = &xwayland_view->view;

	view_unmap(view);
}

static void
handle_xwayland_surface_map(struct wl_listener *listener, void *_data) {
	struct cg_xwayland_view *xwayland_view =
	    wl_container_of(listener, xwayland_view, map);
	struct cg_view *view = &xwayland_view->view;
	if(!xwayland_view_should_manage(view)) {
		struct wlr_output_layout *output_layout = view->server->output_layout;
		struct wlr_box output_box;
		wlr_output_layout_get_box(output_layout, view->server->curr_output->wlr_output,&output_box);
		view->ox = xwayland_view->xwayland_surface->x - output_box.x;
		view->oy = xwayland_view->xwayland_surface->y - output_box.y;
	}

	view_map(view, xwayland_view->xwayland_surface->surface,
	         view->server->curr_output
	             ->workspaces[view->server->curr_output->curr_workspace]);
}

static void
handle_xwayland_surface_destroy(struct wl_listener *listener, void *_data) {
	struct cg_xwayland_view *xwayland_view =
	    wl_container_of(listener, xwayland_view, destroy);
	struct cg_view *view = &xwayland_view->view;

	wl_list_remove(&xwayland_view->map.link);
	wl_list_remove(&xwayland_view->unmap.link);
	wl_list_remove(&xwayland_view->destroy.link);
	wl_list_remove(&xwayland_view->request_fullscreen.link);
	xwayland_view->xwayland_surface = NULL;

	view_destroy(view);
}

static const struct cg_view_impl xwayland_view_impl = {
    .get_pid = get_pid,
    .is_primary = is_primary,
    .activate = activate,
    .close = close,
    .maximize = maximize,
    .destroy = destroy,
};

void
handle_xwayland_surface_new(struct wl_listener *listener, void *data) {
	struct cg_server *server =
	    wl_container_of(listener, server, new_xwayland_surface);
	struct wlr_xwayland_surface *xwayland_surface = data;

	struct cg_xwayland_view *xwayland_view =
	    calloc(1, sizeof(struct cg_xwayland_view));
	if(!xwayland_view) {
		wlr_log(WLR_ERROR, "Failed to allocate XWayland view");
		return;
	}

	view_init(&xwayland_view->view, CG_XWAYLAND_VIEW, &xwayland_view_impl,
	          server);
	xwayland_view->xwayland_surface = xwayland_surface;

	xwayland_view->map.notify = handle_xwayland_surface_map;
	wl_signal_add(&xwayland_surface->events.map, &xwayland_view->map);
	xwayland_view->unmap.notify = handle_xwayland_surface_unmap;
	wl_signal_add(&xwayland_surface->events.unmap, &xwayland_view->unmap);
	xwayland_view->destroy.notify = handle_xwayland_surface_destroy;
	wl_signal_add(&xwayland_surface->events.destroy, &xwayland_view->destroy);
	xwayland_view->request_fullscreen.notify =
	    handle_xwayland_surface_request_fullscreen;
	wl_signal_add(&xwayland_surface->events.request_fullscreen,
	              &xwayland_view->request_fullscreen);
}
