/*
 * Cagebreak: A Wayland tiling compositor.
 *
 * Copyright (C) 2020-2022 The Cagebreak Authors
 * Copyright (C) 2018-2019 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

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
#include "workspace.h"

#if CG_HAS_FANALYZE
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
int
full_screen_workspace_tiles(struct wlr_output_layout *layout,
                            struct wlr_output *output,
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
	struct wlr_box *output_box = wlr_output_layout_get_box(layout, output);
	workspace->focused_tile->tile.width = output_box->width;
	workspace->focused_tile->tile.height = output_box->height;
	workspace->focused_tile->view = NULL;
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
		return NULL;
	}
	workspace->server = output->server;
	workspace->num = -1;
	workspace->scene = wlr_scene_tree_create(&scene_output->scene->node);
	if(full_screen_workspace_tiles(output->server->output_layout,
	                               output->wlr_output, workspace,
	                               &output->server->tiles_curr_id) != 0) {
		free(workspace);
		return NULL;
	}
	workspace->output = output;
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
	message_printf_pos(ws->output, box, CG_MESSAGE_CENTER, "Current frame");
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
	wlr_scene_node_place_above(
	    &outp->bg->node, &outp->workspaces[outp->curr_workspace]->scene->node);
	wlr_scene_node_place_above(&outp->workspaces[ws]->scene->node,
	                           &outp->bg->node);
	outp->curr_workspace = ws;
}
