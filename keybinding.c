#define _POSIX_C_SOURCE 200809L

#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_xcursor_manager.h>

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
		break;
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
keybinding_list_init() {
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

void
swap_tile(struct cg_tile *tile,
          struct cg_tile *(*find_tile)(const struct cg_tile *)) {
	struct cg_tile *swap_tile = find_tile(tile);
	struct cg_server *server = tile->workspace->server;
	if(swap_tile == NULL || swap_tile == tile) {
		return;
	}
	struct cg_view *tmp_view = tile->view;
	struct cg_view *tmp_swap_view = swap_tile->view;
	workspace_tile_update_view(tile,NULL);
	workspace_tile_update_view(swap_tile,tmp_view);
	workspace_tile_update_view(tile,tmp_swap_view);
	workspace_focus_tile(
	    server->curr_output->workspaces[server->curr_output->curr_workspace],
	    swap_tile);
	seat_set_focus(server->seat, swap_tile->view);
	ipc_send_event(tile->workspace->output->server,
	               "{\"event_name\":\"swap_tile\",\"tile_id\":\"%d\",\"swap_"
	               "tile_id\":\"%d\",\"workspace\":\"%d\",\"output\":\"%s\"}",
	               tile->id, swap_tile->id, tile->workspace->num + 1,
	               tile->workspace->output->wlr_output->name);
}

void
swap_tile_left(struct cg_tile *tile) {
	swap_tile(tile, find_left_tile);
}

void
swap_tile_right(struct cg_tile *tile) {
	swap_tile(tile, find_right_tile);
}

void
swap_tile_top(struct cg_tile *tile) {
	swap_tile(tile, find_top_tile);
}

void
swap_tile_bottom(struct cg_tile *tile) {
	swap_tile(tile, find_bottom_tile);
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

int
get_compl_coord(struct cg_tile *tile, int *(*get_coord)(struct cg_tile *tile)) {
	return tile->tile.x + tile->tile.y - *get_coord(tile);
}

int
get_compl_dim(struct cg_tile *tile, int *(*get_dim)(struct cg_tile *tile)) {
	return tile->tile.width + tile->tile.height - *get_dim(tile);
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
	} else if(*get_dim(tile) - coord_offset + dim_offset <= 0) {
		return false;
	}

	for(struct cg_tile *it = tile->next; it != tile && it != NULL;
	    it = it->next) {
		if(it == parent || it == orig) {
			continue;
		}
		if(intervalls_intersect(
		       get_compl_coord(tile, get_coord),
		       get_compl_coord(tile, get_coord) + get_compl_dim(tile, get_dim),
		       get_compl_coord(it, get_coord),
		       get_compl_coord(it, get_coord) + get_compl_dim(it, get_dim))) {
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
		if(intervalls_intersect(
		       get_compl_coord(tile, get_coord),
		       get_compl_coord(tile, get_coord) + get_compl_dim(tile, get_dim),
		       get_compl_coord(it, get_coord),
		       get_compl_coord(it, get_coord) + get_compl_dim(it, get_dim))) {
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
	               "{\"event_name\":\"resize_tile\",\"tile_id\":\"%d\",\"old_"
	               "dims\":\"[%d;%d;%d;%d]\",\"new_dims\":\"[%d;%d;%d;%d]\","
	               "\"workspace\":\"%d\",\"output\":\"%s\"}",
	               tile->id, old_x, old_y, old_height, old_width, tile->tile.x,
	               tile->tile.y, tile->tile.height, tile->tile.width,
	               tile->workspace->num + 1,
	               tile->workspace->output->wlr_output->name);
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
resize_tile(struct cg_server *server, int hpixs, int vpixs) {
	struct cg_output *output = server->curr_output;
	struct wlr_box *output_box = wlr_output_layout_get_box(
	    output->server->output_layout, output->wlr_output);

	struct cg_tile *focused =
	    output->workspaces[output->curr_workspace]->focused_tile;
	/* First do the horizontal adjustment */
	if(hpixs != 0 && focused->tile.width < output_box->width &&
	   is_between_strict(0, output_box->width, focused->tile.width + hpixs)) {
		int x_offset = 0;
		/* In case we are on the total right, move the left edge of the tile */
		if(focused->tile.x + focused->tile.width == output_box->width) {
			x_offset = -hpixs;
		}
		bool resize_allowed =
		    resize_allowed_horizontal(focused, NULL, x_offset, hpixs);
		if(resize_allowed) {
			resize_horizontal(focused, NULL, x_offset, hpixs);
		}
	}
	/* Repeat for vertical */
	if(vpixs != 0 && focused->tile.height < output_box->height &&
	   is_between_strict(0, output_box->height, focused->tile.height + vpixs)) {
		int y_offset = 0;
		if(focused->tile.y + focused->tile.height == output_box->height) {
			y_offset = -vpixs;
		}
		bool resize_allowed =
		    resize_allowed_vertical(focused, NULL, y_offset, vpixs);
		if(resize_allowed) {
			resize_vertical(focused, NULL, y_offset, vpixs);
		}
	}
}

void
keybinding_workspace_fullscreen(struct cg_server *server) {
	output_make_workspace_fullscreen(server->curr_output,
	                                 server->curr_output->curr_workspace);
	struct cg_output *output = server->curr_output;
	ipc_send_event(server,
	               "{\"event_name\":\"fullscreen\",\"tile_id\":\"%d\","
	               "\"workspace\":\"%d\",\"output\":\"%s\"}",
	               output->workspaces[output->curr_workspace]->focused_tile->id,
	               output->workspaces[output->curr_workspace]->num + 1,
	               output->wlr_output->name);
}

// Switch to a differerent virtual terminal
static int
keybinding_switch_vt(struct wlr_backend *backend, unsigned int vt) {
	if(wlr_backend_is_multi(backend)) {
		struct wlr_session *session = wlr_backend_get_session(backend);
		if(session) {
			wlr_session_change_vt(session, vt);
		}
		return 0;
	}
	return -1;
}

/* Split screen (vertical or horizontal)
 * Important: Do not attempt to perform mathematical simplifications in this
 * function without taking rounding errors into account. */
static void
keybinding_split_output(struct cg_output *output, bool vertical) {
	struct cg_view *original_view = seat_get_focus(output->server->seat);
	struct cg_workspace *curr_workspace =
	    output->workspaces[output->curr_workspace];
	int32_t width = curr_workspace->focused_tile->tile.width;
	int32_t height = curr_workspace->focused_tile->tile.height;
	int32_t x = curr_workspace->focused_tile->tile.x;
	int32_t y = curr_workspace->focused_tile->tile.y;
	int32_t new_width, new_height;

	if(vertical) {
		new_width = width / 2;
		new_height = height;
	} else {
		new_width = width;
		new_height = height / 2;
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
	workspace_tile_update_view(new_tile,next_view);
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
	    "{\"event_name\":\"split\",\"tile_id\":\"%d\",\"new_tile_id\":\"%d\","
	    "\"workspace\":\"%d\",\"output\":\"%s\",\"vertical\":\"%d\"}",
	    curr_workspace->focused_tile->id, new_tile->id, curr_workspace->num + 1,
	    curr_workspace->output->wlr_output->name, vertical);
}

static void
keybinding_close_view(struct cg_view *view) {
	if(view == NULL) {
		return;
	}
	struct cg_output *outp = view->workspace->output;
	uint32_t view_id = view->id;
	uint32_t tile_id = view->id;
	uint32_t ws = view->workspace->num;
	view->impl->close(view);
	ipc_send_event(outp->server,
	               "{\"event_name\":\"close\",\"view_id\":\"%d\",\"tile_id\":"
	               "\"%d\",\"workspace\":\"%d\",\"output\":\"%s\"}",
	               view_id, tile_id, ws + 1, outp->wlr_output->name);
}

static void
keybinding_split_vertical(struct cg_server *server) {
	keybinding_split_output(server->curr_output, true);
}

static void
keybinding_split_horizontal(struct cg_server *server) {
	keybinding_split_output(server->curr_output, false);
}

static void
into_process(const char *command) {
	execl("/bin/sh", "sh", "-c", command, (char *)NULL);
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
keybinding_cycle_outputs(struct cg_server *server, bool reverse) {
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
	ipc_send_event(output->server,
	               "{\"event_name\":\"cycle_outputs\",\"old_output\":\"%s\","
	               "\"new_output\":\"%s\",\"reverse\":\"%d\"}",
	               old_output->wlr_output->name, output->wlr_output->name,
	               reverse);
}

/* Cycle through views, whereby the workspace does not change */
void
keybinding_cycle_views(struct cg_server *server, bool reverse, bool ipc) {
	struct cg_workspace *curr_workspace =
	    server->curr_output->workspaces[server->curr_output->curr_workspace];
	struct cg_view *current_view = curr_workspace->focused_tile->view;

	struct cg_view *it_view, *next_view = NULL;
	if(reverse) {
		wl_list_for_each(it_view,&curr_workspace->views,link) {
			if(!view_is_visible(it_view)) {
				next_view=it_view;
				break;
			}
		}
	} else {
		wl_list_for_each_reverse(it_view,&curr_workspace->views,link) {
			if(!view_is_visible(it_view)) {
				next_view=it_view;
				break;
			}
		}
	}

	if(next_view == NULL) {
		return;
	}

	seat_set_focus(server->seat, next_view);
	if(ipc) {
		ipc_send_event(curr_workspace->output->server,
		               "{\"event_name\":\"cycle_views\",\"old_view_id\":\"%d\","
		               "\"new_view_id\":\"%d\",\"tile_id\":\"%d\","
		               "\"workspace\":\"%d\",\"output\":\"%s\"}",
		               current_view->link.next == curr_workspace->views.next
		                   ? -1
		                   : (int)current_view->id,
		               next_view == NULL ? -1 : (int)next_view->id,
		               next_view->tile->id, curr_workspace->num + 1,
		               curr_workspace->output->wlr_output->name);
	}
}

void
keybinding_focus_tile(struct cg_server *server, uint32_t tile_id) {
	struct cg_output *output = server->curr_output;
	struct cg_workspace *workspace = output->workspaces[output->curr_workspace];
	struct cg_tile *old_tile = workspace->focused_tile;
	bool first = true;
	for(struct cg_tile *tile = workspace->focused_tile;
			first || tile != workspace->focused_tile; tile = tile->next) {
		first = false;
		if(tile->id==tile_id) {
			workspace_focus_tile(workspace, tile);
			struct cg_view *next_view = workspace->focused_tile->view;
			seat_set_focus(server->seat, next_view);
			ipc_send_event(
					output->server,
					"{\"event_name\":\"focus_tile\",\"old_tile_id\":\"%d\",\"new_tile_"
					"id\":\"%d\",\"workspace\":\"%d\",\"output\":\"%s\"}",
					old_tile->id, workspace->focused_tile->id, workspace->num + 1,
					output->wlr_output->name);
			break;
		}
	}
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
	               "{\"event_name\":\"switch_ws\",\"old_workspace\":\"%d\","
	               "\"new_workspace\":\"%d\",\"output\":\"%s\"}",
	               old_ws + 1, ws + 1, output->wlr_output->name);
	return 0;
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
	if(nmemb==0) {
		wlr_log(WLR_ERROR,"This is a bug: Cagebreak has no valid modes. This should not occur, since default modes are defined on startup.");
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
	uint32_t nmemb = 4;
	outp_str.str_arr = calloc(nmemb, sizeof(char *));
	print_str(&outp_str, "\"id\": %d,\n", view->id);
	print_str(&outp_str, "\"pid\": \"%d\",\n", view->impl->get_pid(view));
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
	print_str(&outp_str, "\"view\": \"%d\"\n",
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
	uint32_t nmemb = 8;
	struct wlr_box *outp_box=wlr_output_layout_get_box(outp->server->output_layout,outp->wlr_output);
	outp_str.str_arr = calloc(nmemb, sizeof(char *));
	print_str(&outp_str, "\"%s\": {\n", outp->wlr_output->name);
	print_str(&outp_str, "\"priority\": %d,\n", outp->priority);
	if(outp_box!=NULL) {
		print_str(&outp_str, "\"coords\": {\"x\":%d,\"y\":%d},\n", outp_box->x,outp_box->y);
	}
	print_str(&outp_str, "\"size\": {\"width\":%d,\"height\":%d},\n", outp->wlr_output->width,outp->wlr_output->height);
	print_str(&outp_str, "\"refresh_rate\": %f,\n", (float) outp->wlr_output->refresh/1000.0);
	print_str(&outp_str, "\"curr_workspace\": %d,\n", outp->curr_workspace);
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
	if(grp->identifier!=NULL) {
		print_str(&outp_str, "\"%s\": {\n", grp->identifier);
	} else {
		print_str(&outp_str, "\"NULL\": {\n");
	}
	print_str(&outp_str, "\"commands_enabled\": %d,\n", grp->enable_keybindings);
	print_str(&outp_str, "\"repeat_delay\": %d,\n", grp->wlr_group->keyboard.repeat_info.delay);
	print_str(&outp_str, "\"repeat_rate\": %d\n", grp->wlr_group->keyboard.repeat_info.rate);
	print_str(&outp_str, "}");
	return dyn_str_to_str(&outp_str);
}

char *
print_keyboard_groups(struct cg_server *server) {
	uint32_t ninps = wl_list_length(&server->seat->keyboard_groups);
	struct dyn_str outp_str;
	outp_str.len = 0;
	outp_str.cur_pos = 0;
	outp_str.str_arr = calloc((2 * ninps - 1) + 2, sizeof(char *));
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
	if(dev->identifier!=NULL) {
		print_str(&outp_str, "\"%s\": {\n", dev->identifier);
	} else {
		print_str(&outp_str, "\"NULL\": {\n");
	}
	print_str(&outp_str, "\"is_virtual\": %d,\n", dev->is_virtual);
	print_str(&outp_str, "\"type\": \"%s\",\n", dev->wlr_device->type==WLR_INPUT_DEVICE_POINTER?"pointer":dev->wlr_device->type==WLR_INPUT_DEVICE_SWITCH?"switch":dev->wlr_device->type==WLR_INPUT_DEVICE_TABLET_PAD?"tablet pad":dev->wlr_device->type==WLR_INPUT_DEVICE_TABLET_TOOL?"tablet tool":dev->wlr_device->type==WLR_INPUT_DEVICE_TOUCH?"touch":dev->wlr_device->type==WLR_INPUT_DEVICE_KEYBOARD?"keyboard":"unknown");
	print_str(&outp_str, "}");
	return dyn_str_to_str(&outp_str);
}

char *
print_input_devices(struct cg_server *server) {
	uint32_t ninps = wl_list_length(&server->input->devices);
	struct dyn_str outp_str;
	outp_str.len = 0;
	outp_str.cur_pos = 0;
	outp_str.str_arr = calloc((2 * ninps - 1) + 2, sizeof(char *));
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

void
keybinding_dump(struct cg_server *server) {
	struct dyn_str str;
	str.len = 0;
	str.cur_pos = 0;
	uint32_t nmemb = 12;
	str.str_arr = calloc(nmemb, sizeof(char *));

	print_str(&str, "{\"event_name\":\"dump\",");
	print_str(&str, "\"nws\":%d,\n", server->nws);
	print_str(&str, "\"bg_color\":[%f,%f,%f,%f],\n", server->bg_color[0],
	          server->bg_color[1], server->bg_color[2], server->bg_color[3]);
	print_str(&str, "\"views_curr_id\":%d,\n", server->views_curr_id);
	print_str(&str, "\"tiles_curr_id\":%d,\n", server->tiles_curr_id);
	print_str(&str, "\"curr_output\":\"%s\",\n",
	          server->curr_output->wlr_output->name);
	print_modes(&str, server->modes);
	char *outps_str = print_outputs(server);
	if(outps_str!=NULL) {
		print_str(&str, "%s,", outps_str);
		free(outps_str);
	}
	char *keyboards_str = print_keyboard_groups(server);
	if(keyboards_str!=NULL) {
		print_str(&str, "%s,", keyboards_str);
		free(keyboards_str);
	}
	char *input_dev_str = print_input_devices(server);
	if(input_dev_str!=NULL) {
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
		keybinding_cycle_views(server, false, false);
		if(server->curr_output->workspaces[server->curr_output->curr_workspace]
		       ->focused_tile->view == NULL) {
			seat_set_focus(server->seat, NULL);
		}
	}
	keybinding_cycle_outputs(server, reverse);
	if(view != NULL) {
		struct cg_workspace *ws =
		    server->curr_output
		        ->workspaces[server->curr_output->curr_workspace];
		wl_list_insert(&ws->views, &view->link);
		wlr_scene_node_reparent(view->scene_node, &ws->scene->node);
		workspace_tile_update_view(ws->focused_tile,view);
		view->workspace = ws;
		seat_set_focus(server->seat, view);
	}
	int id = -1;
	if(view != NULL) {
		id = view->id;
	}
	ipc_send_event(server,
	               "{\"event_name\":\"move_view_cycle_output\",\"view_id\":\"%"
	               "d\",\"old_output\":\"%s\",\"new_output\":\"%s\"}",
	               id, old_outp->wlr_output->name,
	               server->curr_output->wlr_output->name);
}

void
keybinding_set_nws(struct cg_server *server, int nws) {
	struct cg_output *output;
	int old_nws = server->nws;
	wl_list_for_each(output, &server->outputs, link) {
		for(unsigned int i = nws; i < server->nws; ++i) {
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
		for(int i = server->nws; i < nws; ++i) {
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
			workspace_focus(output, nws - 1);
		}
	}
	server->nws = nws;
	seat_set_focus(
	    server->seat,
	    server->curr_output->workspaces[server->curr_output->curr_workspace]
	        ->focused_tile->view);
	ipc_send_event(
	    server,
	    "{\"event_name\":\"set_nws\",\"old_nws\":\"%d\",\"new_nws\":\"%d\"}",
	    old_nws, server->nws);
}

void
keybinding_definemode(struct cg_server *server, char *mode) {
	int length = 0;
	while(server->modes[length++] != NULL)
		;
	char **tmp = realloc(server->modes, (length + 1) * sizeof(char *));
	if(tmp == NULL) {
		wlr_log(WLR_ERROR, "Could not allocate memory for storing modes.");
		return;
	}
	server->modes = tmp;
	server->modes[length] = NULL;

	server->modes[length - 1] = strdup(mode);
	ipc_send_event(server, "{\"event_name\":\"definemode\",\"mode\":\"%s\"}",
	               mode);
}

void
keybinding_definekey(struct cg_server *server, struct keybinding *kb) {
	keybinding_list_push(server->keybindings, kb);
	ipc_send_event(server,
	               "{\"event_name\":\"definekey\",\"modifiers\":\"%d\",\"key\":"
	               "\"%d\",\"command\":\"%d\"}",
	               kb->modifiers, kb->key, kb->action);
}

void
keybinding_set_background(struct cg_server *server, float *bg) {
	ipc_send_event(server,
	               "{\"event_name\":\"background\",\"old_bg\":\"[%f,%f,%f]\","
	               "\"new_bg\":\"[%f,%f,%f]\"}",
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
	struct cg_output *it;
	int count = 1;
	wl_list_for_each(it, &server->outputs, link) {
		if(count == output) {
			set_output(server, it);
			ipc_send_event(server,
			               "{\"event_name\":\"switch_output\",\"old_output\":"
			               "\"%s\",\"new_output\":\"%s\"}",
			               old_outp->wlr_output->name, it->wlr_output->name);
			return;
		}
		++count;
	}
	message_printf(server->curr_output, "Output %d does not exist", output);
	return;
}

void
keybinding_move_view_to_output(struct cg_server *server, int output_num) {
	struct cg_output *old_outp = server->curr_output;
	struct cg_view *view =
	    server->curr_output->workspaces[server->curr_output->curr_workspace]
	        ->focused_tile->view;
	if(view != NULL) {
		workspace_tile_update_view(server->curr_output->workspaces[server->curr_output->curr_workspace]
		    ->focused_tile,NULL);
		wl_list_remove(&view->link);
		keybinding_cycle_views(server, false, false);
		if(server->curr_output->workspaces[server->curr_output->curr_workspace]
		       ->focused_tile->view == NULL) {
			seat_set_focus(server->seat, NULL);
		}
	}
	keybinding_switch_output(server, output_num);
	if(view != NULL) {
		struct cg_workspace *ws =
		    server->curr_output
		        ->workspaces[server->curr_output->curr_workspace];
		view->workspace = ws;
		wl_list_insert(&ws->views, &view->link);
		wlr_scene_node_reparent(view->scene_node, &ws->scene->node);
		workspace_tile_update_view(ws->focused_tile,view);
		seat_set_focus(server->seat, view);
	}
	pid_t view_pid = -1;
	int view_id = -1;
	if(view != NULL) {
		view_pid = view->impl->get_pid(view);
		view_id = view->id;
	}
	ipc_send_event(
	    server,
	    "{\"event_name\":\"move_view_to_output\",\"view_id\":\"%d\",\"old_"
	    "output\":\"%s\",\"new_output\":\"%s\",\"view_pid\":\"%d\"}",
	    view_id, old_outp->wlr_output->name,
	    server->curr_output->wlr_output->name, view_pid);
}

void
keybinding_move_view_to_workspace(struct cg_server *server, uint32_t ws) {
	uint32_t old_ws = server->curr_output->curr_workspace;
	struct cg_view *view =
	    server->curr_output->workspaces[server->curr_output->curr_workspace]
	        ->focused_tile->view;
	if(view != NULL) {
		wl_list_remove(&view->link);
		server->curr_output->workspaces[server->curr_output->curr_workspace]
		    ->focused_tile->view = NULL;
		keybinding_cycle_views(server, false, false);
		if(server->curr_output->workspaces[server->curr_output->curr_workspace]
		       ->focused_tile->view == NULL) {
			seat_set_focus(server->seat, NULL);
		}
	}
	keybinding_switch_ws(server, ws);
	if(view != NULL) {
		struct cg_workspace *ws =
		    server->curr_output
		        ->workspaces[server->curr_output->curr_workspace];
		view->workspace = ws;
		wl_list_insert(&ws->views, &view->link);
		wlr_scene_node_reparent(view->scene_node, &ws->scene->node);
		workspace_tile_update_view(ws->focused_tile,view);
		seat_set_focus(server->seat, view);
	}
	ipc_send_event(server,
	               "{\"event_name\":\"move_view_to_ws\",\"view_id\":\"%d\","
	               "\"old_workspace\":\"%d\",\"new_workspace\":\"%d\","
	               "\"output\":\"%s\",\"view_pid\":\"%d\"}",
	               view == NULL ? -1 : (int)view->id, old_ws, ws,
	               server->curr_output->wlr_output->name,
	               view == NULL ? 0 : view->impl->get_pid(view));
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
		if(strcmp(config->output_name, output->wlr_output->name) == 0) {
			output_configure(server, output);
			ipc_send_event(
			    output->server,
			    "{\"event_name\":\"configure_output\",\"output\":\"%s\"}",
			    cfg->output_name);
			return;
		}
	}
	wl_list_for_each_safe(output, tmp_output, &server->disabled_outputs, link) {
		if(strcmp(config->output_name, output->wlr_output->name) == 0) {
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
	struct cg_input_config *tcfg=input_manager_create_empty_input_config();
	if(tcfg==NULL) {
		wlr_log(WLR_ERROR,"Could not allocate temporary empty input configuration.");
		return;
	}
	struct cg_input_config *ocfg=input_manager_merge_input_configs(cfg,tcfg);
	free(tcfg);
	if(ocfg==NULL) {
		wlr_log(WLR_ERROR,"Could not allocate input configuration for merging.");
		return;
	}
	wl_list_insert(&server->input_config,&ocfg->link);
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
	ipc_send_event(server, "{\"event_name\":\"configure_message\"}");
}

/* Hint: see keybinding.h for details on "data" */
int
run_action(enum keybinding_action action, struct cg_server *server,
           union keybinding_params data) {
	switch(action) {
	case KEYBINDING_QUIT:
		display_terminate(server);
		break;
	case KEYBINDING_CHANGE_TTY:
		return keybinding_switch_vt(server->backend, data.u);
	case KEYBINDING_LAYOUT_FULLSCREEN:
		keybinding_workspace_fullscreen(server);
		break;
	case KEYBINDING_SPLIT_HORIZONTAL:
		keybinding_split_horizontal(server);
		break;
	case KEYBINDING_SPLIT_VERTICAL:
		keybinding_split_vertical(server);
		break;
	case KEYBINDING_RUN_COMMAND: {
		int pid;
		if((pid = fork()) == 0) {
			if(fork() == 0) {
				into_process(data.c);
			}
			_exit(0);
		} else if(pid > 0) {
			waitpid(pid, NULL, 0);
		}
	} break;
	case KEYBINDING_CYCLE_VIEWS:
		keybinding_cycle_views(server, data.b, true);
		break;
	case KEYBINDING_CYCLE_TILES:
		keybinding_cycle_tiles(server, data.b);
		break;
	case KEYBINDING_CYCLE_OUTPUT:
		keybinding_cycle_outputs(server, data.b);
		break;
	case KEYBINDING_SWITCH_WORKSPACE:
		keybinding_switch_ws(server, data.u);
		break;
	case KEYBINDING_SWITCH_OUTPUT:
		keybinding_switch_output(server, data.u);
		break;
	case KEYBINDING_SWITCH_MODE:
		if(data.u!=server->seat->default_mode) {
			wlr_seat_pointer_notify_clear_focus(server->seat->seat);
			wlr_xcursor_manager_set_cursor_image(server->seat->xcursor_manager,
			                                     "dot_box_mask", server->seat->cursor);
		}
		server->seat->mode = data.u;
		break;
	case KEYBINDING_SWITCH_DEFAULT_MODE:
		ipc_send_event(server,
		               "{\"event_name\":\"switch_default_mode\",\"old_mode\":"
		               "\"%d\",\"mode\":\"%d\"}",
		               server->seat->default_mode, data.u);
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
	case KEYBINDING_RESIZE_TILE_HORIZONTAL:
		resize_tile(server, data.i, 0);
		break;
	case KEYBINDING_RESIZE_TILE_VERTICAL:
		resize_tile(server, 0, data.i);
		break;
	case KEYBINDING_MOVE_VIEW_TO_WORKSPACE: {
		keybinding_move_view_to_workspace(server, data.u);
		break;
	}
	case KEYBINDING_MOVE_VIEW_TO_OUTPUT: {
		keybinding_move_view_to_output(server, data.u);
		break;
	}
	case KEYBINDING_SWAP_LEFT: {
		swap_tile_left(
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile);
		break;
	}
	case KEYBINDING_SWAP_RIGHT: {
		swap_tile_right(
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile);
		break;
	}
	case KEYBINDING_SWAP_TOP: {
		swap_tile_top(
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile);
		break;
	}
	case KEYBINDING_SWAP_BOTTOM: {
		swap_tile_bottom(
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile);
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
	case KEYBINDING_FOCUS_TILE:
		keybinding_focus_tile(server,data.u);
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
