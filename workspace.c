/*
 * Cagebreak: A Wayland tiling compositor.
 *
 * Copyright (C) 2020 The Cagebreak Authors
 * Copyright (C) 2018-2019 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_output_layout.h>

#include "message.h"
#include "output.h"
#include "server.h"
#include "view.h"
#include "workspace.h"

void
full_screen_workspace_tiles(const struct cg_output *output,
                            struct cg_workspace *workspace) {
	workspace->server = output->server;
	workspace->focused_tile = calloc(1, sizeof(struct cg_tile));
	workspace->focused_tile->workspace = workspace;
	workspace->focused_tile->next = workspace->focused_tile;
	workspace->focused_tile->prev = workspace->focused_tile;
	workspace->focused_tile->tile.x = 0;
	workspace->focused_tile->tile.y = 0;
	struct wlr_box *output_box = wlr_output_layout_get_box(
	    output->server->output_layout, output->wlr_output);
	workspace->focused_tile->tile.width = output_box->width;
	workspace->focused_tile->tile.height = output_box->height;
	workspace->focused_tile->view = NULL;
}

struct cg_workspace *
full_screen_workspace(struct cg_output *output) {
	struct cg_workspace *workspace = calloc(1, sizeof(struct cg_workspace));
	full_screen_workspace_tiles(output, workspace);
	workspace->output = output;
	return workspace;
}

void
workspace_focus_tile(struct cg_workspace *ws, struct cg_tile *tile) {
	ws->focused_tile = tile;
	struct wlr_box *box = malloc(sizeof(struct wlr_box));
	box->x = tile->tile.x + tile->tile.width / 2;
	box->y = tile->tile.y + tile->tile.height / 2;
	message_printf_pos(ws->output, box, CG_MESSAGE_CENTER, "Current frame");
}

void
workspace_free_tiles(struct cg_workspace *workspace) {
	workspace->focused_tile->prev->next = NULL;
	while(workspace->focused_tile != NULL) {
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
