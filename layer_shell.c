// Copyright 2020 - 2026, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>

#include "layer_shell.h"
#include "output.h"
#include "seat.h"
#include "server.h"

struct cg_layer_surface {
	struct wlr_layer_surface_v1 *wlr_layer_surface;
	struct cg_server *server;
	struct wlr_scene_layer_surface_v1 *scene_surface;

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener commit;
	struct wl_listener new_popup;
};

struct cg_layer_popup {
	struct cg_layer_surface *layer_surface;
	struct wlr_xdg_popup *wlr_popup;
	struct wlr_scene_tree *scene_tree;

	struct wl_listener commit;
	struct wl_listener new_popup;
	struct wl_listener destroy;
};

static struct cg_output *
layer_surface_get_output(struct cg_layer_surface *layer_surface) {
	struct wlr_output *wlr_output = layer_surface->wlr_layer_surface->output;
	return wlr_output != NULL ? wlr_output->data : NULL;
}

/* wlroots does not notify layer surfaces of the destruction of the output
 * they are assigned to, so they are closed along with it */
void
cg_layer_shell_close_output(struct cg_output *output) {
	struct wlr_scene_tree *layers[] = {
	    output->layer_shell_background, output->layer_shell_bottom,
	    output->layer_shell_top, output->layer_shell_overlay};

	for(size_t i = 0; i < sizeof(layers) / sizeof(layers[0]); ++i) {
		struct wlr_scene_node *node, *tmp;
		if(layers[i] == NULL) {
			continue;
		}
		wl_list_for_each_safe(node, tmp, &layers[i]->children, link) {
			struct cg_layer_surface *it = node->data;
			/* Keep the destroy handler from rearranging the output which
			 * is being destroyed */
			it->wlr_layer_surface->output = NULL;
			wlr_layer_surface_v1_destroy(it->wlr_layer_surface);
		}
	}
}

/* Configure the layer surfaces of an output from the bottom layer upwards,
 * subtracting the exclusive zone of each surface from the area which remains
 * usable for the surfaces above it and for the views. */
void
cg_layer_shell_arrange(struct cg_output *output) {
	struct wlr_scene_tree *layers[] = {
	    output->layer_shell_background, output->layer_shell_bottom,
	    output->layer_shell_top, output->layer_shell_overlay};

	struct wlr_box full_area = {0};
	wlr_output_effective_resolution(output->wlr_output, &full_area.width,
	                                &full_area.height);
	struct wlr_box usable_area = full_area;

	for(size_t i = 0; i < sizeof(layers) / sizeof(layers[0]); ++i) {
		struct wlr_scene_node *node;
		if(layers[i] == NULL) {
			continue;
		}
		wl_list_for_each(node, &layers[i]->children, link) {
			struct cg_layer_surface *it = node->data;
			wlr_scene_layer_surface_v1_configure(it->scene_surface, &full_area,
			                                     &usable_area);
		}
	}

	if(!wlr_box_equal(&usable_area, &output->usable_area)) {
		output->usable_area = usable_area;
		/* The size of the tiles changed, so the workspaces are
		 * reinitialized with a fullscreen layout */
		if(output->workspaces != NULL) {
			for(unsigned int i = 0; i < output->server->nws; ++i) {
				output_make_workspace_fullscreen(output, i);
			}
		}
	}
}

static void
handle_layer_surface_map(struct wl_listener *listener,
                         __attribute__((unused)) void *data) {
	// Layer surface is ready to be shown
	struct cg_layer_surface *layer_surface =
	    wl_container_of(listener, layer_surface, map);
	struct wlr_layer_surface_v1 *wlr_layer_surface =
	    layer_surface->wlr_layer_surface;

	wlr_log(WLR_DEBUG,
	        "Layer surface mapped: namespace=%s keyboard_interactive=%d",
	        wlr_layer_surface->namespace,
	        wlr_layer_surface->current.keyboard_interactive);

	// Give keyboard focus if the layer surface wants it
	if(wlr_layer_surface->current.keyboard_interactive ==
	       ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE ||
	   wlr_layer_surface->current.keyboard_interactive ==
	       ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND) {
		struct cg_seat *seat = layer_surface->server->seat;
		if(seat && seat->seat) {
			wlr_seat_keyboard_notify_enter(
			    seat->seat, wlr_layer_surface->surface, NULL, 0, NULL);
			wlr_log(WLR_DEBUG, "Gave keyboard focus to layer surface");
		}
	}

	// The exclusive zone of the surface is taken from the usable area
	struct cg_output *output = layer_surface_get_output(layer_surface);
	if(output != NULL) {
		cg_layer_shell_arrange(output);
	}
}

