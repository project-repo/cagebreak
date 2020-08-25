/*
 * Cagebreak: A Wayland tiling compositor.
 *
 * Copyright (C) 2020 The Cagebreak Authors
 * Copyright (C) 2018-2020 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#include <X11/Xutil.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_box.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/log.h>
#include <wlr/xwayland.h>
#include <xcb/xproto.h>

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

static char *
get_title(const struct cg_view *view) {
	const struct cg_xwayland_view *xwayland_view =
	    xwayland_view_from_const_view(view);
	return xwayland_view->xwayland_surface->title;
}

static void
get_geometry(const struct cg_view *view, int *width_out, int *height_out) {
	const struct cg_xwayland_view *xwayland_view =
	    xwayland_view_from_const_view(view);
	*width_out = xwayland_view->xwayland_surface->width;
	*height_out = xwayland_view->xwayland_surface->height;
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
}

static void
close(struct cg_view *view) {
	struct cg_xwayland_view *xwayland_view = xwayland_view_from_view(view);
	wlr_xwayland_surface_close(xwayland_view->xwayland_surface);
}

static void
maximize(struct cg_view *view, int width, int height) {
	struct cg_xwayland_view *xwayland_view = xwayland_view_from_view(view);
	struct cg_output *output = view->workspace->output;

	struct wlr_box *box =
	    wlr_output_layout_get_box(view->workspace->server->output_layout,
	                              view->workspace->output->wlr_output);
	struct wlr_xwayland_surface_size_hints *hints =
	    xwayland_view->xwayland_surface->size_hints;

	if(hints != NULL && hints->flags & PMaxSize) {
		if(width > hints->max_width) {
			wlr_output_damage_add_box(output->damage,
			                          &(struct wlr_box){.x = view->ox + box->x,
			                                            .y = view->oy + box->y,
			                                            .width = width,
			                                            .height = height});
			width = hints->max_width;
		}

		if(height > hints->max_height) {
			wlr_output_damage_add_box(output->damage,
			                          &(struct wlr_box){.x = view->ox + box->x,
			                                            .y = view->oy + box->y,
			                                            .width = width,
			                                            .height = height});
			height = hints->max_height;
		}
	}

	wlr_xwayland_surface_configure(xwayland_view->xwayland_surface,
	                               view->ox + box->x, view->oy + box->y, width,
	                               height);
	wlr_xwayland_surface_set_maximized(xwayland_view->xwayland_surface, true);
}

static void
destroy(struct cg_view *view) {
	struct cg_xwayland_view *xwayland_view = xwayland_view_from_view(view);
	free(xwayland_view);
}

static void
for_each_surface(struct cg_view *view, wlr_surface_iterator_func_t iterator,
                 void *data) {
	wlr_surface_for_each_surface(view->wlr_surface, iterator, data);
}

static struct wlr_surface *
wlr_surface_at(const struct cg_view *view, double sx, double sy, double *sub_x,
               double *sub_y) {
	return wlr_surface_surface_at(view->wlr_surface, sx, sy, sub_x, sub_y);
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
handle_xwayland_surface_commit(struct wl_listener *listener, void *_data) {
	struct cg_xwayland_view *xwayland_view =
	    wl_container_of(listener, xwayland_view, commit);
	struct cg_view *view = &xwayland_view->view;
	view_damage_part(view);
}

static void
handle_xwayland_surface_unmap(struct wl_listener *listener, void *_data) {
	struct cg_xwayland_view *xwayland_view =
	    wl_container_of(listener, xwayland_view, unmap);
	struct cg_view *view = &xwayland_view->view;

	wl_list_remove(&xwayland_view->commit.link);

	view_unmap(view);
}

static void
handle_xwayland_surface_map(struct wl_listener *listener, void *_data) {
	struct cg_xwayland_view *xwayland_view =
	    wl_container_of(listener, xwayland_view, map);
	struct cg_view *view = &xwayland_view->view;

	if(!xwayland_view_should_manage(view)) {
		view->ox = xwayland_view->xwayland_surface->x;
		view->oy = xwayland_view->xwayland_surface->y;
	}

	xwayland_view->commit.notify = handle_xwayland_surface_commit;
	wl_signal_add(&xwayland_view->xwayland_surface->surface->events.commit,
	              &xwayland_view->commit);

	view_map(view, xwayland_view->xwayland_surface->surface);

	view_damage_whole(view);
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
    .get_title = get_title,
    .get_geometry = get_geometry,
    .is_primary = is_primary,
    .activate = activate,
    .close = close,
    .maximize = maximize,
    .destroy = destroy,
    .for_each_surface = for_each_surface,
    /* XWayland doesn't have a separate popup iterator. */
    .for_each_popup = NULL,
    .wlr_surface_at = wlr_surface_at,
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

	view_init(
	    &xwayland_view->view,
	    server->curr_output->workspaces[server->curr_output->curr_workspace],
	    CG_XWAYLAND_VIEW, &xwayland_view_impl);
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
