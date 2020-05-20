#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/log.h>

#include "keybinding.h"
#include "message.h"
#include "output.h"
#include "seat.h"
#include "server.h"
#include "view.h"
#include "workspace.h"
#include "xdg_shell.h"

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
	for(unsigned int i = 0; i < list->length; ++i) {
		keybinding_free(list->keybindings[i], true);
	}
	free(list->keybindings);
	free(list);
}

struct cg_tile *
find_right_tile(const struct cg_tile *tile) {
	struct cg_tile *it = tile->next;
	while(it != tile && it->tile.x != tile->tile.x + tile->tile.width) {
		it = it->next;
	}
	if(it == tile) {
		return NULL;
	} else {
		return it;
	}
}

struct cg_tile *
find_left_tile(const struct cg_tile *tile) {
	struct cg_tile *it = tile->prev;
	while(it != tile && it->tile.x + it->tile.width != tile->tile.x) {
		it = it->prev;
	}
	if(it == tile) {
		return NULL;
	} else {
		return it;
	}
}

struct cg_tile *
find_top_tile(const struct cg_tile *tile) {
	struct cg_tile *it = tile->prev;
	while(it != tile && it->tile.y + it->tile.height != tile->tile.y) {
		it = it->prev;
	}
	if(it == tile) {
		return NULL;
	} else {
		return it;
	}
}

