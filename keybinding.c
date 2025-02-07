// Copyright 2020 - 2025, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/log.h>

#include "input.h"
#include "input_manager.h"
#include "keybinding.h"
#include "message.h"
#include "output.h"
#include "seat.h"
#include "server.h"
#include "util.h"
#include "view.h"
#include "workspace.h"

char *keybinding_action_string[] = {FOREACH_KEYBINDING(GENERATE_STRING)};

int
keybinding_resize(struct keybinding_list *list) {
	if(list->length == list->capacity) {
		list->capacity *= 2;
		struct keybinding **new_list =
		    realloc(list->keybindings, sizeof(void *) * list->capacity);
		if(new_list == NULL) {
			return -1;
		} else {
			list->keybindings = new_list;
			return 0;
		}
	}
	return 0;
}

struct keybinding **
find_keybinding(const struct keybinding_list *list,
                const struct keybinding *keybinding) {
	struct keybinding **it = list->keybindings;
	for(size_t i = 0; i < list->length; ++i, ++it) {
		if(!(keybinding->modifiers ^ (*it)->modifiers) &&
		   keybinding->mode == (*it)->mode && keybinding->key == (*it)->key) {
			return it;
		}
	}
	return NULL;
}

void
keybinding_free(struct keybinding *keybinding, bool recursive) {
	switch(keybinding->action) {
	case KEYBINDING_DEFINEMODE:
	case KEYBINDING_RUN_COMMAND:
		if(keybinding->data.c != NULL) {
			free(keybinding->data.c);
		}
		break;
	case KEYBINDING_DEFINEKEY:
		if(keybinding->data.kb != NULL && recursive) {
			keybinding_free(keybinding->data.kb, true);
		}
		break;
	case KEYBINDING_CONFIGURE_OUTPUT:
		free(keybinding->data.o_cfg->output_name);
		free(keybinding->data.o_cfg);
		break;
	case KEYBINDING_CONFIGURE_INPUT:
		free(keybinding->data.i_cfg->identifier);
		if(keybinding->data.i_cfg->mapped_from_region) {
			free(keybinding->data.i_cfg->mapped_from_region);
		}
		if(keybinding->data.i_cfg->mapped_to_output) {
			free(keybinding->data.i_cfg->mapped_to_output);
		}
		free(keybinding->data.o_cfg);
		break;
	case KEYBINDING_CONFIGURE_MESSAGE:
		if(keybinding->data.m_cfg->font != NULL) {
			free(keybinding->data.m_cfg->font);
		}
		free(keybinding->data.m_cfg);
		break;
	case KEYBINDING_DISPLAY_MESSAGE:
		if(keybinding->data.c != NULL) {
			free(keybinding->data.c);
		}
		break;
	case KEYBINDING_SEND_CUSTOM_EVENT:
		if(keybinding->data.c != NULL) {
			free(keybinding->data.c);
		}
		break;
	case KEYBINDING_SETMODECURSOR:
		if(keybinding->data.cs[0] != NULL) {
			free(keybinding->data.cs[0]);
		}
		if(keybinding->data.cs[1] != NULL) {
			free(keybinding->data.cs[1]);
		}
	default:
		break;
	}
	free(keybinding);
}

int
keybinding_list_push(struct keybinding_list *list,
                     struct keybinding *keybinding) {

	/* Error resizing list */
	if(keybinding_resize(list) != 0) {
		return -1;
	}

	/*Maintain that only a single keybinding for a key, modifier and mode may
	 * exist*/
	struct keybinding **found_keybinding = find_keybinding(list, keybinding);
	if(found_keybinding != NULL) {
		keybinding_free(*found_keybinding, true);
		*found_keybinding = keybinding;
		wlr_log(WLR_DEBUG, "A keybinding was found twice in the config file.");
	} else {
		list->keybindings[list->length] = keybinding;
		++list->length;
	}
	return 0;
}

struct keybinding_list *
keybinding_list_init(void) {
	struct keybinding_list *list = malloc(sizeof(struct keybinding_list));
	list->keybindings = malloc(sizeof(struct keybinding *));
	list->capacity = 1;
	list->length = 0;
	return list;
}

void
keybinding_list_free(struct keybinding_list *list) {
	if(!list) {
		return;
	}
	for(unsigned int i = 0; i < list->length; ++i) {
		keybinding_free(list->keybindings[i], true);
	}
	free(list->keybindings);
	free(list);
}

// Returns whether x is between a and b (a exclusive, b exclusive) where
// a<b
bool
is_between_strict(int a, int b, int x) {
	return a < x && x < b;
}

struct cg_tile *
tile_from_id(struct cg_server *server, uint32_t id) {
	struct cg_tile *tile = NULL;
	struct cg_output *outp_it;
	wl_list_for_each(outp_it, &server->outputs, link) {
		for(unsigned int i = 0; i < server->nws && tile == NULL; ++i) {
			bool first = true;
			for(struct cg_tile *tile_it = outp_it->workspaces[i]->focused_tile;
			    first || tile_it != outp_it->workspaces[i]->focused_tile;
			    tile_it = tile_it->next) {
				first = false;
				if(id == tile_it->id) {
					tile = tile_it;
					break;
				}
			}
		}
		if(tile != NULL) {
			break;
		}
	}
	return tile;
}

struct cg_view *
view_from_id(struct cg_server *server, uint32_t id) {
	struct cg_view *view = NULL;
	struct cg_output *outp_it;
	wl_list_for_each(outp_it, &server->outputs, link) {
		for(unsigned int i = 0; i < server->nws && view == NULL; ++i) {
			struct cg_view *view_it;
			wl_list_for_each(view_it, &outp_it->workspaces[i]->views, link) {
				if(id == view_it->id) {
					view = view_it;
					break;
				}
			}
		}
		if(view != NULL) {
			break;
		}
	}
	return view;
}

struct cg_output *
output_from_num(struct cg_server *server, int num) {
	struct cg_output *it;
	int count = 1;
	wl_list_for_each(it, &server->outputs, link) {
		if(count == num) {
			return it;
		}
		++count;
	}
	return NULL;
}

struct cg_tile *
find_right_tile(const struct cg_tile *tile) {
	struct cg_tile *it = tile->next;
	int center = tile->tile.y + tile->tile.height / 2;
	while(it != tile) {
		if(it->tile.x == tile->tile.x + tile->tile.width) {
			if(is_between_strict(it->tile.y, it->tile.y + it->tile.height,
			                     center) ||
			   it->tile.y + it->tile.height == center) {
				return it;
			}
		}
		it = it->next;
	}
	return NULL;
}

struct cg_tile *
find_left_tile(const struct cg_tile *tile) {
	struct cg_tile *it = tile->next;
	int center = tile->tile.y + tile->tile.height / 2;
	while(it != tile) {
		if(it->tile.x + it->tile.width == tile->tile.x) {
			if(is_between_strict(it->tile.y, it->tile.y + it->tile.height,
			                     center) ||
			   it->tile.y + it->tile.height == center) {
				return it;
			}
		}
		it = it->next;
	}
	return NULL;
}

struct cg_tile *
find_top_tile(const struct cg_tile *tile) {
	struct cg_tile *it = tile->next;
	int center = tile->tile.x + tile->tile.width / 2;
	while(it != tile) {
		if(it->tile.y + it->tile.height == tile->tile.y) {
			if(is_between_strict(it->tile.x, it->tile.x + it->tile.width,
			                     center) ||
			   it->tile.x + it->tile.width == center) {
				return it;
			}
		}
		it = it->next;
	}
	return NULL;
}

struct cg_tile *
find_bottom_tile(const struct cg_tile *tile) {
	struct cg_tile *it = tile->next;
	int center = tile->tile.x + tile->tile.width / 2;
	while(it != tile) {
		if(it->tile.y == tile->tile.y + tile->tile.height) {
			if(is_between_strict(it->tile.x, it->tile.x + it->tile.width,
			                     center) ||
			   it->tile.x + it->tile.width == center) {
				return it;
			}
		}
		it = it->next;
	}
	return NULL;
}

int *
get_compl_coord(struct cg_tile *tile, int *(*get_coord)(struct cg_tile *tile)) {
	if(get_coord(tile) == &tile->tile.x) {
		return &tile->tile.y;
	} else {
		return &tile->tile.x;
	}
}

int *
get_compl_dim(struct cg_tile *tile, int *(*get_dim)(struct cg_tile *tile)) {
	if(get_dim(tile) == &tile->tile.height) {
		return &tile->tile.width;
	} else {
		return &tile->tile.height;
	}
}

/* find_tile is the direction in which to search, get_dim returns the dimension
 * which must be equal for merging to be possible and get_coord returns the
 * coordinate which must be equal for merging to be possible. */
