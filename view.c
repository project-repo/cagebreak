/*
 * Cagebreak: A Wayland tiling compositor.
 *
 * Copyright (C) 2020 The Cagebreak Authors
 * Copyright (C) 2018-2020 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_box.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/types/wlr_output_damage.h>

#include "output.h"
#include "seat.h"
#include "server.h"
#include "workspace.h"
#include "xdg_shell.h"
#include "view.h"
#if CG_HAS_XWAYLAND
#include "xwayland.h"
#endif

struct cg_view*
view_get_prev_view(struct cg_view *view) {
	struct cg_view *prev = NULL;

	struct cg_view *it_view;
	wl_list_for_each(it_view, &view->workspace->views, link) {
		if(!view_is_visible(it_view) && view != it_view) {
			prev = it_view;
			break;
		}
	}

	return prev;
}

static void
view_child_handle_commit(struct wl_listener *listener, void *_data) {
	struct cg_view_child *child = wl_container_of(listener, child, commit);
	view_damage_part(child->view);
}

static void
subsurface_create(struct cg_view *view, struct wlr_subsurface *wlr_subsurface);

static void
view_child_handle_new_subsurface(struct wl_listener *listener, void *data) {
	struct cg_view_child *child =
	    wl_container_of(listener, child, new_subsurface);
	struct wlr_subsurface *wlr_subsurface = data;
	subsurface_create(child->view, wlr_subsurface);
}

void
view_child_finish(struct cg_view_child *child) {
	if(!child) {
		return;
	}

	view_damage_whole(child->view);

	wl_list_remove(&child->link);
	wl_list_remove(&child->commit.link);
	wl_list_remove(&child->new_subsurface.link);
}

void
view_child_init(struct cg_view_child *child, struct cg_view *view,
                struct wlr_surface *wlr_surface) {
	child->view = view;
	child->wlr_surface = wlr_surface;

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
subsurface_create(struct cg_view *view, struct wlr_subsurface *wlr_subsurface) {
	struct cg_subsurface *subsurface = calloc(1, sizeof(struct cg_subsurface));
	if(!subsurface) {
		return;
	}

	view_child_init(&subsurface->view_child, view, wlr_subsurface->surface);
	subsurface->view_child.destroy = subsurface_destroy;
	subsurface->wlr_subsurface = wlr_subsurface;

	subsurface->destroy.notify = subsurface_handle_destroy;
	wl_signal_add(&wlr_subsurface->events.destroy, &subsurface->destroy);
}

static void
handle_new_subsurface(struct wl_listener *listener, void *data) {
	struct cg_view *view = wl_container_of(listener, view, new_subsurface);
	struct wlr_subsurface *wlr_subsurface = data;
	subsurface_create(view, wlr_subsurface);
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

	bool first = true;
	for(struct cg_tile *tile = view->workspace->focused_tile;
	    first || view->workspace->focused_tile != tile; tile = tile->next) {
		first = false;
		if(tile->view == view) {
			return tile;
		}
	}
	return NULL;
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
view_maximize(struct cg_view *view, const struct wlr_box *tile_box) {
	view->ox = tile_box->x;
	view->oy = tile_box->y;
	view->impl->maximize(view, tile_box->width, tile_box->height);
}

void
view_position(struct cg_view *view) {
	struct wlr_box *tile_workspace_box =
	    &view->workspace->output
	         ->workspaces[view->workspace->output->curr_workspace]
	         ->focused_tile->tile;

	view_maximize(view, tile_workspace_box);
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
#if CG_HAS_XWAYLAND
	if((view->type != CG_XWAYLAND_VIEW || xwayland_view_should_manage(view)))
#endif
	{
		if(view_is_visible(view)) {
			struct cg_view* prev = view_get_prev_view(view);
			struct cg_tile* view_tile = view_get_tile(view);
			wlr_output_damage_add_box(view_tile->workspace->output->damage, &view_tile->tile);
			if(view->workspace->server->seat->seat->keyboard_state.focused_surface == NULL || view->workspace->server->seat->seat->keyboard_state.focused_surface == view->wlr_surface) {
				seat_set_focus(view->workspace->server->seat, prev);
			} else {
				view_tile->view = prev;
				if(prev != NULL) {
					view_maximize(prev, &view_tile->tile);
				}
			}
		}
	}
#if CG_HAS_XWAYLAND
	else {
		view_damage_whole(view);
		if(view->workspace->server->seat->seat->keyboard_state.focused_surface == NULL || view->workspace->server->seat->seat->keyboard_state.focused_surface == view->wlr_surface) {
			seat_set_focus(view->workspace->server->seat, view->workspace->server->seat->focused_view);
		}
	}
#endif

	wl_list_remove(&view->link);

	wl_list_remove(&view->new_subsurface.link);

	struct cg_view_child *child, *tmp;
	wl_list_for_each_safe(child, tmp, &view->children, link) {
		child->destroy(child);
	}

	view->wlr_surface = NULL;
}

void
view_map(struct cg_view *view, struct wlr_surface *surface) {
	struct cg_output *output = view->workspace->output;
	view->wlr_surface = surface;

	struct wlr_subsurface *subsurface;
	wl_list_for_each(subsurface, &view->wlr_surface->subsurfaces, parent_link) {
		subsurface_create(view, subsurface);
	}

	view->new_subsurface.notify = handle_new_subsurface;
	wl_signal_add(&view->wlr_surface->events.new_subsurface,
	              &view->new_subsurface);

#if CG_HAS_XWAYLAND
	/* We shouldn't position override-redirect windows. They set
	   their own (x,y) coordinates in handle_wayland_surface_map. */
	if(view->type == CG_XWAYLAND_VIEW && !xwayland_view_should_manage(view)) {
		wl_list_insert(
		    &output->workspaces[output->curr_workspace]->unmanaged_views,
		    &view->link);
	} else
#endif
	{
		view_position(view);
		wl_list_insert(&output->workspaces[output->curr_workspace]->views,
		               &view->link);
	}
	seat_set_focus(output->server->seat, view);
}

void
view_destroy(struct cg_view *view) {
	struct cg_output* curr_output = view->workspace->server->curr_output;
	if(view->wlr_surface != NULL) {
		view_unmap(view);
	}

	view->impl->destroy(view);
	view_activate(curr_output->workspaces[curr_output->curr_workspace]->focused_tile->view, true);
}

void
view_init(struct cg_view *view, struct cg_workspace *ws, enum cg_view_type type,
          const struct cg_view_impl *impl) {
	view->workspace = ws;
	view->type = type;
	view->impl = impl;

	wl_list_init(&view->children);
}

struct wlr_surface *
view_wlr_surface_at(const struct cg_view *view, double sx, double sy,
                    double *sub_x, double *sub_y) {
	return view->impl->wlr_surface_at(view, sx, sy, sub_x, sub_y);
}
