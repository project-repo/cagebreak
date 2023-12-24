// Copyright 2020 - 2023, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>

#include "output.h"
#include "server.h"
#include "view.h"
#include "workspace.h"
#include "xdg_shell.h"

static struct cg_view *
popup_get_view(struct wlr_xdg_popup *popup) {
	while(true) {
		if(popup->parent == NULL) {
			return NULL;
		}
		struct wlr_xdg_surface *xdg_surface =
		    wlr_xdg_surface_try_from_wlr_surface(popup->parent);
		if(xdg_surface == NULL) {
			return NULL;
		}
		switch(xdg_surface->role) {
		case WLR_XDG_SURFACE_ROLE_TOPLEVEL:
			return xdg_surface->data;
		case WLR_XDG_SURFACE_ROLE_POPUP:
			popup = xdg_surface->popup;
			break;
		case WLR_XDG_SURFACE_ROLE_NONE:
			return NULL;
		}
	}
}

static void
xdg_decoration_handle_destroy(struct wl_listener *listener, void *data) {
	struct cg_xdg_decoration *xdg_decoration =
	    wl_container_of(listener, xdg_decoration, destroy);

	wl_list_remove(&xdg_decoration->destroy.link);
	wl_list_remove(&xdg_decoration->request_mode.link);
	free(xdg_decoration);
}

#if CG_HAS_FANALYZE
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
static void
xdg_decoration_handle_request_mode(struct wl_listener *listener, void *_data) {
	struct cg_xdg_decoration *xdg_decoration =
	    wl_container_of(listener, xdg_decoration, request_mode);
	enum wlr_xdg_toplevel_decoration_v1_mode mode;

	mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
	wlr_xdg_toplevel_decoration_v1_set_mode(xdg_decoration->wlr_decoration,
	                                        mode);
}
#if CG_HAS_FANALYZE
#pragma GCC diagnostic pop
#endif

static void
popup_unconstrain(struct cg_view *view, struct wlr_xdg_popup *popup) {
	struct wlr_box *popup_box = &popup->current.geometry;

	struct wlr_output_layout *output_layout = view->server->output_layout;
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
	    output_layout,
	    output_get_layout_box(view->workspace->output).x + view->ox +
	        popup_box->x,
	    output_get_layout_box(view->workspace->output).y + view->oy +
	        popup_box->y);
	struct wlr_box output_box;
	wlr_output_layout_get_box(output_layout, wlr_output, &output_box);

	struct wlr_box output_toplevel_box = {.x = -view->ox,
	                                      .y = -view->oy,
	                                      .width = output_box.width,
	                                      .height = output_box.height};

	wlr_xdg_popup_unconstrain_from_box(popup, &output_toplevel_box);
}

static struct cg_xdg_shell_view *
xdg_shell_view_from_view(struct cg_view *view) {
	return (struct cg_xdg_shell_view *)view;
}

static const struct cg_xdg_shell_view *
xdg_shell_view_from_const_view(const struct cg_view *view) {
	return (const struct cg_xdg_shell_view *)view;
}

static pid_t
get_pid(const struct cg_view *view) {
	pid_t pid;
	struct wl_client *client =
	    wl_resource_get_client(view->wlr_surface->resource);
	wl_client_get_credentials(client, &pid, NULL, NULL);
	return pid;
}

static char *
get_title(const struct cg_view *view) {
	const struct cg_xdg_shell_view *xdg_shell_view =
	    xdg_shell_view_from_const_view(view);
	return xdg_shell_view->xdg_surface->toplevel->title;
}

static bool
is_primary(const struct cg_view *view) {
	const struct cg_xdg_shell_view *xdg_shell_view =
	    xdg_shell_view_from_const_view(view);
	struct wlr_xdg_toplevel *parent =
	    xdg_shell_view->xdg_surface->toplevel->parent;
	return parent == NULL;
}

static void
activate(struct cg_view *view, bool activate) {
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	wlr_xdg_toplevel_set_activated(xdg_shell_view->xdg_surface->toplevel,
	                               activate);
}

static void
close(struct cg_view *view) {
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	struct wlr_xdg_surface *surface = xdg_shell_view->xdg_surface;
	if(surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		wlr_xdg_toplevel_send_close(surface->toplevel);
	}
}

static void
maximize(struct cg_view *view, int width, int height) {
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	wlr_xdg_toplevel_set_size(xdg_shell_view->xdg_surface->toplevel, width,
	                          height);
	enum wlr_edges edges =
	    WLR_EDGE_LEFT | WLR_EDGE_RIGHT | WLR_EDGE_TOP | WLR_EDGE_BOTTOM;
	wlr_xdg_toplevel_set_tiled(xdg_shell_view->xdg_surface->toplevel, edges);
}

static void
destroy(struct cg_view *view) {
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	free(xdg_shell_view);
}

static void
handle_xdg_shell_surface_request_fullscreen(struct wl_listener *listener,
                                            void *data) {
	struct cg_xdg_shell_view *xdg_shell_view =
	    wl_container_of(listener, xdg_shell_view, request_fullscreen);

	/**
	 * Certain clients do not like figuring out their own window geometry if
	 * they display in fullscreen mode, so we set it here.
	 */
	struct wlr_box layout_box;
	wlr_output_layout_get_box(xdg_shell_view->view.server->output_layout, NULL,
	                          &layout_box);
	wlr_xdg_toplevel_set_size(xdg_shell_view->xdg_surface->toplevel,
	                          layout_box.width, layout_box.height);

	wlr_xdg_toplevel_set_fullscreen(
	    xdg_shell_view->xdg_surface->toplevel,
	    xdg_shell_view->xdg_surface->toplevel->requested.fullscreen);
}