void
merge_tile(struct cg_tile *tile,
           struct cg_tile *(*find_tile)(const struct cg_tile *),
           int *(*get_dim)(struct cg_tile *tile),
           int *(*get_coord)(struct cg_tile *tile)) {
	struct cg_tile *merge_tile = find_tile(tile);
	if(merge_tile == NULL || merge_tile == tile ||
	   *get_dim(tile) != *get_dim(merge_tile) ||
	   *get_coord(tile) != *get_coord(merge_tile)) {
		return;
	}
	// There are at least two tiles once we reach this point, since merge_tile
	// != tile
	int merge_tile_id = merge_tile->id;
	workspace_tile_update_view(merge_tile, NULL);
	merge_tile->prev->next = merge_tile->next;
	merge_tile->next->prev = merge_tile->prev;
	if(merge_tile->workspace->focused_tile == merge_tile) {
		merge_tile->workspace->focused_tile = tile;
	}
	*get_compl_dim(tile, get_dim) =
	    *get_compl_dim(merge_tile, get_dim) + *get_compl_dim(tile, get_dim);
	*get_compl_coord(tile, get_coord) =
	    fmin(*get_compl_coord(merge_tile, get_coord),
	         *get_compl_coord(tile, get_coord));
	if(tile->workspace->server->seat->cursor_tile == merge_tile) {
		tile->workspace->server->seat->cursor_tile = tile;
	}
	free(merge_tile);
	if(tile->view != NULL) {
		view_maximize(tile->view, tile);
	}
	ipc_send_event(
	    tile->workspace->output->server,
	    "{\"event_name\":\"merge_tile\",\"tile_id\":%d,\"merge_tile_id\":%d,"
	    "\"workspace\":%d,\"output\":\"%s\",\"output_id\":%d}",
	    tile->id, merge_tile_id, tile->workspace->num + 1,
	    tile->workspace->output->name, output_get_num(tile->workspace->output));
}

void
keybinding_focus_tile(struct cg_server *server, uint32_t tile_id);

void
swap_tiles(struct cg_tile *tile, struct cg_tile *swap_tile, bool follow) {
	if(swap_tile == NULL || tile == NULL || swap_tile == tile) {
		return;
	}
	struct cg_server *server = tile->workspace->server;
	struct cg_view *tmp_view = tile->view;
	struct cg_view *tmp_swap_view = swap_tile->view;
	workspace_tile_update_view(tile, NULL);
	workspace_tile_update_view(swap_tile, tmp_view);
	workspace_tile_update_view(tile, tmp_swap_view);
	if(follow) {
		keybinding_focus_tile(server, swap_tile->id);
	} else {
		seat_set_focus(
		    server->seat,
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile->view);
	}
	ipc_send_event(
	    tile->workspace->output->server,
	    "{\"event_name\":\"swap_tile\",\"tile_id\":%d,\"swap_"
	    "tile_id\":%d,\"workspace\":%d,\"output\":\"%s\",\"output_id\":%d}",
	    tile->id, swap_tile->id, tile->workspace->num + 1,
	    tile->workspace->output->name, output_get_num(tile->workspace->output));
}

void
swap_tile(struct cg_server *server, uint32_t tile_id,
          struct cg_tile *(*find_tile)(const struct cg_tile *), bool follow) {
	struct cg_tile *tile = NULL;
	if(tile_id == 0) {
		tile =
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile;
	} else {
		tile = tile_from_id(server, tile_id);
	}
	if(tile != NULL) {
		struct cg_tile *swap_tile = find_tile(tile);
		swap_tiles(tile, swap_tile, follow);
	}
}

int *
get_width(struct cg_tile *tile) {
	return &tile->tile.width;
}

int *
get_height(struct cg_tile *tile) {
	return &tile->tile.height;
}

int *
get_x(struct cg_tile *tile) {
	return &tile->tile.x;
}

int *
get_y(struct cg_tile *tile) {
	return &tile->tile.y;
}

void
merge_tile_left(struct cg_tile *tile) {
	merge_tile(tile, find_left_tile, get_height, get_y);
}

void
merge_tile_right(struct cg_tile *tile) {
	merge_tile(tile, find_right_tile, get_height, get_y);
}

void
merge_tile_top(struct cg_tile *tile) {
	merge_tile(tile, find_top_tile, get_width, get_x);
}

void
merge_tile_bottom(struct cg_tile *tile) {
	merge_tile(tile, find_bottom_tile, get_width, get_x);
}

void
swap_tile_left(struct cg_server *server, uint32_t tile_id, bool follow) {
	swap_tile(server, tile_id, find_left_tile, follow);
}

void
swap_tile_right(struct cg_server *server, uint32_t tile_id, bool follow) {
	swap_tile(server, tile_id, find_right_tile, follow);
}

void
swap_tile_top(struct cg_server *server, uint32_t tile_id, bool follow) {
	swap_tile(server, tile_id, find_top_tile, follow);
}

void
swap_tile_bottom(struct cg_server *server, uint32_t tile_id, bool follow) {
	swap_tile(server, tile_id, find_bottom_tile, follow);
}

void
focus_tile(struct cg_tile *tile,
           struct cg_tile *(*find_tile)(const struct cg_tile *tile)) {
	struct cg_tile *new_tile = find_tile(tile);
	if(new_tile == NULL) {
		return;
	}
	struct cg_server *server = new_tile->workspace->server;
	workspace_focus_tile(tile->workspace, new_tile);
	seat_set_focus(server->seat, new_tile->view);
}

void
focus_tile_left(struct cg_tile *tile) {
	focus_tile(tile, find_left_tile);
}

void
focus_tile_right(struct cg_tile *tile) {
	focus_tile(tile, find_right_tile);
}

void
focus_tile_top(struct cg_tile *tile) {
	focus_tile(tile, find_top_tile);
}

void
focus_tile_bottom(struct cg_tile *tile) {
	focus_tile(tile, find_bottom_tile);
}

bool
intervalls_intersect(int x1, int x2, int y1, int y2) {
	return y2 > x1 && y1 < x2;
}

bool
resize_allowed(struct cg_tile *tile, const struct cg_tile *parent,
               int coord_offset, int dim_offset,
               int *(*get_coord)(struct cg_tile *tile),
               int *(*get_dim)(struct cg_tile *tile), struct cg_tile *orig) {

	if(coord_offset == 0 && dim_offset == 0) {
		return true;
	} else if(*get_dim(tile) + dim_offset <= 0) {
		return false;
	}

	for(struct cg_tile *it = tile->next; it != tile && it != NULL;
	    it = it->next) {
		if(it == parent || it == orig) {
			continue;
		}
		if(intervalls_intersect(*get_compl_coord(tile, get_coord),
		                        *get_compl_coord(tile, get_coord) +
		                            *get_compl_dim(tile, get_dim),
		                        *get_compl_coord(it, get_coord),
		                        *get_compl_coord(it, get_coord) +
		                            *get_compl_dim(it, get_dim))) {
			if(*get_coord(it) == *get_coord(tile) + *get_dim(tile)) {
				if(!resize_allowed(it, tile, dim_offset + coord_offset,
				                   -dim_offset - coord_offset, get_coord,
				                   get_dim, orig)) {
					return false;
				}
			} else if(*get_coord(it) + *get_dim(it) == *get_coord(tile)) {
				if(!resize_allowed(it, tile, 0, coord_offset, get_coord,
				                   get_dim, orig)) {
					return false;
				}
			}
		}
	}
	return true;
}

void
resize(struct cg_tile *tile, const struct cg_tile *parent, int coord_offset,
       int dim_offset, int *(*get_coord)(struct cg_tile *tile),
       int *(*get_dim)(struct cg_tile *tile), struct cg_tile *orig) {
	if(coord_offset == 0 && dim_offset == 0) {
		return;
	}

	for(struct cg_tile *it = tile->next; it != tile && it != NULL;
	    it = it->next) {
		if(it == parent || it == orig) {
			continue;
		}
		if(intervalls_intersect(*get_compl_coord(tile, get_coord),
		                        *get_compl_coord(tile, get_coord) +
		                            *get_compl_dim(tile, get_dim),
		                        *get_compl_coord(it, get_coord),
		                        *get_compl_coord(it, get_coord) +
		                            *get_compl_dim(it, get_dim))) {
			if(*get_coord(it) == *get_coord(tile) + *get_dim(tile)) {
				resize(it, tile, dim_offset + coord_offset,
				       -dim_offset - coord_offset, get_coord, get_dim, orig);
			} else if(*get_coord(it) + *get_dim(it) == *get_coord(tile)) {
				resize(it, tile, 0, coord_offset, get_coord, get_dim, orig);
			}
		}
	}

	int old_x = tile->tile.x, old_y = tile->tile.y,
	    old_height = tile->tile.height, old_width = tile->tile.width;
	*get_coord(tile) += coord_offset;
	*get_dim(tile) += dim_offset;

	if(tile->view != NULL) {
		view_maximize(tile->view, tile);
	}
	ipc_send_event(tile->workspace->output->server,
	               "{\"event_name\":\"resize_tile\",\"tile_id\":%d,\"old_"
	               "dims\":\"[%d,%d,%d,%d]\",\"new_dims\":\"[%d,%d,%d,%d]\","
	               "\"workspace\":%d,\"output\":\"%s\",\"output_id\":%d}",
	               tile->id, old_x, old_y, old_height, old_width, tile->tile.x,
	               tile->tile.y, tile->tile.height, tile->tile.width,
	               tile->workspace->num + 1, tile->workspace->output->name,
	               output_get_num(tile->workspace->output));
}

bool
resize_allowed_horizontal(struct cg_tile *tile, struct cg_tile *parent,
                          int x_offset, int width_offset) {
	return resize_allowed(tile, parent, x_offset, width_offset, get_x,
	                      get_width, tile);
}

bool
resize_allowed_vertical(struct cg_tile *tile, struct cg_tile *parent,
                        int y_offset, int height_offset) {
	return resize_allowed(tile, parent, y_offset, height_offset, get_y,
	                      get_height, tile);
}

void
resize_horizontal(struct cg_tile *tile, struct cg_tile *parent, int x_offset,
                  int width_offset) {
	resize(tile, parent, x_offset, width_offset, get_x, get_width, tile);
}

void
resize_vertical(struct cg_tile *tile, struct cg_tile *parent, int y_offset,
                int height_offset) {
	resize(tile, parent, y_offset, height_offset, get_y, get_height, tile);
}

