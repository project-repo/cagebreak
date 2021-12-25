#ifndef CG_WORKSPACE_H
#define CG_WORKSPACE_H

#include <wlr/types/wlr_box.h>

struct cg_output;
struct cg_server;

struct cg_tile {
	struct cg_workspace *workspace;
	struct wlr_box tile;
	struct cg_view *view;
	struct cg_tile *next;
	struct cg_tile *prev;
	uint32_t id;
};

struct cg_workspace {
	struct cg_server *server;
	struct wl_list views;
	struct wl_list unmanaged_views;
	struct cg_output *output;

	struct cg_tile *focused_tile;
	uint32_t num;
};

struct cg_workspace *
full_screen_workspace(struct cg_output *output);
int
full_screen_workspace_tiles(struct wlr_output_layout *layout,
                            struct wlr_output *output,
                            struct cg_workspace *workspace, uint32_t *tiles_curr_id);
void
workspace_free_tiles(struct cg_workspace *workspace);
void
workspace_free(struct cg_workspace *workspace);
void
workspace_focus_tile(struct cg_workspace *ws, struct cg_tile *tile);

#endif