struct cg_tile *
find_bottom_tile(const struct cg_tile *tile) {
	struct cg_tile *it = tile->next;
	while(it != tile && it->tile.y != tile->tile.y + tile->tile.height) {
		it = it->next;
	}
	if(it == tile) {
		return NULL;
	} else {
		return it;
	}
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
	tile->view = swap_tile->view;
	swap_tile->view = tmp_view;
	if(tile->view != NULL) {
		view_maximize(tile->view, &tile->tile);
		view_damage_whole(tile->view);
	} else {
		wlr_output_damage_add_box(tile->workspace->output->damage, &tile->tile);
	}
	workspace_focus_tile(
	    server->curr_output->workspaces[server->curr_output->curr_workspace],
	    swap_tile);
	if(swap_tile->view != NULL) {
		view_maximize(swap_tile->view, &swap_tile->tile);
		view_damage_whole(swap_tile->view);
	} else {
		wlr_output_damage_add_box(tile->workspace->output->damage, &tile->tile);
	}

	seat_set_focus(server->seat, swap_tile->view);
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

// Returns whether x is between a and b (a exclusive, b exclusive) where
// a<b
bool
is_between_strict(int a, int b, int x) {
	return a < x && x < b;
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

	int old_x = tile->tile.x, old_y = tile->tile.y;
	*get_coord(tile) += coord_offset;
	*get_dim(tile) += dim_offset;
	struct wlr_box damage_box = {
	    .x = old_x,
	    .y = old_y,
	    .width = tile->tile.x - old_x == 0 ? tile->tile.width : coord_offset,
	    .height = tile->tile.y == 0 ? tile->tile.height : coord_offset};
	wlr_output_damage_add_box(tile->workspace->output->damage, &damage_box);
	if(tile->view != NULL) {
		view_maximize(tile->view, &tile->tile);
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
	struct cg_view *current_view = seat_get_focus(server->seat);
	struct cg_output *output = server->curr_output;

	/* We are focused on the background */
	if(current_view == NULL) {
		struct cg_view *it = NULL;
		wl_list_for_each(it, &output->workspaces[output->curr_workspace]->views,
		                 link) {
			if(view_is_visible(it)) {
				current_view = it;
				break;
			}
		}
	}

	workspace_free_tiles(output->workspaces[output->curr_workspace]);
	if(full_screen_workspace_tiles(server->output_layout,output->wlr_output, output->workspaces[output->curr_workspace])!=0) {
		wlr_log(WLR_ERROR, "Failed to allocate space for fullscreen workspace");
		return;
	}

	seat_set_focus(server->seat, current_view);
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
	new_tile->tile.x = new_x;
	new_tile->tile.y = new_y;
	new_tile->tile.width = x + width - new_x;
	new_tile->tile.height = y + height - new_y;
	new_tile->prev = curr_workspace->focused_tile;
	new_tile->next = curr_workspace->focused_tile->next;
	new_tile->view = next_view;
	new_tile->workspace = curr_workspace;
	curr_workspace->focused_tile->next->prev = new_tile;
	curr_workspace->focused_tile->next = new_tile;

	curr_workspace->focused_tile->tile.width = new_width;
	curr_workspace->focused_tile->tile.height = new_height;
	workspace_focus_tile(curr_workspace, curr_workspace->focused_tile);

	if(next_view != NULL) {
		view_maximize(next_view, &new_tile->tile);
	}

	if(original_view != NULL) {
		view_maximize(original_view, &curr_workspace->focused_tile->tile);
	}
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
keybinding_cycle_outputs(struct cg_server *server, bool reverse) {
	if(reverse) {
		server->curr_output = wl_container_of(server->curr_output->link.prev,
		                                      server->curr_output, link);
	} else {
		server->curr_output = wl_container_of(server->curr_output->link.next,
		                                      server->curr_output, link);
	}
	if(&server->curr_output->link == &server->outputs) {
		if(reverse) {
			server->curr_output = wl_container_of(
			    server->curr_output->link.prev, server->curr_output, link);
		} else {
			server->curr_output = wl_container_of(
			    server->curr_output->link.next, server->curr_output, link);
		}
	}
	seat_set_focus(
	    server->seat,
	    server->curr_output->workspaces[server->curr_output->curr_workspace]
	        ->focused_tile->view);
	message_printf(server->curr_output, "Current Output");
}

/* Cycle through views, whereby the workspace does not change */
void
keybinding_cycle_views(struct cg_server *server, bool reverse) {
	struct cg_workspace *curr_workspace =
	    server->curr_output->workspaces[server->curr_output->curr_workspace];
	struct cg_view *current_view = curr_workspace->focused_tile->view;

	struct cg_view *new_view;
	// We are focused on the desktop
	if(current_view == NULL) {
		current_view = wl_container_of(&curr_workspace->views, new_view, link);
		if(reverse) {
			new_view =
			    wl_container_of(curr_workspace->views.prev, new_view, link);
		} else {
			new_view =
			    wl_container_of(curr_workspace->views.next, new_view, link);
		}
	} else {
		if(reverse) {
			new_view = wl_container_of(current_view->link.prev, new_view, link);
		} else {
			new_view = wl_container_of(current_view->link.next, new_view, link);
		}
	}
	while(&new_view->link != &current_view->link &&
	      (&new_view->link == &curr_workspace->views ||
	       view_is_visible(new_view))) {
		if(reverse) {
			new_view = wl_container_of(new_view->link.prev, new_view, link);
		} else {
			new_view = wl_container_of(new_view->link.next, new_view, link);
		}
	}
	if(&new_view->link == &curr_workspace->views) {
		return;
	}

	wlr_output_damage_add_box(curr_workspace->output->damage,
	                          &curr_workspace->focused_tile->tile);
	seat_set_focus(server->seat, new_view);
	/* Move the previous view to the end of the list unless we are focused on
	 * the desktop*/
	if(!reverse && &current_view->link != &curr_workspace->views) {
		wl_list_remove(&current_view->link);
		wl_list_insert(curr_workspace->views.prev, &current_view->link);
	}
}

void
keybinding_cycle_tiles(struct cg_server *server, bool reverse) {
	struct cg_output *output = server->curr_output;
	struct cg_workspace *workspace = output->workspaces[output->curr_workspace];
	if(reverse) {
		workspace_focus_tile(workspace, workspace->focused_tile->prev);
	} else {
		workspace_focus_tile(workspace, workspace->focused_tile->next);
	}
	struct cg_view *next_view = workspace->focused_tile->view;

	seat_set_focus(server->seat, next_view);
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
	output->curr_workspace = ws;
	seat_set_focus(server->seat,
	               server->curr_output->workspaces[ws]->focused_tile->view);
	wlr_output_damage_add_whole(output->damage);
	message_printf(server->curr_output, "Workspace %d", ws + 1);
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

void
keybinding_move_view_to_next_output(struct cg_server *server) {
	if(wl_list_length(&server->outputs) <= 1) {
		return;
	}
	struct cg_view *view =
	    server->curr_output->workspaces[server->curr_output->curr_workspace]
	        ->focused_tile->view;
	if(view != NULL) {
		wl_list_remove(&view->link);
		server->curr_output->workspaces[server->curr_output->curr_workspace]
		    ->focused_tile->view = NULL;
		keybinding_cycle_views(server, false);
		view_damage_whole(view);
		if(server->curr_output->workspaces[server->curr_output->curr_workspace]
		       ->focused_tile->view == NULL) {
			seat_set_focus(server->seat, NULL);
		}
	}
	keybinding_cycle_outputs(server, false);
	if(view != NULL) {
		wl_list_insert(&server->curr_output
		                    ->workspaces[server->curr_output->curr_workspace]
		                    ->views,
		               &view->link);
		server->curr_output->workspaces[server->curr_output->curr_workspace]
		    ->focused_tile->view = view;
		view->workspace = server->curr_output
		                      ->workspaces[server->curr_output->curr_workspace];
		view_position(view);
		seat_set_focus(server->seat, view);
	}
}

void
keybinding_set_nws(struct cg_server *server, int nws) {
	struct cg_output *output;
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
			if(!output->workspaces[i]) {
				wlr_log(WLR_ERROR, "Failed to allocate additional workspaces");
				return;
			}

			wl_list_init(&output->workspaces[i]->views);
			wl_list_init(&output->workspaces[i]->unmanaged_views);
		}

		if(output->curr_workspace >= nws) {
			output->curr_workspace = nws - 1;
		}
	}
	server->nws = nws;
	seat_set_focus(
	    server->seat,
	    server->curr_output->workspaces[server->curr_output->curr_workspace]
	        ->focused_tile->view);
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
}

void
keybinding_definekey(struct cg_server *server, struct keybinding *kb) {
	keybinding_list_push(server->keybindings, kb);
}

void
keybinding_set_background(struct cg_server *server, float *bg) {
	server->bg_color[0] = bg[0];
	server->bg_color[1] = bg[1];
	server->bg_color[2] = bg[2];
}

void
keybinding_move_view_to_workspace(struct cg_server *server, uint32_t ws) {
	struct cg_view *view =
	    server->curr_output->workspaces[server->curr_output->curr_workspace]
	        ->focused_tile->view;
	if(view != NULL) {
		wl_list_remove(&view->link);
		server->curr_output->workspaces[server->curr_output->curr_workspace]
		    ->focused_tile->view = NULL;
		keybinding_cycle_views(server, false);
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
		ws->focused_tile->view = view;
		view_maximize(view, &ws->focused_tile->tile);
		seat_set_focus(server->seat, view);
	}
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
	case KEYBINDING_RUN_COMMAND:
		if(fork() == 0) {
			into_process(data.c);
		}
		break;
	case KEYBINDING_CYCLE_VIEWS:
		keybinding_cycle_views(server, data.b);
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
	case KEYBINDING_SWITCH_MODE:
		server->seat->mode = data.u;
		break;
	case KEYBINDING_SWITCH_DEFAULT_MODE:
		server->seat->mode = data.u;
		server->seat->default_mode = data.u;
		break;
	case KEYBINDING_NOOP:
		break;
	case KEYBINDING_SHOW_TIME:
		keybinding_show_time(server);
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
	case KEYBINDING_MOVE_VIEW_TO_NEXT_OUTPUT: {
		keybinding_move_view_to_next_output(server);
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
	default: {
		wlr_log(WLR_ERROR,
		        "run_action was called with a value not present in \"enum "
		        "keybinding_action\". This should not happen.");
		return -1;
	}
	}
	return 0;
}
