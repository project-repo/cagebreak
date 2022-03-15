/*
 * Cagebreak: A Wayland tiling compositor.
 *
 * Copyright (C) 2020-2022 The Cagebreak Authors
 * Copyright (C) 2018-2020 Jente Hidskes
 * Copyright (C) 2019 The Sway authors
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200809L

#include "config.h"
#include <wlr/config.h>

#include <string.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/wayland.h>
#if WLR_HAS_X11_BACKEND
#include <wlr/backend/x11.h>
#endif
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include <wlr/util/region.h>
#if CG_HAS_XWAYLAND
#include <wlr/xwayland.h>
#endif

#include "keybinding.h"
#include "message.h"
#include "output.h"
#include "seat.h"
#include "server.h"
#include "util.h"
#include "view.h"
#include "workspace.h"
#if CG_HAS_XWAYLAND
#include "xwayland.h"
#endif

void
output_clear(struct cg_output *output) {
	struct cg_server *server = output->server;
	wlr_output_layout_remove(server->output_layout, output->wlr_output);

	if(server->running && server->curr_output == output &&
	   wl_list_length(&server->outputs) > 1) {
		keybinding_cycle_outputs(server, false);
	}

	wl_list_remove(&output->link);

	message_clear(output);

	struct cg_view *view, *view_tmp;
	if(server->running) {
		for(unsigned int i = 0; i < server->nws; ++i) {

			bool first = true;
			for(struct cg_tile *tile = output->workspaces[i]->focused_tile;
			    first || output->workspaces[i]->focused_tile != tile;
			    tile = tile->next) {
				first = false;
				tile->view = NULL;
			}
			wl_list_for_each_safe(view, view_tmp, &output->workspaces[i]->views,
			                      link) {
				wl_list_remove(&view->link);
				if(wl_list_empty(&server->outputs)) {
					view->impl->destroy(view);
				} else {
					wl_list_insert(
					    &server->curr_output
					         ->workspaces[server->curr_output->curr_workspace]
					         ->views,
					    &view->link);
					view->workspace =
					    server->curr_output
					        ->workspaces[server->curr_output->curr_workspace];
					view->tile =
					    server->curr_output
					        ->workspaces[server->curr_output->curr_workspace]
					        ->focused_tile;
					if(server->seat->focused_view == NULL) {
						seat_set_focus(server->seat, view);
					}
				}
			}
			wl_list_for_each_safe(
			    view, view_tmp, &output->workspaces[i]->unmanaged_views, link) {
				wl_list_remove(&view->link);
				if(wl_list_empty(&server->outputs)) {
					view->impl->destroy(view);
				} else {
					wl_list_insert(
					    &server->curr_output
					         ->workspaces[server->curr_output->curr_workspace]
					         ->unmanaged_views,
					    &view->link);
					view->workspace =
					    server->curr_output
					        ->workspaces[server->curr_output->curr_workspace];
					view->tile =
					    server->curr_output
					        ->workspaces[server->curr_output->curr_workspace]
					        ->focused_tile;
				}
			}
		}
	}
}

static void
output_destroy(struct cg_output *output) {
	struct cg_server *server = output->server;
	char *outp_name = strdup(output->wlr_output->name);

	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->mode.link);
	wl_list_remove(&output->commit.link);
	wl_list_remove(&output->frame.link);

	output_clear(output);

	/*Important: due to unfortunate events, "workspace" and "output->workspace"
	 * have nothing in common. The former is the workspace of a single output,
	 * whereas the latter is the workspace of the outputs.*/
	for(unsigned int i = 0; i < server->nws; ++i) {
		workspace_free(output->workspaces[i]);
	}
	free(output->workspaces);

	free(output);

	if(outp_name != NULL) {
		ipc_send_event(server, "new_output(output:%s)", outp_name);
		free(outp_name);
	} else {
		wlr_log(WLR_ERROR,
		        "Failed to allocate memory for output name in output_destroy");
	}
	if(wl_list_empty(&server->outputs)) {
		wl_display_terminate(server->wl_display);
	}
}

static void
handle_output_destroy(struct wl_listener *listener, void *data) {
	struct cg_output *output = wl_container_of(listener, output, destroy);
	output_destroy(output);
}

static void
handle_output_frame(struct wl_listener *listener, void *data) {
	struct cg_output *output=wl_container_of(listener, output, frame);
	if(!output->wlr_output->enabled) {
		return;
	}
	wlr_scene_output_commit(output->scene_output);

	struct timespec now={0};
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(output->scene_output, &now);
}

