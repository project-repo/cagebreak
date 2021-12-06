/*
 * Cagebreak: A Wayland tiling compositor.
 *
 * Copyright (C) 2020-2022 The Cagebreak Authors
 * Copyright (C) 2018-2020 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_box.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_surface.h>

#include "output.h"
#include "seat.h"
#include "server.h"
#include "view.h"
#include "workspace.h"
#if CG_HAS_XWAYLAND
#include "xwayland.h"
#endif

struct cg_view *
view_get_prev_view(struct cg_view *view) {
	struct cg_view *prev = NULL;

	struct cg_view *it_view;
	struct wl_list *it;
	it = view->link.prev;
	while(it != &view->link) {
		if(it == &view->workspace->views) {
			it = it->prev;
			continue;
		}
		it_view = wl_container_of(it, it_view, link);
		if(!view_is_visible(it_view)) {
			prev = it_view;
			break;
		}
		it = it->prev;
	}
	return prev;
}

void
view_damage_child(struct cg_view_child *child, bool whole) {
	if(child->view != NULL) {
		view_damage_part(child->view);
	}
}

static void
view_child_handle_commit(struct wl_listener *listener, void *_data) {
	struct cg_view_child *child = wl_container_of(listener, child, commit);
	view_damage_child(child, false);
}

static void
subsurface_create(struct cg_view_child *parent, struct cg_view *view,
                  struct wlr_subsurface *wlr_subsurface);

static void
view_child_handle_new_subsurface(struct wl_listener *listener, void *data) {
	struct cg_view_child *child =
	    wl_container_of(listener, child, new_subsurface);
	struct wlr_subsurface *wlr_subsurface = data;
	subsurface_create(child, child->view, wlr_subsurface);
}

void
view_child_finish(struct cg_view_child *child) {
	if(!child) {
		return;
	}

	if(child->view != NULL) {
		view_damage_child(child, true);
	}

	struct cg_view_child *subchild, *tmpchild;
	wl_list_for_each_safe(subchild, tmpchild, &child->children, parent_link) {
		subchild->parent = NULL;
		wl_list_remove(&subchild->parent_link);
	}

	wl_list_remove(&child->link);
	if(child->parent != NULL) {
		wl_list_remove(&child->parent_link);
	}
	wl_list_remove(&child->commit.link);
	wl_list_remove(&child->new_subsurface.link);
}

void
view_child_init(struct cg_view_child *child, struct cg_view_child *parent,
                struct cg_view *view, struct wlr_surface *wlr_surface) {
	child->view = view;
	child->parent = parent;
	if(parent != NULL) {
		wl_list_insert(&parent->children, &child->parent_link);
	}
	child->wlr_surface = wlr_surface;
	wl_list_init(&child->children);

	child->commit.notify = view_child_handle_commit;
	wl_signal_add(&wlr_surface->events.commit, &child->commit);
	child->new_subsurface.notify = view_child_handle_new_subsurface;
	wl_signal_add(&wlr_surface->events.new_subsurface, &child->new_subsurface);

	wl_list_insert(&view->children, &child->link);
}

static void
subsurface_destroy(struct cg_view_child *child) {
	if(!child) {
		return;
	}

	struct cg_subsurface *subsurface = (struct cg_subsurface *)child;
	wl_list_remove(&subsurface->destroy.link);
	view_child_finish(&subsurface->view_child);
	free(subsurface);
}

static void
subsurface_handle_destroy(struct wl_listener *listener, void *_data) {
	struct cg_subsurface *subsurface =
	    wl_container_of(listener, subsurface, destroy);
	struct cg_view_child *view_child = (struct cg_view_child *)subsurface;
	subsurface_destroy(view_child);
}

static void
subsurface_create(struct cg_view_child *parent, struct cg_view *view,
                  struct wlr_subsurface *wlr_subsurface) {
	struct cg_subsurface *subsurface = calloc(1, sizeof(struct cg_subsurface));
	if(!subsurface) {
		return;
	}

	view_child_init(&subsurface->view_child, parent, view,
	                wlr_subsurface->surface);
	subsurface->view_child.destroy = subsurface_destroy;
	subsurface->wlr_subsurface = wlr_subsurface;

	subsurface->destroy.notify = subsurface_handle_destroy;
	wl_signal_add(&wlr_subsurface->events.destroy, &subsurface->destroy);
}

static void
handle_new_subsurface(struct wl_listener *listener, void *data) {
	struct cg_view *view = wl_container_of(listener, view, new_subsurface);
	struct wlr_subsurface *wlr_subsurface = data;
	subsurface_create(NULL, view, wlr_subsurface);
}

char *
view_get_title(const struct cg_view *view) {
	const char *title = view->impl->get_title(view);
	if(!title) {
		return NULL;
	}
	return strndup(title, strlen(title));
}

bool
view_is_primary(const struct cg_view *view) {
	return view->impl->is_primary(view);
}

struct cg_tile *
view_get_tile(const struct cg_view *view) {

	if(view->tile != NULL && view->tile->view == view) {
		return view->tile;
	} else {
		return NULL;
	}
}

bool
view_is_visible(const struct cg_view *view) {
#if CG_HAS_XWAYLAND
	if(view->type == CG_XWAYLAND_VIEW && !xwayland_view_should_manage(view)) {
		return true;
	}
#endif
	return view_get_tile(view) != NULL;
}

void
view_damage(struct cg_view *view, bool whole) {
	struct cg_tile *view_tile = view_get_tile(view);
	if(view_tile != NULL &&
	   (view->wlr_surface->current.width != view_tile->tile.width ||
	    view->wlr_surface->current.height != view_tile->tile.height)) {
		view_maximize(view, view_tile);
	}
	output_damage_surface(view->workspace->output, view->wlr_surface, view->ox,
	                      view->oy, whole);
}

void
view_damage_part(struct cg_view *view) {
	view_damage(view, false);
}

void
view_damage_whole(struct cg_view *view) {
	view_damage(view, true);
}

void
view_activate(struct cg_view *view, bool activate) {
	if(view != NULL) {
		view->impl->activate(view, activate);
	}
}

void
view_maximize(struct cg_view *view, struct cg_tile *tile) {
	view->ox = tile->tile.x;
	view->oy = tile->tile.y;
	view->impl->maximize(view, tile->tile.width, tile->tile.height);
	view->tile = tile;
}

void
view_for_each_surface(struct cg_view *view,
                      wlr_surface_iterator_func_t iterator, void *data) {
	view->impl->for_each_surface(view, iterator, data);
}

void
view_for_each_popup(struct cg_view *view, wlr_surface_iterator_func_t iterator,
                    void *data) {
	if(!view->impl->for_each_popup) {
		return;
	}
	view->impl->for_each_popup(view, iterator, data);
}

void
view_unmap(struct cg_view *view) {
	/* If the view is not mapped, do nothing */
	if(view->wlr_surface == NULL) {
		return;
	}

	struct cg_view_child *child, *tmp;
	wl_list_for_each_safe(child, tmp, &view->children, link) {
		child->destroy(child);
	}