/* hpixs: positiv -> right, negative -> left; vpixs: positiv -> down, negative
 * -> up */
void
resize_tile(struct cg_server *server, int hpixs, int vpixs, int tile_id) {
	struct cg_output *output = server->curr_output;

	struct cg_tile *tile =
	    output->workspaces[output->curr_workspace]->focused_tile;
	if(tile_id != 0) {
		tile = tile_from_id(server, tile_id);
	}
	if(tile == NULL) {
		return;
	}
	/* First do the horizontal adjustment */
	if(hpixs != 0 && tile->tile.width < output_get_layout_box(output).width &&
	   is_between_strict(0, output_get_layout_box(output).width,
	                     tile->tile.width + hpixs)) {
		int x_offset = 0;
		/* In case we are on the total right, move the left edge of the tile */
		if(tile->tile.x + tile->tile.width ==
		   output_get_layout_box(output).width) {
			x_offset = -hpixs;
		}
		bool resize_allowed =
		    resize_allowed_horizontal(tile, NULL, x_offset, hpixs);
		if(resize_allowed) {
			resize_horizontal(tile, NULL, x_offset, hpixs);
		}
	}
	/* Repeat for vertical */
	if(vpixs != 0 && tile->tile.height < output_get_layout_box(output).height &&
	   is_between_strict(0, output_get_layout_box(output).height,
	                     tile->tile.height + vpixs)) {
		int y_offset = 0;
		if(tile->tile.y + tile->tile.height ==
		   output_get_layout_box(output).height) {
			y_offset = -vpixs;
		}
		bool resize_allowed =
		    resize_allowed_vertical(tile, NULL, y_offset, vpixs);
		if(resize_allowed) {
			resize_vertical(tile, NULL, y_offset, vpixs);
		}
	}
}

void
keybinding_workspace_fullscreen(struct cg_server *server, uint32_t screen,
                                uint32_t workspace) {
	struct cg_output *output = server->curr_output;
	uint32_t ws = output->curr_workspace;
	if(screen != 0) {
		output = output_from_num(server, screen);
		ws = workspace;
	}
	if(output == NULL || ws >= server->nws) {
		return;
	}
	output_make_workspace_fullscreen(output, ws);
	ipc_send_event(server,
	               "{\"event_name\":\"fullscreen\",\"tile_id\":%d,"
	               "\"workspace\":%d,\"output\":\"%s\",\"output_id\":%d}",
	               output->workspaces[ws]->focused_tile->id,
	               output->workspaces[ws]->num + 1, output->name,
	               output_get_num(output));
}

// Switch to a differerent virtual terminal
static int
keybinding_switch_vt(struct cg_server *server, unsigned int vt) {
	if(wlr_backend_is_multi(server->backend)) {
		if(server->session) {
			wlr_session_change_vt(server->session, vt);
		}
		return 0;
	}
	return -1;
}

/* Split screen (vertical or horizontal)
 * Important: Do not attempt to perform mathematical simplifications in this
 * function without taking rounding errors into account. */
static void
keybinding_split_output(struct cg_output *output, bool vertical,
                        float percentage) {
	struct cg_view *original_view = seat_get_focus(output->server->seat);
	struct cg_workspace *curr_workspace =
	    output->workspaces[output->curr_workspace];
	int32_t width = curr_workspace->focused_tile->tile.width;
	int32_t height = curr_workspace->focused_tile->tile.height;
	int32_t x = curr_workspace->focused_tile->tile.x;
	int32_t y = curr_workspace->focused_tile->tile.y;
	int32_t new_width, new_height;

	if(vertical) {
		new_width = (int)(((float)width) * percentage);
		new_height = height;
	} else {
		new_width = width;
		new_height = (int)(((float)height) * percentage);
	}
	if(new_width < 1 || new_height < 1) {
		return;
	}

	struct cg_view *next_view = NULL;
	struct cg_view *it;
	wl_list_for_each(it, &output->workspaces[output->curr_workspace]->views,
	                 link) {
		if(!view_is_visible(it) && original_view != it) {
			next_view = it;
			break;
		}
	}

	int32_t new_x, new_y;
	if(vertical) {
		new_x = x + new_width;
		new_y = y;
	} else {
		new_x = x;
		new_y = y + new_height;
	}

	struct cg_tile *new_tile = calloc(1, sizeof(struct cg_tile));
	if(!new_tile) {
		wlr_log(WLR_ERROR, "Failed to allocate new tile for splitting");
		return;
	}
	new_tile->id = output->server->tiles_curr_id;
	++output->server->tiles_curr_id;
	new_tile->tile.x = new_x;
	new_tile->tile.y = new_y;
	new_tile->tile.width = x + width - new_x;
	new_tile->tile.height = y + height - new_y;
	new_tile->prev = curr_workspace->focused_tile;
	new_tile->next = curr_workspace->focused_tile->next;
	workspace_tile_update_view(new_tile, next_view);
	new_tile->workspace = curr_workspace;
	curr_workspace->focused_tile->next->prev = new_tile;
	curr_workspace->focused_tile->next = new_tile;

	curr_workspace->focused_tile->tile.width = new_width;
	curr_workspace->focused_tile->tile.height = new_height;
	workspace_focus_tile(curr_workspace, curr_workspace->focused_tile);

	if(next_view != NULL) {
		view_maximize(next_view, new_tile);
	}

	if(original_view != NULL) {
		view_maximize(original_view, curr_workspace->focused_tile);
	}
	ipc_send_event(
	    output->server,
	    "{\"event_name\":\"split\",\"tile_id\":%d,\"new_tile_id\":%d,"
	    "\"workspace\":%d,\"output\":\"%s\",\"output_id\":%d,\"vertical\":%d}",
	    curr_workspace->focused_tile->id, new_tile->id, curr_workspace->num + 1,
	    curr_workspace->output->name, output_get_num(curr_workspace->output),
	    vertical);
}

static void
keybinding_close_view(struct cg_view *view) {
	if(view == NULL) {
		return;
	}
	struct cg_output *outp = view->workspace->output;
	uint32_t view_id = view->id;
	uint32_t view_pid = view->impl->get_pid(view);
	uint32_t tile_id = view->id;
	uint32_t ws = view->workspace->num;
	view->impl->close(view);
	ipc_send_event(
	    outp->server,
	    "{\"event_name\":\"close\",\"view_id\":%d,\"view_pid\":%d,\"tile_id\":"
	    "%d,\"workspace\":%d,\"output\":\"%s\",\"output_id\":%d}",
	    view_id, view_pid, tile_id, ws + 1, outp->name, output_get_num(outp));
}

static void
keybinding_split_vertical(struct cg_server *server, float percentage) {
	keybinding_split_output(server->curr_output, true, percentage);
}

static void
keybinding_split_horizontal(struct cg_server *server, float percentage) {
	keybinding_split_output(server->curr_output, false, percentage);
}

static void
into_process(const char *command) {
	execlp("sh", "sh", "-c", command, (char *)NULL);
	_exit(1);
}

void
set_output(struct cg_server *server, struct cg_output *output) {
	server->curr_output = output;
	seat_set_focus(
	    server->seat,
	    server->curr_output->workspaces[server->curr_output->curr_workspace]
	        ->focused_tile->view);
	message_printf(server->curr_output, "Current Output");
}

void
keybinding_cycle_outputs(struct cg_server *server, bool reverse,
                         bool trigger_event) {
	struct cg_output *output = NULL;
	struct cg_output *old_output = server->curr_output;
	if(reverse) {
		output = wl_container_of(server->curr_output->link.prev,
		                         server->curr_output, link);
	} else {
		output = wl_container_of(server->curr_output->link.next,
		                         server->curr_output, link);
	}
	if(&output->link == &server->outputs) {
		if(reverse) {
			output = wl_container_of(output->link.prev, output, link);
		} else {
			output = wl_container_of(output->link.next, output, link);
		}
	}
	set_output(server, output);
	if(trigger_event) {
		ipc_send_event(
		    output->server,
		    "{\"event_name\":\"cycle_outputs\",\"old_output\":\"%s\",\"old_"
		    "output_id\":%d,"
		    "\"new_output\":\"%s\",\"new_output_id\":%d,\"reverse\":%d}",
		    old_output->name, output_get_num(old_output), output->name,
		    output_get_num(output), reverse);
	}
}

/* Cycle through views, whereby the workspace does not change */
void
keybinding_cycle_views(struct cg_server *server, struct cg_tile *tile,
                       uint32_t view_id, bool reverse, bool ipc) {
	if(tile == NULL) {
		tile =
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile;
	}
	struct cg_view *current_view = tile->view;
	struct cg_workspace *ws = tile->workspace;

	struct cg_view *it_view, *next_view = NULL;
	if(view_id == 0) {
		if(reverse) {
			wl_list_for_each(it_view, &ws->views, link) {
				if(!view_is_visible(it_view)) {
					next_view = it_view;
					break;
				}
			}
		} else {
			wl_list_for_each_reverse(it_view, &ws->views, link) {
				if(!view_is_visible(it_view)) {
					next_view = it_view;
					break;
				}
			}
		}
	} else {
		next_view = view_from_id(server, view_id);
		if(next_view == NULL || view_is_visible(next_view)) {
			return;
		}
	}

	if(next_view == NULL) {
		return;
	}

	workspace_tile_update_view(tile, next_view);
	if(tile ==
	   server->curr_output->workspaces[server->curr_output->curr_workspace]
	       ->focused_tile) {
		seat_set_focus(server->seat, next_view);
	}
	if(ipc) {
		int curr_id = -1;
		int curr_pid = -1;
		if(current_view != NULL && current_view->link.next != ws->views.next) {
			curr_id = current_view->id;
			curr_pid = current_view->impl->get_pid(current_view);
		}
		ipc_send_event(
		    ws->output->server,
		    "{\"event_name\":\"cycle_views\",\"old_view_id\":%d,\"old_view_"
		    "pid\":%d,"
		    "\"new_view_id\":%d,\"new_view_pid\":%d,\"tile_id\":%d,"
		    "\"workspace\":%d,\"output\":\"%s\",\"output_id\":%d}",
		    curr_id, curr_pid, next_view == NULL ? -1 : (int)next_view->id,
		    next_view == NULL ? -1 : (int)next_view->impl->get_pid(next_view),
		    next_view->tile->id, ws->num + 1, ws->output->name,
		    output_get_num(ws->output));
	}
}