static void
handle_layer_surface_unmap(struct wl_listener *listener,
                           __attribute__((unused)) void *data) {
	// Layer surface should no longer be shown
	struct cg_layer_surface *layer_surface =
	    wl_container_of(listener, layer_surface, unmap);
	struct wlr_layer_surface_v1 *wlr_layer_surface =
	    layer_surface->wlr_layer_surface;

	wlr_log(WLR_DEBUG, "Layer surface unmapped");

	// Clear keyboard focus if this layer surface had it
	struct cg_seat *seat = layer_surface->server->seat;
	if(seat && seat->seat) {
		struct wlr_surface *focused =
		    seat->seat->keyboard_state.focused_surface;
		if(focused == wlr_layer_surface->surface) {
			wlr_seat_keyboard_clear_focus(seat->seat);
			wlr_log(WLR_DEBUG, "Cleared keyboard focus from layer surface");
		}
	}

	// The exclusive zone of the surface is given back to the usable area
	struct cg_output *output = layer_surface_get_output(layer_surface);
	if(output != NULL) {
		cg_layer_shell_arrange(output);
	}
}

static void
handle_layer_surface_destroy(struct wl_listener *listener,
                             __attribute__((unused)) void *data) {
	struct cg_layer_surface *layer_surface =
	    wl_container_of(listener, layer_surface, destroy);
	struct cg_output *output = layer_surface_get_output(layer_surface);

	wl_list_remove(&layer_surface->map.link);
	wl_list_remove(&layer_surface->unmap.link);
	wl_list_remove(&layer_surface->destroy.link);
	wl_list_remove(&layer_surface->commit.link);
	wl_list_remove(&layer_surface->new_popup.link);

	free(layer_surface);

	if(output != NULL) {
		cg_layer_shell_arrange(output);
	}
}

static void
handle_layer_surface_commit(struct wl_listener *listener,
                            __attribute__((unused)) void *data) {
	struct cg_layer_surface *layer_surface =
	    wl_container_of(listener, layer_surface, commit);
	struct wlr_layer_surface_v1 *wlr_layer_surface =
	    layer_surface->wlr_layer_surface;

	// Rearrange whenever the state which determines the usable area changed
	struct cg_output *output = layer_surface_get_output(layer_surface);
	if(output != NULL && (wlr_layer_surface->initial_commit ||
	                      wlr_layer_surface->current.committed != 0)) {
		cg_layer_shell_arrange(output);
	}
}

static void
layer_popup_unconstrain(struct cg_layer_popup *popup) {
	struct wlr_output *output =
	    popup->layer_surface->wlr_layer_surface->output;
	if(output == NULL) {
		return;
	}
	struct wlr_scene_tree *tree = popup->layer_surface->scene_surface->tree;

	/* The box is relative to the layer surface, which is positioned
	 * relative to the output it belongs to */
	struct wlr_box output_box = {.x = -tree->node.x,
	                             .y = -tree->node.y,
	                             .width = output->width,
	                             .height = output->height};

	wlr_xdg_popup_unconstrain_from_box(popup->wlr_popup, &output_box);
}

static void
handle_layer_popup_destroy(struct wl_listener *listener,
                           __attribute__((unused)) void *data) {
	struct cg_layer_popup *popup = wl_container_of(listener, popup, destroy);

	wl_list_remove(&popup->new_popup.link);
	wl_list_remove(&popup->destroy.link);
	wl_list_remove(&popup->commit.link);
	free(popup);
}

static void
handle_layer_popup_commit(struct wl_listener *listener,
                          __attribute__((unused)) void *data) {
	struct cg_layer_popup *popup = wl_container_of(listener, popup, commit);
	if(popup->wlr_popup->base->initial_commit) {
		layer_popup_unconstrain(popup);
	}
}

static struct cg_layer_popup *
create_layer_popup(struct wlr_xdg_popup *wlr_popup,
                   struct cg_layer_surface *layer_surface,
                   struct wlr_scene_tree *parent_tree);

static void
handle_layer_popup_new_popup(struct wl_listener *listener, void *data) {
	struct cg_layer_popup *popup = wl_container_of(listener, popup, new_popup);
	create_layer_popup(data, popup->layer_surface, popup->scene_tree);
}

static struct cg_layer_popup *
create_layer_popup(struct wlr_xdg_popup *wlr_popup,
                   struct cg_layer_surface *layer_surface,
                   struct wlr_scene_tree *parent_tree) {
	struct cg_layer_popup *popup = calloc(1, sizeof(struct cg_layer_popup));
	if(!popup) {
		return NULL;
	}

	popup->layer_surface = layer_surface;
	popup->wlr_popup = wlr_popup;
	/* The scene tree is destroyed along with the popup surface */
	popup->scene_tree =
	    wlr_scene_xdg_surface_create(parent_tree, wlr_popup->base);
	if(!popup->scene_tree) {
		free(popup);
		return NULL;
	}

	wl_signal_add(&wlr_popup->base->surface->events.commit, &popup->commit);
	popup->commit.notify = handle_layer_popup_commit;
	wl_signal_add(&wlr_popup->base->events.new_popup, &popup->new_popup);
	popup->new_popup.notify = handle_layer_popup_new_popup;
	wl_signal_add(&wlr_popup->events.destroy, &popup->destroy);
	popup->destroy.notify = handle_layer_popup_destroy;
	return popup;
}

