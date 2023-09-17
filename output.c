// Copyright 2020 - 2023, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

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
#include <wlr/backend/headless.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xcursor_manager.h>
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
		keybinding_cycle_outputs(server, false, true);
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
				workspace_tile_update_view(tile, NULL);
			}
			struct cg_workspace *ws =
			    server->curr_output
			        ->workspaces[server->curr_output->curr_workspace];
			wl_list_for_each_safe(view, view_tmp, &output->workspaces[i]->views,
			                      link) {
				wl_list_remove(&view->link);
				if(wl_list_empty(&server->outputs)) {
					view->impl->destroy(view);
				} else {
					wl_list_insert(&ws->views, &view->link);
					wlr_scene_node_reparent(&view->scene_tree->node, ws->scene);
					view->workspace = ws;
					view->tile = ws->focused_tile;
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
					wl_list_insert(&ws->unmanaged_views, &view->link);
					wlr_scene_node_reparent(&view->scene_tree->node, ws->scene);
					view->workspace = ws;
					view->tile = ws->focused_tile;
				}
			}
		}
	}
}

int
output_get_num(const struct cg_output *output) {
	struct cg_output *it;
	int count = 1;
	wl_list_for_each(it, &output->server->outputs, link) {
		if(strcmp(output->name, it->name) == 0) {
			return count;
		}
		++count;
	}
	return -1;
}

struct wlr_box
output_get_layout_box(struct cg_output *output) {
	if(!output->destroyed) {
		wlr_output_layout_get_box(output->server->output_layout,
		                          output->wlr_output, &output->layout_box);
	}
	return output->layout_box;
}