#if CG_HAS_XWAYLAND
	if((view->type != CG_XWAYLAND_VIEW || xwayland_view_should_manage(view)))
#endif
	{
		struct cg_tile *view_tile = view_get_tile(view);
		if(view_tile != NULL) {
			wlr_output_damage_add_box(view_tile->workspace->output->damage,
			                          &view_tile->tile);
		}
		struct cg_view *prev = view_get_prev_view(view);
		if(view == view->server->seat->focused_view) {
			seat_set_focus(view->server->seat, prev);
		} else if(view->server->seat->seat->keyboard_state.focused_surface ==
		          view->wlr_surface) {
			wlr_seat_keyboard_clear_focus(view->server->seat->seat);
			seat_set_focus(view->server->seat,
			               view->server->seat->focused_view);
		}
		view_tile = view_get_tile(view);
		if(view_tile != NULL) {
			wlr_output_damage_add_box(view_tile->workspace->output->damage,
			                          &view_tile->tile);
			view_tile->view = prev;
			if(prev != NULL) {
				view_maximize(prev, view_tile);
			}
		}
	}
#if CG_HAS_XWAYLAND
	else {
		view_damage_whole(view);
		if(view->server->seat->seat->keyboard_state.focused_surface == NULL ||
		   view->server->seat->seat->keyboard_state.focused_surface ==
		       view->wlr_surface) {
			seat_set_focus(view->server->seat,
			               view->server->seat->focused_view);
		}
	}
#endif

	wl_list_remove(&view->link);

	wl_list_remove(&view->new_subsurface.link);
	view->wlr_surface = NULL;
}

void
view_map(struct cg_view *view, struct wlr_surface *surface,
         struct cg_workspace *ws) {
	struct cg_output *output = ws->output;
	view->wlr_surface = surface;

	struct wlr_subsurface *subsurface;
	wl_list_for_each(subsurface, &view->wlr_surface->subsurfaces_above,
	                 parent_link) {
		subsurface_create(NULL, view, subsurface);
	}

	wl_list_for_each(subsurface, &view->wlr_surface->subsurfaces_below,
	                 parent_link) {
		subsurface_create(NULL, view, subsurface);
	}

	view->new_subsurface.notify = handle_new_subsurface;
	wl_signal_add(&view->wlr_surface->events.new_subsurface,
	              &view->new_subsurface);

	view->workspace = ws;

#if CG_HAS_XWAYLAND
	/* We shouldn't position override-redirect windows. They set
	   their own (x,y) coordinates in handle_wayland_surface_map. */
	if(view->type == CG_XWAYLAND_VIEW && !xwayland_view_should_manage(view)) {
		wl_list_insert(&ws->unmanaged_views, &view->link);
	} else
#endif
	{
		view->tile = view->workspace->focused_tile;
		view_maximize(view, view->tile);
		wl_list_insert(&ws->views, &view->link);
	}
	seat_set_focus(output->server->seat, view);
}

void
view_destroy(struct cg_view *view) {
	struct cg_output *curr_output = view->server->curr_output;
	if(view->wlr_surface != NULL) {
		view_unmap(view);
	}

	view->impl->destroy(view);
	view_activate(curr_output->workspaces[curr_output->curr_workspace]
	                  ->focused_tile->view,
	              true);
}

void
view_init(struct cg_view *view, enum cg_view_type type,
          const struct cg_view_impl *impl, struct cg_server *server) {
	view->workspace = NULL;
	view->tile = NULL;
	view->server = server;
	view->type = type;
	view->impl = impl;

	wl_list_init(&view->children);
}

struct wlr_surface *
view_wlr_surface_at(const struct cg_view *view, double sx, double sy,
                    double *sub_x, double *sub_y) {
	return view->impl->wlr_surface_at(view, sx, sy, sub_x, sub_y);
}