struct cg_output_config *
output_find_config(struct cg_server *server, struct wlr_output *output) {
	struct cg_output_config *config;
	wl_list_for_each(config, &server->output_config, link) {
		if(strcmp(config->output_name, output->name) == 0) {
			return config;
		}
	}
	return NULL;
}

static void
output_set_mode(struct wlr_output *output, int width, int height,
                float refresh_rate) {
	int mhz = (int)(refresh_rate * 1000);

	if(wl_list_empty(&output->modes)) {
		wlr_log(WLR_DEBUG, "Assigning custom mode to %s", output->name);
		wlr_output_set_custom_mode(output, width, height,
		                           refresh_rate > 0 ? mhz : 0);
		return;
	}

	struct wlr_output_mode *mode, *best = NULL;
	wl_list_for_each(mode, &output->modes, link) {
		if(mode->width == width && mode->height == height) {
			if(mode->refresh == mhz) {
				best = mode;
				break;
			}
			if(best == NULL || mode->refresh > best->refresh) {
				best = mode;
			}
		}
	}
	if(!best) {
		wlr_log(WLR_ERROR, "Configured mode for %s not available",
		        output->name);
		wlr_log(WLR_INFO, "Picking preferred mode instead");
		best = wlr_output_preferred_mode(output);
	} else {
		wlr_log(WLR_DEBUG, "Assigning configured mode to %s", output->name);
	}
	wlr_output_set_mode(output, best);
	if(!wlr_output_test(output)) {
		wlr_log(WLR_ERROR, "Unable to assign configured mode to %s, picking arbitrary available mode",output->name);
		struct wlr_output_mode *mode;
		wl_list_for_each(mode, &output->modes, link) {
			if(mode == best) {
				continue;
			}
			wlr_output_set_mode(output, mode);
			if(wlr_output_test(output)) {
				break;
			}
		}
	}
}

void
output_insert(struct cg_server *server, struct cg_output *output) {
	struct cg_output *it, *prev_it = NULL;
	bool first = true;
	wl_list_for_each(it, &server->outputs, link) {
		if(it->priority < output->priority) {
			if(first == true) {
				wl_list_insert(&server->outputs, &output->link);
			} else {
				wl_list_insert(it->link.prev, &output->link);
			}
			return;
		}
		first = false;
		prev_it = it;
	}
	if(prev_it == NULL) {
		wl_list_insert(&server->outputs, &output->link);
	} else {
		wl_list_insert(&prev_it->link, &output->link);
	}
}

void
output_configure(struct cg_server *server, struct cg_output *output) {
	struct wlr_output *wlr_output = output->wlr_output;
	struct cg_output_config *config = output_find_config(server, wlr_output);
	/* Refuse to disable the only output */
	if(config != NULL && config->status == OUTPUT_DISABLE &&
	   wl_list_length(&server->outputs) == 1) {
		return;
	}
	if(output->wlr_output->enabled) {
		wlr_output_layout_remove(server->output_layout, wlr_output);
	}

	if(config == NULL) {
		wlr_output_layout_add_auto(server->output_layout, wlr_output);

		struct wlr_output_mode *preferred_mode =
		    wlr_output_preferred_mode(wlr_output);
		if(preferred_mode) {
			wlr_output_set_mode(wlr_output, preferred_mode);
		}
		wl_list_remove(&output->link);
		output_insert(server, output);
		wlr_output_enable(wlr_output, true);
		wlr_output_commit(wlr_output);
	} else {
		if(config->status == OUTPUT_DISABLE) {
			output_clear(output);
			wl_list_insert(&server->disabled_outputs, &output->link);
			wlr_output_enable(wlr_output, false);
			wlr_output_commit(wlr_output);
		} else {
			wl_list_remove(&output->link);
			if(config->priority != -1) {
				output->priority = config->priority;
			}
			if(config->pos.x != -1) {
				output_set_mode(wlr_output, config->pos.width,
				                config->pos.height, config->refresh_rate);
				wlr_output_layout_add(server->output_layout, wlr_output,
				                      config->pos.x, config->pos.y);
			}
			output_insert(server, output);
			wlr_output_enable(wlr_output, true);
			wlr_output_commit(wlr_output);
		}
	}
	struct wlr_scene_output *scene_output;
	wl_list_for_each(scene_output,&output->server->scene->outputs,link) {
		if(scene_output->output == wlr_output) {
			output->scene_output=scene_output;
			break;
		}
	}
	if(output->bg!=NULL) {
		wlr_scene_node_destroy(&output->bg->node);
	}
	output->bg=wlr_scene_rect_create(&output->scene_output->scene->node, output->wlr_output->width,output->wlr_output->height, server->bg_color);
	struct wlr_box *box = wlr_output_layout_get_box(
	    server->output_layout, output->wlr_output);
	wlr_scene_node_set_position(&output->bg->node, box->x, box->y);
	wlr_scene_node_lower_to_bottom(&output->bg->node);
}