int
keybinding_switch_ws(struct cg_server *server, uint32_t ws) {
	if(ws >= server->nws) {
		wlr_log(WLR_ERROR,
		        "Requested workspace %u, but only have %u workspaces.", ws + 1,
		        server->nws);
		return -1;
	}
	struct cg_output *output = server->curr_output;
	uint32_t old_ws = server->curr_output->curr_workspace;
	workspace_focus(output, ws);
	seat_set_focus(server->seat,
	               server->curr_output->workspaces[ws]->focused_tile->view);
	message_printf(server->curr_output, "Workspace %d", ws + 1);
	ipc_send_event(output->server,
	               "{\"event_name\":\"switch_ws\",\"old_workspace\":%d,"
	               "\"new_workspace\":%d,\"output\":\"%s\",\"output_id\":%d}",
	               old_ws + 1, ws + 1, output->name, output_get_num(output));
	return 0;
}

void
keybinding_focus_tile(struct cg_server *server, uint32_t tile_id) {
	struct cg_output *output = server->curr_output;
	struct cg_workspace *workspace = output->workspaces[output->curr_workspace];
	struct cg_tile *old_tile = workspace->focused_tile;
	struct cg_tile *tile = tile_from_id(server, tile_id);
	if(tile == NULL) {
		return;
	}
	if(server->curr_output != tile->workspace->output) {
		set_output(server, tile->workspace->output);
	}
	if(server->curr_output->curr_workspace != (int)tile->workspace->num) {
		keybinding_switch_ws(server, tile->workspace->num);
	}
	workspace_focus_tile(tile->workspace, tile);
	struct cg_view *next_view = tile->workspace->focused_tile->view;
	seat_set_focus(server->seat, next_view);
	ipc_send_event(
	    output->server,
	    "{\"event_name\":\"focus_tile\",\"old_tile_id\":%d,\"new_tile_"
	    "id\":%d,\"old_workspace\":%d,\"new_workspace\":%d,\"old_output\":\"%"
	    "s\",\"old_output_id\":%d,\"output\":\"%s\",\"output_id\":%d}",
	    old_tile->id, tile->workspace->focused_tile->id, workspace->num + 1,
	    tile->workspace->num + 1, output->name, output_get_num(output),
	    tile->workspace->output->name, output_get_num(tile->workspace->output));
}

void
keybinding_cycle_tiles(struct cg_server *server, bool reverse) {
	struct cg_output *output = server->curr_output;
	struct cg_workspace *workspace = output->workspaces[output->curr_workspace];
	if(reverse) {
		keybinding_focus_tile(server, workspace->focused_tile->prev->id);
	} else {
		keybinding_focus_tile(server, workspace->focused_tile->next->id);
	}
}

void
keybinding_show_time(struct cg_server *server) {
	char *msg, *tmp;
	time_t timep;

	timep = time(NULL);
	tmp = ctime(&timep);
	msg = strdup(tmp);
	msg[strcspn(msg, "\n")] = '\0'; /* Remove the newline */

	message_printf(server->curr_output, "%s", msg);
	free(msg);
}

struct dyn_str {
	uint32_t len;
	uint32_t cur_pos;
	char **str_arr;
};

/*Note that inp is in an invalid state after calling the function * and should
 * no longer be used.*/
char *
dyn_str_to_str(struct dyn_str *inp) {
	char *outp = calloc(inp->len + 1, sizeof(char));
	if(outp == NULL) {
		return NULL;
	}
	outp[inp->len] = '\0';
	uint32_t tmp_pos = 0;
	for(uint32_t i = 0; i < inp->cur_pos; ++i) {
		strcat(outp + tmp_pos, inp->str_arr[i]);
		tmp_pos += strlen(inp->str_arr[i]);
		free(inp->str_arr[i]);
	}
	free(inp->str_arr);
	return outp;
}

int
print_str(struct dyn_str *outp, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	char *ret = malloc_vsprintf_va_list(fmt, args);
	if(ret == NULL) {
		return -1;
	}
	outp->str_arr[outp->cur_pos] = ret;
	++outp->cur_pos;
	outp->len += strlen(ret);
	return 0;
}

char *
print_message_conf(struct cg_message_config *config) {
	struct dyn_str outp_str;
	outp_str.len = 0;
	outp_str.cur_pos = 0;
	uint32_t nmemb = 8;
	outp_str.str_arr = calloc(nmemb, sizeof(char *));
	print_str(&outp_str, "\"message_config\": {");
	print_str(&outp_str, "\"font\": \"%s\",\n", config->font);
	print_str(&outp_str, "\"display_time\": %d,\n", config->display_time);
	print_str(&outp_str, "\"bg_color\": [%f,%f,%f,%f],\n", config->bg_color[0],
	          config->bg_color[1], config->bg_color[2], config->bg_color[3]);
	print_str(&outp_str, "\"fg_color\": [%f,%f,%f,%f],\n", config->fg_color[0],
	          config->fg_color[1], config->fg_color[2], config->fg_color[3]);
	print_str(&outp_str, "\"enabled\": %d,\n", config->enabled == 1);
	switch(config->anchor) {
	case CG_MESSAGE_TOP_LEFT:
		print_str(&outp_str, "\"anchor\": \"top_left\"\n", config->font);
		break;
	case CG_MESSAGE_TOP_CENTER:
		print_str(&outp_str, "\"anchor\": \"top_center\"\n", config->font);
		break;
	case CG_MESSAGE_TOP_RIGHT:
		print_str(&outp_str, "\"anchor\": \"top_right\"\n", config->font);
		break;
	case CG_MESSAGE_BOTTOM_LEFT:
		print_str(&outp_str, "\"anchor\": \"bottom_left\"\n", config->font);
		break;
	case CG_MESSAGE_BOTTOM_CENTER:
		print_str(&outp_str, "\"anchor\": \"bottom_center\"\n", config->font);
		break;
	case CG_MESSAGE_BOTTOM_RIGHT:
		print_str(&outp_str, "\"anchor\": \"bottom_right\"\n", config->font);
		break;
	case CG_MESSAGE_CENTER:
		print_str(&outp_str, "\"anchor\": \"center\"\n", config->font);
		break;
	case CG_MESSAGE_NOPT: // This should actually never occur
		print_str(&outp_str, "\"anchor\": \"no_op\"\n", config->font);
		break;
	}
	print_str(&outp_str, "}");
	return dyn_str_to_str(&outp_str);
}

void
print_modes(struct dyn_str *str, char **modes) {
	uint32_t len = 0;
	char **tmp = modes;
	uint32_t nmemb = 0;
	while(*tmp != NULL) {
		len += strlen(*tmp);
		++nmemb;
		++tmp;
	}
	if(nmemb == 0) {
		wlr_log(WLR_ERROR,
		        "This is a bug: Cagebreak has no valid modes. This should not "
		        "occur, since default modes are defined on startup.");
		return;
	}
	/* We are assuming here that we have at least one mode, which is given by
	 * the initialization of cagebreak */
	char *modes_str = calloc(len + 3 * (nmemb - 1) + 1, sizeof(char));
	modes_str[len + nmemb - 1] = '\0';
	uint32_t tmp_pos = 0;
	for(uint32_t i = 0; i < nmemb; ++i) {
		if(i != 0) {
			strcat(modes_str + tmp_pos, "\",\"");
			tmp_pos += 3;
		}
		strcat(modes_str + tmp_pos, modes[i]);
		tmp_pos += strlen(modes[i]);
	}
	print_str(str, "\"modes\":[\"%s\"],\n", modes_str);
	free(modes_str);
}

char *
print_view(struct cg_view *view) {
	struct dyn_str outp_str;
	outp_str.len = 0;
	outp_str.cur_pos = 0;
	uint32_t nmemb = 5;
	outp_str.str_arr = calloc(nmemb, sizeof(char *));
	print_str(&outp_str, "\"id\": %d,\n", view->id);
	print_str(&outp_str, "\"pid\": %d,\n", view->impl->get_pid(view));
	if(view->server->bs == true) {
		char *title_str = view->impl->get_title(view);
		print_str(&outp_str, "\"title\": \"%s\",\n",
		          title_str == NULL ? "" : title_str);
	}
	print_str(&outp_str, "\"coords\": {\"x\":%d,\"y\":%d},\n", view->ox,
	          view->oy);
#if CG_HAS_XWAYLAND
	print_str(&outp_str, "\"type\": \"%s\"\n",
	          view->type == CG_XWAYLAND_VIEW ? "xwayland" : "xdg");
#else
	print_str(&outp_str, "\"type\": \"xdg\"\n");
#endif
	return dyn_str_to_str(&outp_str);
}

