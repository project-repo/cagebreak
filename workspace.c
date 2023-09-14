// Copyright 2020 - 2023, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "message.h"
#include "output.h"
#include "seat.h"
#include "server.h"
#include "view.h"
#include "workspace.h"

void
workspace_tile_update_view(struct cg_tile *tile, struct cg_view *view) {
	if(tile->view != NULL) {
		wlr_scene_node_set_enabled(&tile->view->scene_tree->node, false);
		tile->view->tile = NULL;
	}
	tile->view = view;
	if(view != NULL) {
		view_maximize(view, tile);
		wlr_scene_node_set_enabled(&view->scene_tree->node, true);
	}
}

#if CG_HAS_FANALYZE
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
int
full_screen_workspace_tiles(struct wlr_output_layout *layout,
                            struct cg_workspace *workspace,
                            uint32_t *tiles_curr_id) {
	workspace->focused_tile = calloc(1, sizeof(struct cg_tile));
	if(!workspace->focused_tile) {
		return -1;
	}
	workspace->focused_tile->workspace = workspace;
	workspace->focused_tile->next = workspace->focused_tile;
	workspace->focused_tile->prev = workspace->focused_tile;
	workspace->focused_tile->tile.x = 0;
	workspace->focused_tile->tile.y = 0;
	workspace->focused_tile->tile.width =
	    output_get_layout_box(workspace->output).width;
	workspace->focused_tile->tile.height =
	    output_get_layout_box(workspace->output).height;
	workspace_tile_update_view(workspace->focused_tile, NULL);
	workspace->focused_tile->id = *tiles_curr_id;
	++(*tiles_curr_id);
	return 0;
}
#if CG_HAS_FANALYZE
#pragma GCC diagnostic pop
#endif

struct cg_workspace *
full_screen_workspace(struct cg_output *output) {
	struct cg_workspace *workspace = calloc(1, sizeof(struct cg_workspace));
	if(!workspace) {
		return NULL;
	}
	struct wlr_scene_output *scene_output =
	    wlr_scene_get_scene_output(output->server->scene, output->wlr_output);
	if(scene_output == NULL) {
		free(workspace);
		return NULL;
	}
	workspace->output = output;
	workspace->server = output->server;
	workspace->num = -1;
	workspace->scene = wlr_scene_tree_create(&scene_output->scene->tree);
	if(full_screen_workspace_tiles(output->server->output_layout, workspace,
	                               &output->server->tiles_curr_id) != 0) {
		free(workspace);
		return NULL;
	}
	return workspace;
}

void
workspace_focus_tile(struct cg_workspace *ws, struct cg_tile *tile) {
	ws->focused_tile = tile;
	struct wlr_box *box = malloc(sizeof(struct wlr_box));
	if(!box) {
		wlr_log(WLR_ERROR, "Failed to allocate box required to focus tile");
		return;
	}
	box->x = tile->tile.x + tile->tile.width / 2;
	box->y = tile->tile.y + tile->tile.height / 2;
	message_printf_pos(ws->output, box, CG_MESSAGE_CENTER, "Current tile");
}

void
workspace_free_tiles(struct cg_workspace *workspace) {
	workspace->focused_tile->prev->next = NULL;
	while(workspace->focused_tile != NULL) {
		if(workspace->output->server->running &&
		   workspace->output->server->seat->cursor_tile ==
		       workspace->focused_tile) {
			workspace->output->server->seat->cursor_tile = NULL;
		}
		if(workspace->output->server->running &&
		   workspace->focused_tile == workspace->server->seat->cursor_tile) {
			workspace->server->seat->cursor_tile = NULL;
		}
		struct cg_tile *next = workspace->focused_tile->next;
		free(workspace->focused_tile);
		workspace->focused_tile = next;
	}
}

void
workspace_free(struct cg_workspace *workspace) {
	workspace_free_tiles(workspace);
	free(workspace);
}

void
workspace_focus(struct cg_output *outp, int ws) {
	if(ws >= outp->server->nws) {
		wlr_log(WLR_ERROR,
		        "Attempt to focus workspace %d, but only %d workspaces are "
		        "available.",
		        ws, outp->server->nws);
		return;
	}
	wlr_scene_node_place_above(
	    &outp->bg->node, &outp->workspaces[outp->curr_workspace]->scene->node);
	wlr_scene_node_place_above(&outp->workspaces[ws]->scene->node,
	                           &outp->bg->node);
	outp->curr_workspace = ws;
}
