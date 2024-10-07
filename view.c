// Copyright 2020 - 2024, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>

#include "ipc_server.h"
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
view_activate(struct cg_view *view, bool activate) {
	if(view != NULL) {
		view->impl->activate(view, activate);
	}
}

void
view_maximize(struct cg_view *view, struct cg_tile *tile) {
	view->ox = tile->tile.x;
	view->oy = tile->tile.y;
	wlr_scene_node_set_position(
	    &view->scene_tree->node,
	    view->ox + output_get_layout_box(view->workspace->output).x,
	    view->oy + output_get_layout_box(view->workspace->output).y);
	view->impl->maximize(view, tile->tile.width, tile->tile.height);
	view->tile = tile;
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

void
view_unmap(struct cg_view *view) {

	uint32_t id = view->id;
	uint32_t tile_id = 0;
	uint32_t ws = view->workspace->num;
	char *output_name = view->workspace->output->name;
	int output_id = output_get_num(view->workspace->output);
	pid_t pid = view->impl->get_pid(view);
	/* If the view is not mapped, do nothing */
	if(view->wlr_surface == NULL) {
		return;
	}

	if(view->tile == NULL) {
		tile_id = -1;
	} else {
		tile_id = view->tile->id;
	}

#if CG_HAS_XWAYLAND
	if((view->type != CG_XWAYLAND_VIEW || xwayland_view_should_manage(view)))
#endif
	{
		struct cg_view *prev = view_get_prev_view(view);
		if(view == view->server->seat->focused_view) {
			seat_set_focus(view->server->seat, prev);
		} else if(view->server->seat->seat->keyboard_state.focused_surface ==
		          view->wlr_surface) {
			wlr_seat_keyboard_clear_focus(view->server->seat->seat);
			seat_set_focus(view->server->seat,
			               view->server->seat->focused_view);
		}
		struct cg_tile *view_tile = view_get_tile(view);
		if(view_tile != NULL) {
			workspace_tile_update_view(view_tile, prev);
		}
	}
#if CG_HAS_XWAYLAND
	else {
		if(view->server->seat->seat->keyboard_state.focused_surface == NULL ||
		   view->server->seat->seat->keyboard_state.focused_surface ==
		       view->wlr_surface) {
			seat_set_focus(view->server->seat,
			               view->server->seat->focused_view);
		}
	}
#endif

	wlr_scene_node_destroy(&view->scene_tree->node);

	wl_list_remove(&view->link);

	view->wlr_surface = NULL;
	ipc_send_event(
	    view->workspace->server,
	    "{\"event_name\":\"view_unmap\",\"view_id\":%d,\"tile_id\":%d,"
	    "\"workspace\":%d,\"output\":\"%s\",\"output_id\":%d,\"view_pid\":%d}",
	    id, tile_id, ws + 1, output_name, output_id, pid);
}

void
view_map(struct cg_view *view, struct wlr_surface *surface,
         struct cg_workspace *ws) {
	struct cg_output *output = ws->output;
	view->wlr_surface = surface;

	wlr_scene_node_reparent(&view->scene_tree->node, ws->scene);
	if(!view->scene_tree) {
		wl_resource_post_no_memory(surface->resource);
		return;
	}
	view->scene_tree->node.data = view;
	view->workspace = ws;

#if CG_HAS_XWAYLAND
	/* We shouldn't position override-redirect windows. They set
	   their own (x,y) coordinates in handle_wayland_surface_map. */
	if(view->type == CG_XWAYLAND_VIEW && !xwayland_view_should_manage(view)) {
		wl_list_insert(&ws->unmanaged_views, &view->link);
		wlr_scene_node_set_position(
		    &view->scene_tree->node,
		    view->ox + output_get_layout_box(view->workspace->output).x,
		    view->oy + output_get_layout_box(view->workspace->output).y);
	} else
#endif
	{
		wl_list_insert(&ws->views, &view->link);
	}
	seat_set_focus(output->server->seat, view);
	int tile_id = 0;
	if(view->tile == NULL) {
		tile_id = -1;
	} else {
		tile_id = view->tile->id;
	}
	ipc_send_event(
	    output->server,
	    "{\"event_name\":\"view_map\",\"view_id\":%d,\"tile_id\":%d,"
	    "\"workspace\":%d,\"output\":\"%s\",\"output_id\":%d,\"view_pid\":%d}",
	    view->id, tile_id, view->workspace->num + 1,
	    view->workspace->output->name, output_get_num(view->workspace->output),
	    view->impl->get_pid(view));
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
	view->id = server->views_curr_id;
	++server->views_curr_id;
	view->scene_tree = wlr_scene_tree_create(
	    server->curr_output->workspaces[server->curr_output->curr_workspace]
	        ->scene);
}