char *
print_tile(struct cg_tile *tile) {
	struct dyn_str outp_str;
	outp_str.len = 0;
	outp_str.cur_pos = 0;
	uint32_t nmemb = 4;
	outp_str.str_arr = calloc(nmemb, sizeof(char *));
	print_str(&outp_str, "\"id\": %d,\n", tile->id);
	print_str(&outp_str, "\"coords\": {\"x\":%d,\"y\":%d},\n", tile->tile.x,
	          tile->tile.y);
	print_str(&outp_str, "\"size\": {\"width\":%d,\"height\":%d},\n",
	          tile->tile.width, tile->tile.height);
	print_str(&outp_str, "\"view_id\": %d\n",
	          tile->view == NULL ? -1 : (int)tile->view->id);
	return dyn_str_to_str(&outp_str);
}

char *
print_views(struct cg_workspace *ws) {
	struct dyn_str outp_str;
	outp_str.len = 0;
	outp_str.cur_pos = 0;
	uint32_t nviews = wl_list_length(&ws->views);
	if(nviews == 0) {
		return strdup("{}");
	}
	outp_str.str_arr = calloc(2 * nviews - 1, sizeof(char *));
	struct cg_view *it;
	uint32_t count = 0;
	wl_list_for_each(it, &ws->views, link) {
		if(count != 0) {
			print_str(&outp_str, ",");
		}
		++count;
		char *view_str = print_view(it);
		if(view_str != NULL) {
			print_str(&outp_str, "{\n%s\n}", view_str);
			free(view_str);
		}
	}
	return dyn_str_to_str(&outp_str);
}

char *
print_tiles(struct cg_workspace *ws) {
	struct dyn_str outp_str;
	outp_str.len = 0;
	outp_str.cur_pos = 0;
	uint32_t ntiles = 0;
	bool first = true;
	for(struct cg_tile *tile = ws->focused_tile;
	    first || tile != ws->focused_tile; tile = tile->next) {
		first = false;
		++ntiles;
	}
	if(ntiles == 0) {
		return strdup("{}");
	}
	outp_str.str_arr = calloc(2 * ntiles - 1, sizeof(char *));
	first = true;
	for(struct cg_tile *tile = ws->focused_tile;
	    first || tile != ws->focused_tile; tile = tile->next) {
		if(first == false) {
			print_str(&outp_str, ",");
		}
		first = false;
		char *tile_str = print_tile(tile);
		if(tile_str != NULL) {
			print_str(&outp_str, "{\n%s\n}", tile_str);
			free(tile_str);
		}
	}
	return dyn_str_to_str(&outp_str);
}

char *
print_workspace(struct cg_workspace *ws) {
	struct dyn_str outp_str;
	outp_str.len = 0;
	outp_str.cur_pos = 0;
	uint32_t nmemb = 6;
	outp_str.str_arr = calloc(nmemb, sizeof(char *));
	print_str(&outp_str, "\"views\": [");
	char *views_str = print_views(ws);
	if(views_str != NULL) {
		print_str(&outp_str, "%s", views_str);
		free(views_str);
	}
	print_str(&outp_str, "],");
	print_str(&outp_str, "\"tiles\": [");
	char *tiles_str = print_tiles(ws);
	if(tiles_str != NULL) {
		print_str(&outp_str, "%s", tiles_str);
		free(tiles_str);
	}
	print_str(&outp_str, "]");
	return dyn_str_to_str(&outp_str);
}

char *
print_workspaces(struct cg_output *outp) {
	struct dyn_str outp_str;
	outp_str.len = 0;
	outp_str.cur_pos = 0;
	outp_str.str_arr = calloc((2 * outp->server->nws - 1) + 2, sizeof(char *));
	print_str(&outp_str, "\"workspaces\": [");
	for(int i = 0; i < outp->server->nws; ++i) {
		if(i != 0) {
			print_str(&outp_str, ",");
		}
		char *ws = print_workspace(outp->workspaces[i]);
		if(ws != NULL) {
			print_str(&outp_str, "{%s}", ws);
			free(ws);
		}
	}
	print_str(&outp_str, "]\n");
	return dyn_str_to_str(&outp_str);
}

char *
print_output(struct cg_output *outp) {
	struct dyn_str outp_str;
	outp_str.len = 0;
	outp_str.cur_pos = 0;
	uint32_t nmemb = 10;
	outp_str.str_arr = calloc(nmemb, sizeof(char *));
	print_str(&outp_str, "\"%s\": {\n", outp->name);
	print_str(&outp_str, "\"priority\": %d,\n", outp->priority);
	print_str(&outp_str, "\"coords\": {\"x\":%d,\"y\":%d},\n",
	          output_get_layout_box(outp).x, output_get_layout_box(outp).y);
	print_str(&outp_str, "\"size\": {\"width\":%d,\"height\":%d},\n",
	          outp->wlr_output->width, outp->wlr_output->height);
	print_str(&outp_str, "\"refresh_rate\": %f,\n",
	          (float)outp->wlr_output->refresh / 1000.0);
	print_str(&outp_str, "\"permanent\": %d,\n",
	          outp->role == OUTPUT_ROLE_PERMANENT);
	print_str(&outp_str, "\"active\": %d,\n", !outp->destroyed);
	print_str(&outp_str, "\"curr_workspace\": %d,\n", outp->curr_workspace + 1);
	char *workspaces_str = print_workspaces(outp);
	if(workspaces_str != NULL) {
		print_str(&outp_str, "%s", workspaces_str);
		free(workspaces_str);
	}
	print_str(&outp_str, "}");
	return dyn_str_to_str(&outp_str);
}

char *
print_outputs(struct cg_server *server) {
	uint32_t noutps = wl_list_length(&server->outputs);
	struct dyn_str outp_str;
	outp_str.len = 0;
	outp_str.cur_pos = 0;
	outp_str.str_arr = calloc((2 * noutps - 1) + 2, sizeof(char *));
	print_str(&outp_str, "\"outputs\": {");
	struct cg_output *it;
	uint32_t count = 0;
	wl_list_for_each(it, &server->outputs, link) {
		if(count != 0) {
			print_str(&outp_str, ",");
		}
		++count;
		char *outp = print_output(it);
		if(outp == NULL) {
			continue;
		}
		print_str(&outp_str, "%s", outp);
		free(outp);
	}
	print_str(&outp_str, "}\n");
	return dyn_str_to_str(&outp_str);
}

char *
print_keyboard_group(struct cg_keyboard_group *grp) {
	struct dyn_str outp_str;
	outp_str.len = 0;
	outp_str.cur_pos = 0;
	uint32_t nmemb = 5;
	outp_str.str_arr = calloc(nmemb, sizeof(char *));
	if(grp->identifier != NULL) {
		print_str(&outp_str, "\"%s\": {\n", grp->identifier);
	} else {
		print_str(&outp_str, "\"NULL\": {\n");
	}
	print_str(&outp_str, "\"commands_enabled\": %d,\n",
	          grp->enable_keybindings);
	print_str(&outp_str, "\"repeat_delay\": %d,\n",
	          grp->wlr_group->keyboard.repeat_info.delay);
	print_str(&outp_str, "\"repeat_rate\": %d\n",
	          grp->wlr_group->keyboard.repeat_info.rate);
	print_str(&outp_str, "}");
	return dyn_str_to_str(&outp_str);
}

char *
print_keyboard_groups(struct cg_server *server) {
	uint32_t ninps = wl_list_length(&server->seat->keyboard_groups);
	struct dyn_str outp_str;
	outp_str.len = 0;
	outp_str.cur_pos = 0;
	outp_str.str_arr = calloc((2 * ninps - 1) + 3, sizeof(char *));
	print_str(&outp_str, "\"keyboards\": {");
	struct cg_keyboard_group *it;
	uint32_t count = 0;
	wl_list_for_each(it, &server->seat->keyboard_groups, link) {
		if(count != 0) {
			print_str(&outp_str, ",");
		}
		++count;
		char *outp = print_keyboard_group(it);
		if(outp == NULL) {
			continue;
		}
		print_str(&outp_str, "%s", outp);
		free(outp);
	}
	print_str(&outp_str, "}\n");
	return dyn_str_to_str(&outp_str);
}

char *
print_input_device(struct cg_input_device *dev) {
	struct dyn_str outp_str;
	outp_str.len = 0;
	outp_str.cur_pos = 0;
	uint32_t nmemb = 4;
	outp_str.str_arr = calloc(nmemb, sizeof(char *));
	if(dev->identifier != NULL) {
		print_str(&outp_str, "\"%s\": {\n", dev->identifier);
	} else {
		print_str(&outp_str, "\"NULL\": {\n");
	}
	print_str(&outp_str, "\"is_virtual\": %d,\n", dev->is_virtual);
	print_str(&outp_str, "\"type\": \"%s\"\n",
	          dev->wlr_device->type == WLR_INPUT_DEVICE_POINTER  ? "pointer"
	          : dev->wlr_device->type == WLR_INPUT_DEVICE_SWITCH ? "switch"
	          : dev->wlr_device->type == WLR_INPUT_DEVICE_TABLET_PAD
	              ? "tablet pad"
	          : dev->wlr_device->type == WLR_INPUT_DEVICE_TABLET   ? "tablet"
	          : dev->wlr_device->type == WLR_INPUT_DEVICE_TOUCH    ? "touch"
	          : dev->wlr_device->type == WLR_INPUT_DEVICE_KEYBOARD ? "keyboard"
	                                                               : "unknown");
	print_str(&outp_str, "}");
	return dyn_str_to_str(&outp_str);
}