static void
handle_output_commit(struct wl_listener *listener, void *data) {
	struct cg_output *output = wl_container_of(listener, output, commit);
	struct wlr_output_event_commit *event = data;

	if(!output->wlr_output->enabled || output->workspaces == NULL) {
		return;
	}

	if(event->committed &
	   (WLR_OUTPUT_STATE_TRANSFORM | WLR_OUTPUT_STATE_SCALE)) {
		struct cg_view *view;
		wl_list_for_each(
		    view, &output->workspaces[output->curr_workspace]->views, link) {
			if(view_is_visible(view)) {
				view_maximize(view, view->tile);
			}
		}
	}
}

static void
handle_output_mode(struct wl_listener *listener, void *data) {
	struct cg_output *output = wl_container_of(listener, output, mode);

	if(!output->wlr_output->enabled || output->workspaces == NULL) {
		return;
	}

	struct cg_view *view;
	wl_list_for_each(view, &output->workspaces[output->curr_workspace]->views,
	                 link) {
		if(view_is_visible(view)) {
			view_maximize(view, view->tile);
		}
	}
}

#if CG_HAS_FANALYZE
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
void
handle_new_output(struct wl_listener *listener, void *data) {
	struct cg_server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	if(!wlr_output_init_render(wlr_output,server->allocator,server->renderer)) {
		wlr_log(WLR_ERROR, "Failed to initialize output rendering");
		return;
	}

	struct cg_output *output = calloc(1, sizeof(struct cg_output));
	if(!output) {
		wlr_log(WLR_ERROR, "Failed to allocate output");
		return;
	}

	output->wlr_output = wlr_output;
	output->server = server;
	output->last_scanned_out_view = NULL;

	struct cg_output_priorities *it;
	int prio = -1;
	wl_list_for_each(it, &server->output_priorities, link) {
		if(strcmp(output->wlr_output->name, it->ident) == 0) {
			prio = it->priority;
		}
	}
	output->priority = prio;
	wlr_output_set_transform(wlr_output, server->output_transform);
	output->workspaces = NULL;

	wl_list_init(&output->messages);

	if(!wlr_xcursor_manager_load(server->seat->xcursor_manager,
	                             wlr_output->scale)) {
		wlr_log(WLR_ERROR,
		        "Cannot load XCursor theme for output '%s' with scale %f",
		        wlr_output->name, wlr_output->scale);
	}

	output_insert(server, output);
	output_configure(server, output);

	output->workspaces = malloc(server->nws * sizeof(struct cg_workspace *));
	for(unsigned int i = 0; i < server->nws; ++i) {
		output->workspaces[i] = full_screen_workspace(output);
		output->workspaces[i]->num = i;
		if(!output->workspaces[i]) {
			wlr_log(WLR_ERROR, "Failed to allocate workspaces for output");
			return;
		}
		wl_list_init(&output->workspaces[i]->views);
		wl_list_init(&output->workspaces[i]->unmanaged_views);
	}

	wlr_scene_node_raise_to_top(&output->workspaces[0]->scene->node);
	workspace_focus(output,0);

	/* We are the first output. Set the current output to this one. */
	if(server->curr_output == NULL) {
		server->curr_output = output;
	}
	wlr_xcursor_manager_set_cursor_image(server->seat->xcursor_manager,
	                                     DEFAULT_XCURSOR, server->seat->cursor);
	wlr_cursor_warp(server->seat->cursor, NULL, 0, 0);

	output->destroy.notify = handle_output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);
	output->frame.notify = handle_output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
	output->commit.notify = handle_output_commit;
	wl_signal_add(&wlr_output->events.commit, &output->commit);
	output->mode.notify = handle_output_mode;
	wl_signal_add(&wlr_output->events.mode, &output->mode);
	ipc_send_event(server, "new_output(output:%s,priority:%d)",
	               output->wlr_output->name, output->priority);
}
#if CG_HAS_FANALYZE
#pragma GCC diagnostic pop
#endif

void
output_set_window_title(struct cg_output *output, const char *title) {
	struct wlr_output *wlr_output = output->wlr_output;

	if(wlr_output_is_wl(wlr_output)) {
		wlr_wl_output_set_title(wlr_output, title);
#if WLR_HAS_X11_BACKEND
	} else if(wlr_output_is_x11(wlr_output)) {
		wlr_x11_output_set_title(wlr_output, title);
#endif
	}
}