static void
handle_new_popup(struct wl_listener *listener, void *data) {
	struct cg_layer_surface *layer_surface =
	    wl_container_of(listener, layer_surface, new_popup);
	wlr_log(WLR_DEBUG, "New popup from layer surface");
	create_layer_popup(data, layer_surface,
	                   layer_surface->scene_surface->tree);
}

static void
handle_new_layer_surface(struct wl_listener *listener, void *data) {
	struct cg_server *server =
	    wl_container_of(listener, server, new_layer_surface);
	struct wlr_layer_surface_v1 *wlr_layer_surface = data;

	wlr_log(WLR_INFO, "New layer surface: namespace %s layer %d",
	        wlr_layer_surface->namespace, wlr_layer_surface->pending.layer);

	// Create our layer surface wrapper
	struct cg_layer_surface *layer_surface =
	    calloc(1, sizeof(struct cg_layer_surface));
	if(!layer_surface) {
		wlr_layer_surface_v1_destroy(wlr_layer_surface);
		return;
	}

	layer_surface->wlr_layer_surface = wlr_layer_surface;
	layer_surface->server = server;

	// If no output was specified, assign the first one
	if(!wlr_layer_surface->output) {
		struct cg_output *output;
		wl_list_for_each(output, &server->outputs, link) {
			wlr_layer_surface->output = output->wlr_output;
			break;
		}

		if(!wlr_layer_surface->output) {
			wlr_log(WLR_ERROR, "No output available for layer surface");
			wlr_layer_surface_v1_destroy(wlr_layer_surface);
			free(layer_surface);
			return;
		}
	}

	// Get the output's layer tree based on the layer surface's layer
	struct cg_output *output = wlr_layer_surface->output->data;
	struct wlr_scene_tree *parent = NULL;

	switch(wlr_layer_surface->pending.layer) {
	case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
		parent = output->layer_shell_background;
		break;
	case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
		parent = output->layer_shell_bottom;
		break;
	case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
		parent = output->layer_shell_top;
		break;
	case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
		parent = output->layer_shell_overlay;
		break;
	}

	if(!parent) {
		wlr_log(WLR_ERROR, "Invalid layer for layer surface");
		wlr_layer_surface_v1_destroy(wlr_layer_surface);
		free(layer_surface);
		return;
	}

	layer_surface->scene_surface =
	    wlr_scene_layer_surface_v1_create(parent, wlr_layer_surface);

	if(!layer_surface->scene_surface) {
		wlr_log(WLR_ERROR, "Failed to create scene layer surface");
		wlr_layer_surface_v1_destroy(wlr_layer_surface);
		free(layer_surface);
		return;
	}
	// The scene tree of a layer is iterated to arrange its surfaces
	layer_surface->scene_surface->tree->node.data = layer_surface;

	// Set up event listeners
	layer_surface->map.notify = handle_layer_surface_map;
	wl_signal_add(&wlr_layer_surface->surface->events.map, &layer_surface->map);

	layer_surface->unmap.notify = handle_layer_surface_unmap;
	wl_signal_add(&wlr_layer_surface->surface->events.unmap,
	              &layer_surface->unmap);

	layer_surface->destroy.notify = handle_layer_surface_destroy;
	wl_signal_add(&wlr_layer_surface->events.destroy, &layer_surface->destroy);

	layer_surface->commit.notify = handle_layer_surface_commit;
	wl_signal_add(&wlr_layer_surface->surface->events.commit,
	              &layer_surface->commit);

	layer_surface->new_popup.notify = handle_new_popup;
	wl_signal_add(&wlr_layer_surface->events.new_popup,
	              &layer_surface->new_popup);

	// Store the layer surface in the wlr_layer_surface data
	wlr_layer_surface->data = layer_surface;
}

void
cg_layer_shell_init(struct cg_server *server) {
	server->layer_shell = wlr_layer_shell_v1_create(server->wl_display, 4);
	if(!server->layer_shell) {
		wlr_log(WLR_ERROR, "Failed to create layer shell");
		return;
	}

	server->new_layer_surface.notify = handle_new_layer_surface;
	wl_signal_add(&server->layer_shell->events.new_surface,
	              &server->new_layer_surface);

	wlr_log(WLR_INFO, "Layer shell support initialized");
}

void
cg_layer_shell_destroy(struct cg_server *server) {
	if(server->layer_shell) {
		wl_list_remove(&server->new_layer_surface.link);
		// wlr_layer_shell_v1 is destroyed automatically with the display
		server->layer_shell = NULL;
	}
}