char *
print_input_devices(struct cg_server *server) {
	uint32_t ninps = wl_list_length(&server->input->devices);
	struct dyn_str outp_str;
	outp_str.len = 0;
	outp_str.cur_pos = 0;
	outp_str.str_arr = calloc((2 * ninps - 1) + 3, sizeof(char *));
	print_str(&outp_str, "\"input_devices\": {");
	struct cg_input_device *it;
	uint32_t count = 0;
	wl_list_for_each(it, &server->input->devices, link) {
		if(count != 0) {
			print_str(&outp_str, ",");
		}
		++count;
		char *outp = print_input_device(it);
		if(outp == NULL) {
			continue;
		}
		print_str(&outp_str, "%s", outp);
		free(outp);
	}
	print_str(&outp_str, "}\n");
	return dyn_str_to_str(&outp_str);
}

char *
get_mode_name(char **modes, unsigned int mode_nr) {
	unsigned int i;
	for(i = 0; i < mode_nr && modes[i] != NULL; ++i) {
	}
	if(modes[i] != NULL) {
		return modes[i];
	} else {
		return "NULL";
	}
}

void
keybinding_dump(struct cg_server *server) {
	struct dyn_str str;
	str.len = 0;
	str.cur_pos = 0;
	uint32_t nmemb = 14;
	str.str_arr = calloc(nmemb, sizeof(char *));

	print_str(&str, "{\"event_name\":\"dump\",");
	print_str(&str, "\"nws\":%d,\n", server->nws);
	print_str(&str, "\"bg_color\":[%f,%f,%f],\n", server->bg_color[0],
	          server->bg_color[1], server->bg_color[2]);
	struct cg_view *focused_view = seat_get_focus(server->seat);
	int curr_view_id = -1, curr_tile_id = -1;
	if(focused_view != NULL) {
		curr_view_id = focused_view->id;
		if(focused_view->tile != NULL) {
			curr_tile_id = focused_view->tile->id;
		}
	}
	print_str(&str, "\"views_curr_id\":%d,\n", curr_view_id);
	print_str(&str, "\"tiles_curr_id\":%d,\n", curr_tile_id);
	print_str(&str, "\"curr_output\":\"%s\",\n", server->curr_output->name);
	print_str(&str, "\"default_mode\":\"%s\",\n",
	          get_mode_name(server->modes, server->seat->default_mode));
	print_modes(&str, server->modes);
	char *message_string = print_message_conf(&server->message_config);
	if(message_string != NULL) {
		print_str(&str, "%s,", message_string);
		free(message_string);
	}
	char *outps_str = print_outputs(server);
	if(outps_str != NULL) {
		print_str(&str, "%s,", outps_str);
		free(outps_str);
	}
	char *keyboards_str = print_keyboard_groups(server);
	if(keyboards_str != NULL) {
		print_str(&str, "%s,", keyboards_str);
		free(keyboards_str);
	}
	char *input_dev_str = print_input_devices(server);
	if(input_dev_str != NULL) {
		print_str(&str, "%s,", input_dev_str);
		free(input_dev_str);
	}
	print_str(&str, "\"cursor_coords\":{\"x\":%f,\"y\":%f}\n",
	          server->seat->cursor->x, server->seat->cursor->y);
	print_str(&str, "}");

	char *send_str = dyn_str_to_str(&str);
	if(send_str == NULL) {
		wlr_log(WLR_ERROR, "Unable to create output string for \"dump\".");
	}
	ipc_send_event(server, send_str);
	free(send_str);
}

void
keybinding_show_info(struct cg_server *server) {
	char *msg = server_show_info(server);

	if(!msg) {
		return;
	}

	message_printf(server->curr_output, "%s", msg);
	free(msg);
}

void
keybinding_display_message(struct cg_server *server, char *msg) {
	message_printf(server->curr_output, "%s", msg);
}

void
keybinding_send_custom_event(struct cg_server *server, char *msg) {
	ipc_send_event(server,
	               "{\"event_name\":\"custom_event\",\"message\":\"%s\"}", msg);
}

void
keybinding_move_view_to_cycle_output(struct cg_server *server, bool reverse) {
	if(wl_list_length(&server->outputs) <= 1) {
		return;
	}
	struct cg_output *old_outp = server->curr_output;
	struct cg_view *view =
	    server->curr_output->workspaces[server->curr_output->curr_workspace]
	        ->focused_tile->view;
	if(view != NULL) {
		wl_list_remove(&view->link);
		server->curr_output->workspaces[server->curr_output->curr_workspace]
		    ->focused_tile->view = NULL;
		keybinding_cycle_views(server, NULL, 0, false, false);
		if(server->curr_output->workspaces[server->curr_output->curr_workspace]
		       ->focused_tile->view == NULL) {
			seat_set_focus(server->seat, NULL);
		}
	}
	keybinding_cycle_outputs(server, reverse, false);
	if(view != NULL) {
		struct cg_workspace *ws =
		    server->curr_output
		        ->workspaces[server->curr_output->curr_workspace];
		wl_list_insert(&ws->views, &view->link);
		wlr_scene_node_reparent(&view->scene_tree->node, ws->scene);
		workspace_tile_update_view(ws->focused_tile, view);
		view->workspace = ws;
		seat_set_focus(server->seat, view);
	}
	int id = -1;
	int pid = -1;
	if(view != NULL) {
		id = view->id;
		pid = view->impl->get_pid(view);
	}
	ipc_send_event(
	    server,
	    "{\"event_name\":\"move_view_to_cycle_output\",\"view_id\":%d,\"view_"
	    "pid\":%d,\"old_output\":\"%s\",\"old_output_id\":%d,\"new_output\":\"%"
	    "s\",\"new_output_id\":%d,\"old_tile_id\":%d,\"new_tile_id\":%d}",
	    id, pid, old_outp->name, output_get_num(old_outp),
	    server->curr_output->name, output_get_num(server->curr_output),
	    old_outp->workspaces[old_outp->curr_workspace]->focused_tile->id,
	    server->curr_output->workspaces[server->curr_output->curr_workspace]
	        ->focused_tile->id);
}

void
keybinding_set_nws(struct cg_server *server, int nws) {
	struct cg_output *output;
	unsigned int old_nws = server->nws;
	server->nws = nws;
	wl_list_for_each(output, &server->outputs, link) {
		for(unsigned int i = nws; i < old_nws; ++i) {
			struct cg_view *view, *tmp;
			wl_list_for_each_safe(view, tmp, &output->workspaces[i]->views,
			                      link) {
				wl_list_remove(&view->link);
				wl_list_insert(&output->workspaces[nws - 1]->views,
				               &view->link);
				view->workspace = output->workspaces[nws - 1];
			}
			wl_list_for_each_safe(
			    view, tmp, &output->workspaces[i]->unmanaged_views, link) {
				wl_list_remove(&view->link);
				wl_list_insert(&output->workspaces[nws - 1]->unmanaged_views,
				               &view->link);
				view->workspace = output->workspaces[nws - 1];
			}
			workspace_free(output->workspaces[i]);
		}
		struct cg_workspace **new_workspaces =
		    realloc(output->workspaces, nws * sizeof(struct cg_workspace *));
		if(new_workspaces == NULL) {
			wlr_log(WLR_ERROR, "Error reallocating memory for workspaces.");
			return;
		}
		output->workspaces = new_workspaces;
		for(int i = old_nws; i < nws; ++i) {
			output->workspaces[i] = full_screen_workspace(output);
			output->workspaces[i]->num = i;
			if(!output->workspaces[i]) {
				wlr_log(WLR_ERROR, "Failed to allocate additional workspaces");
				return;
			}

			wl_list_init(&output->workspaces[i]->views);
			wl_list_init(&output->workspaces[i]->unmanaged_views);
		}

		if(output->curr_workspace >= nws) {
			output->curr_workspace = 0;
			workspace_focus(output, nws - 1);
		}
	}
	seat_set_focus(
	    server->seat,
	    server->curr_output->workspaces[server->curr_output->curr_workspace]
	        ->focused_tile->view);
	ipc_send_event(server,
	               "{\"event_name\":\"set_nws\",\"old_nws\":%d,\"new_nws\":%d}",
	               old_nws, server->nws);
}

void
keybinding_definemode(struct cg_server *server, char *mode) {
	int length = 0;
	while(server->modes[length++] != NULL)
		;
	char **tmp = realloc(server->modes, (length + 1) * sizeof(char *));
	char **tmp2 = realloc(server->modecursors, (length + 1) * sizeof(char *));
	if(tmp == NULL || tmp2 == NULL) {
		if(tmp != NULL) {
			free(tmp);
		}
		if(tmp2 != NULL) {
			free(tmp2);
		}
		wlr_log(WLR_ERROR, "Could not allocate memory for storing modes.");
		return;
	}
	server->modes = tmp;
	server->modecursors = tmp2;
	server->modes[length] = NULL;
	server->modecursors[length] = NULL;

	server->modes[length - 1] = strdup(mode);
	ipc_send_event(server, "{\"event_name\":\"definemode\",\"mode\":\"%s\"}",
	               mode);
}

void
keybinding_definekey(struct cg_server *server, struct keybinding *kb) {
	keybinding_list_push(server->keybindings, kb);
	ipc_send_event(server,
	               "{\"event_name\":\"definekey\",\"modifiers\":%d,\"key\":"
	               "%d,\"command\":\"%s\"}",
	               kb->modifiers, kb->key,
	               keybinding_action_string[kb->action]);
}