static void
output_destroy(struct cg_output *output) {
	struct cg_server *server = output->server;
	char *outp_name = strdup(output->name);
	int outp_num = output_get_num(output);

	if(output->destroyed == false) {
		wl_list_remove(&output->destroy.link);
		wl_list_remove(&output->mode.link);
		wl_list_remove(&output->commit.link);
		wl_list_remove(&output->frame.link);
	}
	output->destroyed = true;
	enum output_role role = output->role;
	if(role == OUTPUT_ROLE_PERMANENT) {
		wlr_output_layout_get_box(server->output_layout, output->wlr_output,
		                          &output->layout_box);
		wlr_output_layout_remove(server->output_layout, output->wlr_output);
		output->wlr_output=wlr_headless_add_output(server->headless_backend,output->layout_box.width,output->layout_box.height);
		wlr_output_layout_add(server->output_layout, output->wlr_output, output->layout_box.x,
		                      output->layout_box.y);

	} else {

		output_clear(output);

		for(unsigned int i = 0; i < server->nws; ++i) {
			workspace_free(output->workspaces[i]);
		}
		free(output->workspaces);
		free(output->name);

		free(output);
	}
	if(outp_name != NULL) {
		ipc_send_event(server,
		               "{\"event_name\":\"destroy_output\",\"output\":\"%s\","
		               "\"output_id\":%d,\"permanent\":%d}",
		               outp_name, outp_num, role == OUTPUT_ROLE_PERMANENT);
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
	struct cg_output *output = wl_container_of(listener, output, frame);
	if(!output->wlr_output->enabled) {
		return;
	}
	struct wlr_scene_output *scene_output =
	    wlr_scene_get_scene_output(output->server->scene, output->wlr_output);
	if(scene_output == NULL) {
		return;
	}
	wlr_scene_output_commit(scene_output);

	struct timespec now = {0};
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

static int
output_set_mode(struct wlr_output *output, int width, int height,
                float refresh_rate) {
	int mhz = (int)(refresh_rate * 1000);

	if(wl_list_empty(&output->modes)) {
		wlr_log(WLR_DEBUG, "Assigning custom mode to %s", output->name);
		wlr_output_set_custom_mode(output, width, height,
		                           refresh_rate > 0 ? mhz : 0);
		return 0;
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
	wlr_output_commit(output);
	if(!wlr_output_test(output)) {
		wlr_log(WLR_ERROR,
		        "Unable to assign configured mode to %s, picking arbitrary "
		        "available mode",
		        output->name);
		struct wlr_output_mode *mode;
		wl_list_for_each(mode, &output->modes, link) {
			if(mode == best) {
				continue;
			}
			wlr_output_set_mode(output, mode);
			wlr_output_commit(output);
			if(wlr_output_test(output)) {
				break;
			}
		}
		if(!wlr_output_test(output)) {
			return 1;
		}
	}
	return 0;
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
output_apply_config(struct cg_server *server, struct cg_output *output,
                    struct cg_output_config *config) {
	struct wlr_output *wlr_output = output->wlr_output;

	wlr_output_layout_get_box(server->output_layout, output->wlr_output,
							  &output->layout_box);
	if(config->role != OUTPUT_ROLE_DEFAULT) {
		output->role = config->role;
		if((output->role == OUTPUT_ROLE_PERIPHERAL)&&(output->destroyed == true)) {
			output_destroy(output);
			wlr_output_destroy(wlr_output);
			return;
		}
	}

	if(output->wlr_output->enabled) {
		wlr_output_layout_remove(server->output_layout, wlr_output);
	}

	if(config->priority != -1) {
		output->priority = config->priority;
	}

	if(config->angle != -1) {
		wlr_output_set_transform(wlr_output, config->angle);
	}
	if(config->scale != -1) {
		wlr_log(WLR_INFO, "Setting output scale to %f", config->scale);
		wlr_output_set_scale(wlr_output, config->scale);
	}
	if(config->pos.x != -1) {
		if(output_set_mode(wlr_output, config->pos.width, config->pos.height,
		                   config->refresh_rate) != 0) {
			wlr_log(WLR_ERROR, "Setting output mode failed, disabling output.");
			output_clear(output);
			wl_list_insert(&server->disabled_outputs, &output->link);
			wlr_output_enable(wlr_output, false);
			wlr_output_commit(wlr_output);
			return;
		}
		wlr_output_layout_add(server->output_layout, wlr_output, config->pos.x,
		                      config->pos.y);
		/* Since the size of the output may have changed, we
		 * reinitialize all workspaces with a fullscreen layout */
		if(output->layout_box.width!=config->pos.width||output->layout_box.height!=config->pos.height) {
			for(unsigned int i = 0; i < output->server->nws; ++i) {
				output_make_workspace_fullscreen(output, i);
			}
		} else {
			wlr_output_layout_get_box(server->output_layout, output->wlr_output,
									  &output->layout_box);
			for(unsigned int i = 0; i < server->nws; ++i) {
				struct cg_workspace *ws=output->workspaces[i];
				bool first = true;
				for(struct cg_tile *tile = ws->focused_tile;
					first || output->workspaces[i]->focused_tile != tile;
					tile = tile->next) {
					first = false;
					if(tile->view != NULL) {
						wlr_scene_node_set_position(
								&tile->view->scene_tree->node,
								tile->view->ox + output->layout_box.x,
								output->layout_box.y);
					}
				}
			}
		}
	} else {
		wlr_output_layout_add_auto(server->output_layout, wlr_output);
		// The following two lines make sure that the output is "manually" managed,
		// so that its position doesn't change anymore in the future.
		wlr_output_layout_get_box(server->output_layout, output->wlr_output,
								  &output->layout_box);
		wlr_output_layout_move(server->output_layout, output->wlr_output, output->layout_box.x,
		                      output->layout_box.y);


		struct wlr_output_mode *preferred_mode =
		    wlr_output_preferred_mode(wlr_output);
		if(preferred_mode) {
			wlr_output_set_mode(wlr_output, preferred_mode);
		}
	}
	/* Refuse to disable the only output */
	if(config->status == OUTPUT_DISABLE &&
	   wl_list_length(&server->outputs) > 1) {
		output_clear(output);
		wl_list_insert(&server->disabled_outputs, &output->link);
		wlr_output_enable(wlr_output, false);
		wlr_output_commit(wlr_output);
	} else {
		wl_list_remove(&output->link);
		output_insert(server, output);
		wlr_output_enable(wlr_output, true);
		wlr_output_commit(wlr_output);
	}

	if(output->bg != NULL) {
		wlr_scene_node_destroy(&output->bg->node);
		output->bg = NULL;
	}
	struct wlr_scene_output *scene_output =
	    wlr_scene_get_scene_output(output->server->scene, output->wlr_output);
	if(scene_output == NULL) {
		return;
	}
	output->bg = wlr_scene_rect_create(
	    &scene_output->scene->tree, output->wlr_output->width,
	    output->wlr_output->height, server->bg_color);
	wlr_output_layout_get_box(server->output_layout, output->wlr_output,
	                          &output->layout_box);
	wlr_scene_node_set_position(&output->bg->node,
	                            output_get_layout_box(output).x,
	                            output_get_layout_box(output).y);
	wlr_scene_node_lower_to_bottom(&output->bg->node);
}

struct cg_output_config *
empty_output_config(void) {
	struct cg_output_config *cfg = calloc(1, sizeof(struct cg_output_config));
	if(cfg == NULL) {
		wlr_log(WLR_ERROR, "Could not allocate output configuration.");
		return NULL;
	}

	cfg->status = OUTPUT_DEFAULT;
	cfg->role = OUTPUT_ROLE_DEFAULT;
	cfg->pos.x = -1;
	cfg->pos.y = -1;
	cfg->pos.width = -1;
	cfg->pos.height = -1;
	cfg->output_name = NULL;
	cfg->refresh_rate = 0;
	cfg->priority = -1;
	cfg->scale = -1;
	cfg->angle = -1;

	return cfg;
}

/* cfg1 has precedence over cfg2 */
struct cg_output_config *
merge_output_configs(struct cg_output_config *cfg1,
                     struct cg_output_config *cfg2) {
	struct cg_output_config *out_cfg = empty_output_config();
	if(cfg1->status == out_cfg->status) {
		out_cfg->status = cfg2->status;
	} else {
		out_cfg->status = cfg1->status;
	}
	if(cfg1->role == out_cfg->role) {
		out_cfg->role = cfg2->role;
	} else {
		out_cfg->role = cfg1->role;
	}
	if(cfg1->pos.x == out_cfg->pos.x) {
		out_cfg->pos.x = cfg2->pos.x;
		out_cfg->pos.y = cfg2->pos.y;
		out_cfg->pos.width = cfg2->pos.width;
		out_cfg->pos.height = cfg2->pos.height;
	} else {
		out_cfg->pos.x = cfg1->pos.x;
		out_cfg->pos.y = cfg1->pos.y;
		out_cfg->pos.width = cfg1->pos.width;
		out_cfg->pos.height = cfg1->pos.height;
	}
	if(cfg1->output_name == NULL) {
		if(cfg2->output_name == NULL) {
			out_cfg->output_name = NULL;
		} else {
			out_cfg->output_name = strdup(cfg2->output_name);
		}
	} else {
		out_cfg->output_name = strdup(cfg1->output_name);
	}
	if(cfg1->refresh_rate == out_cfg->refresh_rate) {
		out_cfg->refresh_rate = cfg2->refresh_rate;
	} else {
		out_cfg->refresh_rate = cfg1->refresh_rate;
	}
	if(cfg1->priority == out_cfg->priority) {
		out_cfg->priority = cfg2->priority;
	} else {
		out_cfg->priority = cfg1->priority;
	}
	if(cfg1->scale == out_cfg->scale) {
		out_cfg->scale = cfg2->scale;
	} else {
		out_cfg->scale = cfg1->scale;
	}
	if(cfg1->angle == out_cfg->angle) {
		out_cfg->angle = cfg2->angle;
	} else {
		out_cfg->angle = cfg1->angle;
	}
	return out_cfg;
}

void
output_configure(struct cg_server *server, struct cg_output *output) {
	struct cg_output_config *tot_config = empty_output_config();
	struct cg_output_config *config;
	wl_list_for_each(config, &server->output_config, link) {
		if(strcmp(config->output_name, output->name) == 0) {
			if(tot_config == NULL) {
				return;
			}
			struct cg_output_config *prev_config = tot_config;
			tot_config = merge_output_configs(config, tot_config);
			if(prev_config->output_name != NULL) {
				free(prev_config->output_name);
			}
			free(prev_config);
		}
	}
	if(tot_config != NULL) {
		output_apply_config(server, output, tot_config);
	}
	free(tot_config->output_name);
	free(tot_config);
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

void
output_make_workspace_fullscreen(struct cg_output *output, int ws) {
	struct cg_server *server = output->server;
	struct cg_view *current_view = seat_get_focus(server->seat);

	if(current_view == NULL || ws != output->curr_workspace) {
		struct cg_view *it = NULL;
		wl_list_for_each(it, &output->workspaces[ws]->views, link) {
			if(view_is_visible(it)) {
				current_view = it;
				break;
			}
		}
	}

	workspace_free_tiles(output->workspaces[ws]);
	if(full_screen_workspace_tiles(server->output_layout,
	                               output->workspaces[ws],
	                               &server->tiles_curr_id) != 0) {
		wlr_log(WLR_ERROR, "Failed to allocate space for fullscreen workspace");
		return;
	}

	struct cg_view *it_view;
	wl_list_for_each(it_view, &output->workspaces[ws]->views, link) {
		it_view->tile = output->workspaces[ws]->focused_tile;
	}

	if(ws == output->curr_workspace) {
		seat_set_focus(server->seat, current_view);
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

	if(!wlr_output_init_render(wlr_output, server->allocator,
	                           server->renderer)) {
		wlr_log(WLR_ERROR, "Failed to initialize output rendering");
		return;
	}

	struct cg_output *ito;
	bool reinit = false;
	struct cg_output *output = NULL;
	wl_list_for_each(ito, &server->outputs, link) {
		if((strcmp(ito->name, wlr_output->name) == 0) && (ito->destroyed)) {
			reinit = true;
			output = ito;
		}
	}
	if(reinit) {
		wlr_output_layout_remove(server->output_layout, output->wlr_output);
		wlr_output_destroy(output->wlr_output);
	} else {
		output = calloc(1, sizeof(struct cg_output));
	}
	if(!output) {
		wlr_log(WLR_ERROR, "Failed to allocate output");
		return;
	}

	output->wlr_output = wlr_output;
	output->destroyed = false;

	if(!reinit) {
		output->server = server;
		output->last_scanned_out_view = NULL;
		output->name = strdup(wlr_output->name);

		struct cg_output_priorities *it;
		int prio = -1;
		wl_list_for_each(it, &server->output_priorities, link) {
			if(strcmp(output->name, it->ident) == 0) {
				prio = it->priority;
			}
		}
		output->priority = prio;
		wlr_output_set_transform(wlr_output, WL_OUTPUT_TRANSFORM_NORMAL);
		output->workspaces = NULL;

		wl_list_init(&output->messages);

		if(!wlr_xcursor_manager_load(server->seat->xcursor_manager,
		                             wlr_output->scale)) {
			wlr_log(WLR_ERROR,
			        "Cannot load XCursor theme for output '%s' with scale %f",
			        output->name, wlr_output->scale);
		}

		output_insert(server, output);
		output_configure(server, output);
		wlr_output_layout_get_box(server->output_layout, output->wlr_output,
		                          &output->layout_box);

		output->workspaces =
		    malloc(server->nws * sizeof(struct cg_workspace *));
		for(unsigned int i = 0; i < server->nws; ++i) {
			output->workspaces[i] = full_screen_workspace(output);
			if(!output->workspaces[i]) {
				wlr_log(WLR_ERROR, "Failed to allocate workspaces for output");
				return;
			}
			output->workspaces[i]->num = i;
			wl_list_init(&output->workspaces[i]->views);
			wl_list_init(&output->workspaces[i]->unmanaged_views);
		}

		wlr_scene_node_raise_to_top(&output->workspaces[0]->scene->node);
		workspace_focus(output, 0);

		/* We are the first output. Set the current output to this one. */
		if(server->curr_output == NULL) {
			server->curr_output = output;
		}
	} else {
		wlr_output_layout_add(server->output_layout, wlr_output,
		                      output_get_layout_box(output).x,
		                      output_get_layout_box(output).y);
		output_configure(server, output);
		wlr_output_layout_get_box(server->output_layout, output->wlr_output,
		                          &output->layout_box);
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

	ipc_send_event(server,
	               "{\"event_name\":\"new_output\",\"output\":\"%s\",\"output_"
	               "id\":%d,\"priority\":%d,\"restart\":%d}",
	               output->name, output_get_num(output), output->priority,
	               reinit);
}
#if CG_HAS_FANALYZE
#pragma GCC diagnostic pop
#endif