static void
handle_xdg_shell_surface_unmap(struct wl_listener *listener, void *_data) {
	struct cg_xdg_shell_view *xdg_shell_view =
	    wl_container_of(listener, xdg_shell_view, unmap);
	struct cg_view *view = &xdg_shell_view->view;

	view_unmap(view);
}

static void
handle_xdg_shell_surface_map(struct wl_listener *listener, void *_data) {
	struct cg_xdg_shell_view *xdg_shell_view =
	    wl_container_of(listener, xdg_shell_view, map);
	struct cg_view *view = &xdg_shell_view->view;

	view_map(view, xdg_shell_view->xdg_surface->surface,
	         view->server->curr_output
	             ->workspaces[view->server->curr_output->curr_workspace]);
}

static void
handle_xdg_shell_surface_destroy(struct wl_listener *listener, void *_data) {
	struct cg_xdg_shell_view *xdg_shell_view =
	    wl_container_of(listener, xdg_shell_view, destroy);
	struct cg_view *view = &xdg_shell_view->view;

	wl_list_remove(&xdg_shell_view->map.link);
	wl_list_remove(&xdg_shell_view->unmap.link);
	wl_list_remove(&xdg_shell_view->destroy.link);
	wl_list_remove(&xdg_shell_view->request_fullscreen.link);
	xdg_shell_view->xdg_surface = NULL;

	view_destroy(view);
}

static const struct cg_view_impl xdg_shell_view_impl = {.get_pid = get_pid,
                                                        .get_title = get_title,
                                                        .is_primary =
                                                            is_primary,
                                                        .activate = activate,
                                                        .close = close,
                                                        .maximize = maximize,
                                                        .destroy = destroy};

void
handle_xdg_shell_surface_new(struct wl_listener *listener, void *data) {
	struct cg_server *server =
	    wl_container_of(listener, server, new_xdg_shell_surface);
	struct wlr_xdg_surface *xdg_surface = data;

	switch(xdg_surface->role) {
	case WLR_XDG_SURFACE_ROLE_TOPLEVEL:;
		struct cg_xdg_shell_view *xdg_shell_view =
		    calloc(1, sizeof(struct cg_xdg_shell_view));
		if(!xdg_shell_view) {
			wlr_log(WLR_ERROR, "Failed to allocate XDG Shell view");
			return;
		}

		view_init(&xdg_shell_view->view, CG_XDG_SHELL_VIEW,
		          &xdg_shell_view_impl, server);

		xdg_shell_view->xdg_surface = xdg_surface;

		xdg_shell_view->map.notify = handle_xdg_shell_surface_map;
		wl_signal_add(&xdg_surface->surface->events.map, &xdg_shell_view->map);
		xdg_shell_view->unmap.notify = handle_xdg_shell_surface_unmap;
		wl_signal_add(&xdg_surface->surface->events.unmap,
		              &xdg_shell_view->unmap);
		xdg_shell_view->destroy.notify = handle_xdg_shell_surface_destroy;
		wl_signal_add(&xdg_surface->events.destroy, &xdg_shell_view->destroy);
		xdg_shell_view->request_fullscreen.notify =
		    handle_xdg_shell_surface_request_fullscreen;
		wl_signal_add(&xdg_surface->toplevel->events.request_fullscreen,
		              &xdg_shell_view->request_fullscreen);

		xdg_surface->data = xdg_shell_view;
		break;
	case WLR_XDG_SURFACE_ROLE_POPUP:;
		struct wlr_xdg_popup *popup = xdg_surface->popup;
		struct cg_view *view = popup_get_view(popup);
		if(view == NULL) {
			return;
		}

		struct wlr_scene_tree *parent_scene_tree = NULL;
		struct wlr_xdg_surface *parent =
		    wlr_xdg_surface_try_from_wlr_surface(popup->parent);
		if(parent == NULL) {
			return;
		}
		switch(parent->role) {
		case WLR_XDG_SURFACE_ROLE_TOPLEVEL:;
			parent_scene_tree = view->scene_tree;
			break;
		case WLR_XDG_SURFACE_ROLE_POPUP:
			parent_scene_tree = parent->data;
			break;
		case WLR_XDG_SURFACE_ROLE_NONE:
			break;
		}
		if(parent_scene_tree == NULL) {
			return;
		}

		struct wlr_scene_tree *popup_scene_tree =
		    wlr_scene_xdg_surface_create(parent_scene_tree, xdg_surface);
		if(popup_scene_tree == NULL) {
			wlr_log(WLR_ERROR,
			        "Failed to allocate scene-graph node for XDG popup");
			return;
		}

		popup_unconstrain(view, popup);

		xdg_surface->data = popup_scene_tree;
		break;
	case WLR_XDG_SURFACE_ROLE_NONE:
		assert(false); // unreachable
	}
}

void
handle_xdg_toplevel_decoration(struct wl_listener *listener, void *data) {
	struct cg_server *server =
	    wl_container_of(listener, server, xdg_toplevel_decoration);
	struct wlr_xdg_toplevel_decoration_v1 *wlr_decoration = data;

	struct cg_xdg_decoration *xdg_decoration =
	    calloc(1, sizeof(struct cg_xdg_decoration));
	if(!xdg_decoration) {
		return;
	}

	xdg_decoration->wlr_decoration = wlr_decoration;
	xdg_decoration->server = server;

	xdg_decoration->destroy.notify = xdg_decoration_handle_destroy;
	wl_signal_add(&wlr_decoration->events.destroy, &xdg_decoration->destroy);
	xdg_decoration->request_mode.notify = xdg_decoration_handle_request_mode;
	wl_signal_add(&wlr_decoration->events.request_mode,
	              &xdg_decoration->request_mode);

	xdg_decoration_handle_request_mode(&xdg_decoration->request_mode,
	                                   wlr_decoration);
}