void
keybinding_set_background(struct cg_server *server, float *bg) {
	ipc_send_event(server,
	               "{\"event_name\":\"background\",\"old_bg\":[%f,%f,%f],"
	               "\"new_bg\":[%f,%f,%f]}",
	               server->bg_color[0], server->bg_color[1],
	               server->bg_color[2], bg[0], bg[1], bg[2]);
	server->bg_color[0] = bg[0];
	server->bg_color[1] = bg[1];
	server->bg_color[2] = bg[2];
	struct cg_output *it = NULL;
	wl_list_for_each(it, &server->outputs, link) {
		wlr_scene_rect_set_color(it->bg, server->bg_color);
	}
	wl_list_for_each(it, &server->disabled_outputs, link) {
		wlr_scene_rect_set_color(it->bg, server->bg_color);
	}
}

void
keybinding_switch_output(struct cg_server *server, int output) {
	struct cg_output *old_outp = server->curr_output;
	struct cg_output *new_outp = output_from_num(server, output);
	if(new_outp != NULL) {
		set_output(server, new_outp);
		ipc_send_event(server,
		               "{\"event_name\":\"switch_output\",\"old_output\":"
		               "\"%s\",\"old_output_id\":%d,\"new_output\":\"%s\","
		               "\"new_output_id\":%d}",
		               old_outp->name, output_get_num(old_outp), new_outp->name,
		               output_get_num(new_outp));
		return;
	}
	message_printf(server->curr_output, "Output %d does not exist", output);
	return;
}

void
keybinding_move_view_to_tile(struct cg_server *server, uint32_t view_id,
                             uint32_t tile_id, bool follow) {
	struct cg_view *view = view_from_id(server, view_id);
	struct cg_tile *tile = tile_from_id(server, tile_id);
	struct cg_tile *old_tile = view ? view->tile : NULL;
	struct cg_output *old_outp = view ? view->workspace->output : NULL;
	int old_workspace = view ? (int)view->workspace->num : -1;
	if(tile == NULL) {
		return;
	}
	if(view != NULL) {
		if(old_tile != NULL) {
			workspace_tile_update_view(old_tile, NULL);
			wl_list_remove(&view->link);
			keybinding_cycle_views(server, old_tile, 0, false, false);
			if(old_tile->view == NULL &&
			   old_tile == server->curr_output
			                   ->workspaces[server->curr_output->curr_workspace]
			                   ->focused_tile) {
				seat_set_focus(server->seat, NULL);
			}
		} else {
			wl_list_remove(&view->link);
		}
	}

	if(view != NULL) {
		struct cg_workspace *ws = tile->workspace;
		view->workspace = ws;
		wl_list_insert(&ws->views, &view->link);
		wlr_scene_node_reparent(&view->scene_tree->node, ws->scene);
		view->tile = tile;
		tile->view = view;
		if(tile ==
		   server->curr_output->workspaces[server->curr_output->curr_workspace]
		       ->focused_tile) {
			seat_set_focus(server->seat, view);
		}
	} else {
		if(tile->view) {
			tile->view->tile = NULL;
			tile->view = NULL;
		}
	}
	if(follow) {
		keybinding_focus_tile(server, tile->id);
	} else {
		if(tile->view != NULL) {
			workspace_tile_update_view(tile, tile->view);
		}
	}
	ipc_send_event(
	    server,
	    "{\"event_name\":\"move_view\",\"view_id\":%d,\"old_output\":\"%s\","
	    "\"old_workspace\":\"%d\",\"old_tile\":\"%d\",\"new_output\":\"%s\","
	    "\"new_workspace\":\"%d\",\"new_tile\":\"%d\"}",
	    view_id, old_outp ? old_outp->name : "", old_workspace,
	    old_tile ? (int)old_tile->id : -1, server->curr_output->name,
	    tile->workspace->num, tile->id);
}

void
keybinding_move_view_to_output(struct cg_server *server, int view_id,
                               int output_num, bool follow) {
	struct cg_output *outp = output_from_num(server, output_num);
	if(outp != NULL) {
		keybinding_move_view_to_tile(
		    server, view_id,
		    outp->workspaces[outp->curr_workspace]->focused_tile->id, follow);
	} else {
		message_printf(server->curr_output, "Output number %d not found.",
		               output_num);
	}
}

void
keybinding_move_view_to_workspace(struct cg_server *server, int view_id,
                                  uint32_t ws, bool follow) {
	if(ws >= server->nws) {
		message_printf(server->curr_output,
		               "Attempting to move view to workspace %d, but there are "
		               "only %d workspaces.",
		               ws + 1, server->nws);
		return;
	}
	keybinding_move_view_to_tile(
	    server, view_id, server->curr_output->workspaces[ws]->focused_tile->id,
	    follow);
}

void
merge_config(struct cg_output_config *config_new,
             struct cg_output_config *config_old) {
	if(config_new->status == OUTPUT_DEFAULT) {
		config_new->status = config_old->status;
	}
	if(config_new->pos.x == -1) {
		config_new->pos = config_old->pos;
	}
	if(config_new->refresh_rate == 0) {
		config_new->refresh_rate = config_old->refresh_rate;
	}
	if(config_new->priority == -1) {
		config_new->priority = config_old->priority;
	}
}

void
keybinding_configure_output(struct cg_server *server,
                            struct cg_output_config *cfg) {
	struct cg_output_config *config;
	config = malloc(sizeof(struct cg_output_config));
	if(config == NULL) {
		wlr_log(WLR_ERROR,
		        "Could not allocate memory for server configuration.");
		return;
	}

	*config = *cfg;
	config->output_name = strdup(cfg->output_name);

	struct cg_output_config *it, *tmp;
	wl_list_for_each_safe(it, tmp, &server->output_config, link) {
		if(strcmp(config->output_name, it->output_name) == 0) {
			wl_list_remove(&it->link);
			merge_config(config, it);
			free(it->output_name);
			free(it);
		}
	}
	wl_list_insert(&server->output_config, &config->link);

	struct cg_output *output, *tmp_output;
	wl_list_for_each_safe(output, tmp_output, &server->outputs, link) {
		if(strcmp(config->output_name, output->name) == 0) {
			int output_num = output_get_num(output);
			output_configure(server, output);
			ipc_send_event(server,
			               "{\"event_name\":\"configure_output\",\"output\":\"%"
			               "s\",\"output_id\":%d}",
			               cfg->output_name, output_num);
			return;
		}
	}
	wl_list_for_each_safe(output, tmp_output, &server->disabled_outputs, link) {
		if(strcmp(config->output_name, output->name) == 0) {
			output_configure(server, output);
			ipc_send_event(
			    output->server,
			    "{\"event_name\":\"configure_output\",\"output\":\"%s\"}",
			    cfg->output_name);
			return;
		}
	}
}

void
keybinding_configure_input(struct cg_server *server,
                           struct cg_input_config *cfg) {
	struct cg_input_config *tcfg = input_manager_create_empty_input_config();
	if(tcfg == NULL) {
		wlr_log(WLR_ERROR,
		        "Could not allocate temporary empty input configuration.");
		return;
	}
	struct cg_input_config *ocfg = input_manager_merge_input_configs(cfg, tcfg);
	free(tcfg);
	if(ocfg == NULL) {
		wlr_log(WLR_ERROR,
		        "Could not allocate input configuration for merging.");
		return;
	}
	wl_list_insert(&server->input_config, &ocfg->link);
	cg_input_manager_configure(server);
	ipc_send_event(server,
	               "{\"event_name\":\"configure_input\",\"input\":\"%s\"}",
	               cfg->identifier);
}

void
keybinding_configure_message(struct cg_server *server,
                             struct cg_message_config *config) {
	if(config->font != NULL) {
		free(server->message_config.font);
		server->message_config.font = strdup(config->font);
	}
	if(config->display_time != -1) {
		server->message_config.display_time = config->display_time;
	}
	if(config->bg_color[0] != -1) {
		server->message_config.bg_color[0] = config->bg_color[0];
		server->message_config.bg_color[1] = config->bg_color[1];
		server->message_config.bg_color[2] = config->bg_color[2];
		server->message_config.bg_color[3] = config->bg_color[3];
	}
	if(config->fg_color[0] != -1) {
		server->message_config.fg_color[0] = config->fg_color[0];
		server->message_config.fg_color[1] = config->fg_color[1];
		server->message_config.fg_color[2] = config->fg_color[2];
		server->message_config.fg_color[3] = config->fg_color[3];
	}
	if(config->anchor != CG_MESSAGE_NOPT) {
		server->message_config.anchor = config->anchor;
	}
	if(config->enabled != -1) {
		server->message_config.enabled = config->enabled;
	}
	ipc_send_event(server, "{\"event_name\":\"configure_message\"}");
}

void
set_cursor(bool enabled, struct cg_seat *seat) {
	if(enabled == true) {
		seat->enable_cursor = true;
		wlr_cursor_set_xcursor(seat->cursor, seat->xcursor_manager,
		                       DEFAULT_XCURSOR);
	} else {
		seat->enable_cursor = false;
		wlr_cursor_unset_image(seat->cursor);
	}
}

