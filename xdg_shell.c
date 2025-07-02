// Copyright 2020 - 2025, project-repo and the cagebreak contributors
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

static void
xdg_decoration_handle_destroy(struct wl_listener *listener,
                              __attribute__((unused)) void *data) {
	struct cg_xdg_decoration *xdg_decoration =
	    wl_container_of(listener, xdg_decoration, destroy);

	wl_list_remove(&xdg_decoration->destroy.link);
	wl_list_remove(&xdg_decoration->request_mode.link);
	wl_list_remove(&xdg_decoration->link);
	free(xdg_decoration);
}

static void
xdg_decoration_handle_request_mode(struct wl_listener *listener,
                                   __attribute__((unused)) void *_data) {
	struct cg_xdg_decoration *xdg_decoration =
	    wl_container_of(listener, xdg_decoration, request_mode);

	if(xdg_decoration->wlr_decoration->toplevel->base->initialized) {
		wlr_xdg_toplevel_decoration_v1_set_mode(
		    xdg_decoration->wlr_decoration,
		    WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}
}

static void
popup_unconstrain(struct cg_xdg_shell_popup *popup) {
	struct cg_view *view = popup->view;

	struct wlr_box output_toplevel_box = {
	    .x = -view->ox,
	    .y = -view->oy,
	    .width = view->workspace->output->wlr_output->width,
	    .height = view->workspace->output->wlr_output->height};

	wlr_xdg_popup_unconstrain_from_box(popup->wlr_popup, &output_toplevel_box);
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
	return xdg_shell_view->toplevel->title;
}

static bool
is_primary(const struct cg_view *view) {
	const struct cg_xdg_shell_view *xdg_shell_view =
	    xdg_shell_view_from_const_view(view);
	struct wlr_xdg_toplevel *parent = xdg_shell_view->toplevel->parent;
	return parent == NULL;
}

static void
activate(struct cg_view *view, bool activate) {
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	wlr_xdg_toplevel_set_activated(xdg_shell_view->toplevel, activate);
}

static void
close(struct cg_view *view) {
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	if(view->type == CG_XDG_SHELL_VIEW) {
		wlr_xdg_toplevel_send_close(xdg_shell_view->toplevel);
	}
}

static void
maximize(struct cg_view *view, int width, int height) {
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	wlr_xdg_toplevel_set_size(xdg_shell_view->toplevel, width, height);
	enum wlr_edges edges =
	    WLR_EDGE_LEFT | WLR_EDGE_RIGHT | WLR_EDGE_TOP | WLR_EDGE_BOTTOM;
	wlr_xdg_toplevel_set_tiled(xdg_shell_view->toplevel, edges);
}

static void
destroy(struct cg_view *view) {
	struct cg_xdg_shell_view *xdg_shell_view = xdg_shell_view_from_view(view);
	free(xdg_shell_view);
}

static void
handle_xdg_shell_surface_request_fullscreen(
    struct wl_listener *listener, __attribute__((unused)) void *data) {
	struct cg_xdg_shell_view *xdg_shell_view =
	    wl_container_of(listener, xdg_shell_view, request_fullscreen);

	/**
	 * Certain clients do not like figuring out their own window geometry if
	 * they display in fullscreen mode, so we set it here (if the view was
	 * already mapped).
	 */
	if(xdg_shell_view->view.workspace != NULL) {
		struct wlr_box layout_box;
		wlr_output_layout_get_box(
		    xdg_shell_view->view.server->output_layout,
		    xdg_shell_view->view.workspace->output->wlr_output, &layout_box);
		wlr_xdg_toplevel_set_size(xdg_shell_view->toplevel, layout_box.width,
		                          layout_box.height);
	}

	wlr_xdg_toplevel_set_fullscreen(
	    xdg_shell_view->toplevel,
	    xdg_shell_view->toplevel->requested.fullscreen);
}

static void
handle_xdg_shell_surface_unmap(struct wl_listener *listener,
                               __attribute__((unused)) void *_data) {
	struct cg_xdg_shell_view *xdg_shell_view =
	    wl_container_of(listener, xdg_shell_view, unmap);
	struct cg_view *view = &xdg_shell_view->view;
	wl_list_remove(&xdg_shell_view->new_popup.link);
	wl_list_remove(&xdg_shell_view->request_fullscreen.link);
	view_unmap(view);
}

struct cg_xdg_decoration *
xdg_decoration_from_surface(struct wlr_surface *surface,
                            struct cg_server *server) {
	struct cg_xdg_decoration *deco;
	wl_list_for_each(deco, &server->xdg_decorations, link) {
		if(deco->wlr_decoration->toplevel->base->surface == surface) {
			return deco;
		}
	}
	return NULL;
}

static void
handle_xdg_shell_surface_map(struct wl_listener *listener,
                             __attribute__((unused)) void *_data) {
	struct cg_xdg_shell_view *xdg_shell_view =
	    wl_container_of(listener, xdg_shell_view, map);
	struct cg_view *view = &xdg_shell_view->view;

	xdg_shell_view->new_popup.notify = handle_xdg_shell_popup_new;
	wl_signal_add(&xdg_shell_view->toplevel->base->events.new_popup,
	              &xdg_shell_view->new_popup);
	xdg_shell_view->request_fullscreen.notify =
	    handle_xdg_shell_surface_request_fullscreen;
	wl_signal_add(&xdg_shell_view->toplevel->events.request_fullscreen,
	              &xdg_shell_view->request_fullscreen);
	view_map(view, xdg_shell_view->toplevel->base->surface,
	         view->server->curr_output
	             ->workspaces[view->server->curr_output->curr_workspace]);
}

static void
handle_xdg_shell_surface_destroy(struct wl_listener *listener,
                                 __attribute__((unused)) void *_data) {
	struct cg_xdg_shell_view *xdg_shell_view =
	    wl_container_of(listener, xdg_shell_view, destroy);
	struct cg_view *view = &xdg_shell_view->view;

	wl_list_remove(&xdg_shell_view->map.link);
	wl_list_remove(&xdg_shell_view->unmap.link);
	wl_list_remove(&xdg_shell_view->destroy.link);
	wl_list_remove(&xdg_shell_view->commit.link);
	xdg_shell_view->toplevel = NULL;

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
handle_xdg_shell_toplevel_commit(struct wl_listener *listener,
                                 __attribute__((unused)) void *data) {
	struct cg_xdg_shell_view *xdg_shell_view =
	    wl_container_of(listener, xdg_shell_view, commit);
	struct cg_view *view = &xdg_shell_view->view;
	struct wlr_xdg_surface *xdg_surface = xdg_shell_view->toplevel->base;
	if(xdg_surface->initial_commit) {
		struct cg_xdg_decoration *decoration =
		    xdg_decoration_from_surface(xdg_surface->surface, view->server);
		if(decoration != NULL) {
			wlr_xdg_toplevel_decoration_v1_set_mode(
			    decoration->wlr_decoration,
			    WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
		}
		wlr_xdg_surface_schedule_configure(xdg_surface);
		wlr_xdg_toplevel_set_wm_capabilities(
		    xdg_shell_view->toplevel, XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);
	}
}

void
handle_xdg_shell_toplevel_new(struct wl_listener *listener, void *data) {
	struct cg_server *server =
	    wl_container_of(listener, server, new_xdg_shell_toplevel);
	struct wlr_xdg_toplevel *xdg_toplevel = data;
	wlr_xdg_surface_ping(xdg_toplevel->base);

	struct cg_xdg_shell_view *xdg_shell_view =
	    calloc(1, sizeof(struct cg_xdg_shell_view));
	if(!xdg_shell_view) {
		wlr_log(WLR_ERROR, "Failed to allocate XDG Shell view");
		return;
	}

	view_init(&xdg_shell_view->view, CG_XDG_SHELL_VIEW, &xdg_shell_view_impl,
	          server);

	xdg_shell_view->toplevel = xdg_toplevel;

	xdg_shell_view->commit.notify = handle_xdg_shell_toplevel_commit;
	wl_signal_add(&xdg_toplevel->base->surface->events.commit,
	              &xdg_shell_view->commit);
	xdg_shell_view->map.notify = handle_xdg_shell_surface_map;
	wl_signal_add(&xdg_toplevel->base->surface->events.map,
	              &xdg_shell_view->map);
	xdg_shell_view->unmap.notify = handle_xdg_shell_surface_unmap;
	wl_signal_add(&xdg_toplevel->base->surface->events.unmap,
	              &xdg_shell_view->unmap);
	xdg_shell_view->destroy.notify = handle_xdg_shell_surface_destroy;
	wl_signal_add(&xdg_toplevel->base->events.destroy,
	              &xdg_shell_view->destroy);

	wlr_scene_xdg_surface_create(xdg_shell_view->view.scene_tree,
	                             xdg_toplevel->base);
	xdg_toplevel->base->data = xdg_shell_view;
	return;
}

static void
handle_xdg_shell_popup_destroy(struct wl_listener *listener,
                               __attribute__((unused)) void *data) {
	struct cg_xdg_shell_popup *popup =
	    wl_container_of(listener, popup, destroy);

	wl_list_remove(&popup->new_popup.link);
	wl_list_remove(&popup->destroy.link);
	wl_list_remove(&popup->commit.link);
	wl_list_remove(&popup->reposition.link);
	wlr_scene_node_destroy(&popup->scene_tree->node);
	free(popup);
}

static void
handle_xdg_shell_popup_commit(struct wl_listener *listener,
                              __attribute__((unused)) void *data) {
	struct cg_xdg_shell_popup *popup = wl_container_of(listener, popup, commit);
	if(popup->wlr_popup->base->initial_commit) {
		popup_unconstrain(popup);
	}
}

static void
handle_xdg_shell_popup_reposition(struct wl_listener *listener,
                                  __attribute__((unused)) void *data) {
	struct cg_xdg_shell_popup *popup =
	    wl_container_of(listener, popup, reposition);
	popup_unconstrain(popup);
}

static struct cg_xdg_shell_popup *
create_xdg_popup(struct wlr_xdg_popup *wlr_popup, struct cg_view *view,
                 struct wlr_scene_tree *parent_tree);

void
handle_xdg_shell_popup_new_popup(struct wl_listener *listener, void *data) {
	struct cg_xdg_shell_popup *popup =
	    wl_container_of(listener, popup, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;
	create_xdg_popup(wlr_popup, popup->view, popup->xdg_surface_tree);
}

static struct cg_xdg_shell_popup *
create_xdg_popup(struct wlr_xdg_popup *wlr_popup, struct cg_view *view,
                 struct wlr_scene_tree *parent_tree) {

	struct wlr_xdg_surface *xdg_surface = wlr_popup->base;
	struct cg_xdg_shell_popup *popup = calloc(1, sizeof(*popup));
	if(!popup) {
		return NULL;
	}

	popup->wlr_popup = wlr_popup;
	popup->view = view;
	struct cg_xdg_shell_view *xdg_view = wl_container_of(view, xdg_view, view);
	xdg_surface->data = xdg_view;
	popup->scene_tree = wlr_scene_tree_create(parent_tree);
	if(!popup->scene_tree) {
		free(popup);
		return NULL;
	}

	popup->xdg_surface_tree =
	    wlr_scene_xdg_surface_create(popup->scene_tree, xdg_surface);
	if(!popup->xdg_surface_tree) {
		wlr_scene_node_destroy(&popup->scene_tree->node);
		free(popup);
		return NULL;
	}

	wl_signal_add(&xdg_surface->surface->events.commit, &popup->commit);
	popup->commit.notify = handle_xdg_shell_popup_commit;
	wl_signal_add(&xdg_surface->events.new_popup, &popup->new_popup);
	popup->new_popup.notify = handle_xdg_shell_popup_new_popup;
	wl_signal_add(&wlr_popup->events.reposition, &popup->reposition);
	popup->reposition.notify = handle_xdg_shell_popup_reposition;
	wl_signal_add(&wlr_popup->events.destroy, &popup->destroy);
	popup->destroy.notify = handle_xdg_shell_popup_destroy;
	return popup;
}

void
handle_xdg_shell_popup_new(struct wl_listener *listener, void *data) {

	struct cg_xdg_shell_view *xdg_shell_view =
	    wl_container_of(listener, xdg_shell_view, new_popup);
	struct wlr_xdg_popup *xdg_popup = data;

	create_xdg_popup(xdg_popup, &xdg_shell_view->view,
	                 xdg_shell_view->view.scene_tree);
	return;
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

	wl_list_insert(&server->xdg_decorations, &xdg_decoration->link);

	xdg_decoration->wlr_decoration = wlr_decoration;
	xdg_decoration->server = server;

	xdg_decoration->destroy.notify = xdg_decoration_handle_destroy;
	wl_signal_add(&wlr_decoration->events.destroy, &xdg_decoration->destroy);
	xdg_decoration->request_mode.notify = xdg_decoration_handle_request_mode;
	wl_signal_add(&wlr_decoration->events.request_mode,
	              &xdg_decoration->request_mode);
}