/* Hint: see keybinding.h for details on "data" */
int
run_action(enum keybinding_action action, struct cg_server *server,
           union keybinding_params data) {
	switch(action) {
	case KEYBINDING_QUIT:
		display_terminate(server);
		server->running = false;
		break;
	case KEYBINDING_CHANGE_TTY:
		return keybinding_switch_vt(server, data.u);
	case KEYBINDING_CURSOR:
		set_cursor(data.i, server->seat);
		break;
	case KEYBINDING_LAYOUT_FULLSCREEN:
		keybinding_workspace_fullscreen(server, data.us[0], data.us[1]);
		break;
	case KEYBINDING_SPLIT_HORIZONTAL:
		keybinding_split_horizontal(server, data.f);
		break;
	case KEYBINDING_SPLIT_VERTICAL:
		keybinding_split_vertical(server, data.f);
		break;
	case KEYBINDING_RUN_COMMAND: {
		int pid;
		if((pid = fork()) == 0) {
			setsid();
			sigset_t set;
			sigemptyset(&set);
			sigprocmask(SIG_SETMASK, &set, NULL);
			if(fork() == 0) {
				into_process(data.c);
			}
			_exit(0);
		} else if(pid > 0) {
			waitpid(pid, NULL, 0);
		}
	} break;
	case KEYBINDING_CYCLE_VIEWS:
		keybinding_cycle_views(server, NULL, data.us[1], data.us[0], true);
		break;
	case KEYBINDING_CYCLE_TILES:
		if(data.us[1] == 0) {
			keybinding_cycle_tiles(server, data.us[0]);
		} else {
			keybinding_focus_tile(server, data.us[1]);
		}
		break;
	case KEYBINDING_CYCLE_OUTPUT:
		keybinding_cycle_outputs(server, data.b, true);
		break;
	case KEYBINDING_SWITCH_WORKSPACE:
		keybinding_switch_ws(server, data.u);
		break;
	case KEYBINDING_SWITCH_OUTPUT:
		keybinding_switch_output(server, data.u);
		break;
	case KEYBINDING_SWITCH_MODE:
		uint32_t n_modes = 0;
		while(server->modes[n_modes] != NULL) {
			++n_modes;
		}
		if(data.u != server->seat->default_mode && data.u < n_modes &&
		   server->seat->num_pointers > 0) {
			wlr_seat_pointer_notify_clear_focus(server->seat->seat);
			if(server->seat->enable_cursor == true &&
			   server->modecursors[data.u] != NULL) {
				wlr_cursor_set_xcursor(server->seat->cursor,
				                       server->seat->xcursor_manager,
				                       server->modecursors[data.u]);
			}
		}
		server->seat->mode = data.u;
		break;
	case KEYBINDING_SWITCH_DEFAULT_MODE:
		ipc_send_event(server,
		               "{\"event_name\":\"switch_default_mode\",\"old_mode\":"
		               "\"%s\",\"mode\":\"%s\"}",
		               get_mode_name(server->modes, server->seat->default_mode),
		               get_mode_name(server->modes, data.u));
		uint32_t n_modes2 = 0;
		while(server->modes[n_modes2] != NULL) {
			++n_modes2;
		}
		if(data.u != server->seat->default_mode && data.u < n_modes2) {
			wlr_seat_pointer_notify_clear_focus(server->seat->seat);
			if(server->seat->enable_cursor == true &&
			   server->seat->num_pointers > 0) {
				if(server->modecursors[data.u] != NULL) {
					wlr_cursor_set_xcursor(server->seat->cursor,
					                       server->seat->xcursor_manager,
					                       server->modecursors[data.u]);
				} else {
					wlr_cursor_set_xcursor(server->seat->cursor,
					                       server->seat->xcursor_manager,
					                       DEFAULT_XCURSOR);
				}
			}
		}
		server->seat->mode = data.u;
		server->seat->default_mode = data.u;
		break;
	case KEYBINDING_NOOP:
		break;
	case KEYBINDING_SHOW_TIME:
		keybinding_show_time(server);
		break;
	case KEYBINDING_DUMP:
		keybinding_dump(server);
		break;
	case KEYBINDING_SHOW_INFO:
		keybinding_show_info(server);
		break;
	case KEYBINDING_DISPLAY_MESSAGE:
		keybinding_display_message(server, data.c);
		break;
	case KEYBINDING_SEND_CUSTOM_EVENT:
		keybinding_send_custom_event(server, data.c);
		break;
	case KEYBINDING_RESIZE_TILE_HORIZONTAL:
		resize_tile(server, data.is[0], 0, data.is[1]);
		break;
	case KEYBINDING_RESIZE_TILE_VERTICAL:
		resize_tile(server, 0, data.is[0], data.is[1]);
		break;
	case KEYBINDING_MOVE_TO_TILE: {
		struct cg_view *view =
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile->view;
		keybinding_move_view_to_tile(server, view ? (int)view->id : -1,
		                             data.us[0], data.us[1] > 0);
		break;
	}
	case KEYBINDING_MOVE_TO_WORKSPACE: {
		struct cg_view *view =
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile->view;
		keybinding_move_view_to_workspace(server, view ? (int)view->id : -1,
		                                  data.us[0], data.us[1] > 0);
		break;
	}
	case KEYBINDING_MOVE_TO_OUTPUT: {
		struct cg_view *view =
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile->view;
		keybinding_move_view_to_output(server, view ? (int)view->id : -1,
		                               data.us[0], data.us[1] > 0);
		break;
	}
	case KEYBINDING_MOVE_VIEW_TO_TILE: {
		keybinding_move_view_to_tile(server, data.us[0], data.us[1],
		                             data.us[2] > 0);
		break;
	}
	case KEYBINDING_MOVE_VIEW_TO_WORKSPACE: {
		keybinding_move_view_to_workspace(server, data.us[0], data.us[1],
		                                  data.us[2] > 0);
		break;
	}
	case KEYBINDING_MOVE_VIEW_TO_OUTPUT: {
		keybinding_move_view_to_output(server, data.us[0], data.us[1],
		                               data.us[2] > 0);
		break;
	}
	case KEYBINDING_MERGE_LEFT: {
		struct cg_tile *tile =
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile;
		if(data.u != 0) {
			tile = tile_from_id(server, data.u);
		}
		if(tile != NULL) {
			merge_tile_left(tile);
		}
		break;
	}
	case KEYBINDING_MERGE_RIGHT: {
		struct cg_tile *tile =
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile;
		if(data.u != 0) {
			tile = tile_from_id(server, data.u);
		}
		if(tile != NULL) {
			merge_tile_right(tile);
		}
		break;
	}
	case KEYBINDING_MERGE_TOP: {
		struct cg_tile *tile =
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile;
		if(data.u != 0) {
			tile = tile_from_id(server, data.u);
		}
		if(tile != NULL) {
			merge_tile_top(tile);
		}
		break;
	}
	case KEYBINDING_MERGE_BOTTOM: {
		struct cg_tile *tile =
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile;
		if(data.u != 0) {
			tile = tile_from_id(server, data.u);
		}
		if(tile != NULL) {
			merge_tile_bottom(tile);
		}
		break;
	}
	case KEYBINDING_SWAP_LEFT: {
		swap_tile_left(server, data.us[0], data.us[1] == 1);
		break;
	}
	case KEYBINDING_SWAP_RIGHT: {
		swap_tile_right(server, data.us[0], data.us[1] == 1);
		break;
	}
	case KEYBINDING_SWAP_TOP: {
		swap_tile_top(server, data.us[0], data.us[1] == 1);
		break;
	}
	case KEYBINDING_SWAP_BOTTOM: {
		swap_tile_bottom(server, data.us[0], data.us[1] == 1);
		break;
	}
	case KEYBINDING_SWAP: {
		swap_tiles(tile_from_id(server, data.us[0]),
		           tile_from_id(server, data.us[1]), data.us[2] == 1);
		break;
	}
	case KEYBINDING_FOCUS_LEFT: {
		focus_tile_left(
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile);
		break;
	}
	case KEYBINDING_FOCUS_RIGHT: {
		focus_tile_right(
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile);
		break;
	}
	case KEYBINDING_FOCUS_TOP: {
		focus_tile_top(
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile);
		break;
	}
	case KEYBINDING_FOCUS_BOTTOM: {
		focus_tile_bottom(
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile);
		break;
	}
	case KEYBINDING_MOVE_VIEW_TO_CYCLE_OUTPUT: {
		keybinding_move_view_to_cycle_output(server, data.b);
		break;
	}
	case KEYBINDING_DEFINEKEY:
		keybinding_definekey(server, data.kb);
		break;
	case KEYBINDING_BACKGROUND:
		keybinding_set_background(server, data.color);
		break;
	case KEYBINDING_DEFINEMODE:
		keybinding_definemode(server, data.c);
		break;
	case KEYBINDING_WORKSPACES:
		keybinding_set_nws(server, data.i);
		break;
	case KEYBINDING_CONFIGURE_OUTPUT:
		keybinding_configure_output(server, data.o_cfg);
		break;
	case KEYBINDING_CONFIGURE_MESSAGE:
		keybinding_configure_message(server, data.m_cfg);
		break;
	case KEYBINDING_CONFIGURE_INPUT:
		keybinding_configure_input(server, data.i_cfg);
		break;
	case KEYBINDING_CLOSE_VIEW:
		keybinding_close_view(
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile->view);
		break;
	case KEYBINDING_SETMODECURSOR:
		for(int i = 0; server->modes[i] != NULL; ++i) {
			if(strcmp(server->modes[i], data.cs[0]) == 0) {
				if(server->modecursors[i] != NULL) {
					free(server->modecursors[i]);
				}
				server->modecursors[i] = strdup(data.cs[1]);
			}
		}
		break;
	default: {
		wlr_log(WLR_ERROR,
		        "run_action was called with a value not present in \"enum "
		        "keybinding_action\". This should not happen.");
		return -1;
	}
	}
	return 0;
}
