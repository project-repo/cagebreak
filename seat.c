// Copyright 2020 - 2023, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#define _POSIX_C_SOURCE 200812L

#include "config.h"

#include <linux/input-event-codes.h>
#include <stdint.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/util/log.h>
#if CG_HAS_XWAYLAND
#include <wlr/xwayland.h>
#endif

#include "input_manager.h"
#include "keybinding.h"
#include "message.h"
#include "output.h"
#include "seat.h"
#include "server.h"
#include "view.h"
#include "workspace.h"
#if CG_HAS_XWAYLAND
#include "xwayland.h"
#endif

static void
drag_icon_update_position(struct cg_drag_icon *drag_icon);

static void
update_capabilities(const struct cg_seat *seat) {
	uint32_t caps = 0;

	if(seat->num_keyboards > 0) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	if(seat->num_pointers > 0) {
		caps |= WL_SEAT_CAPABILITY_POINTER;
	}
	if(seat->num_touch > 0) {
		caps |= WL_SEAT_CAPABILITY_TOUCH;
	}
	wlr_seat_set_capabilities(seat->seat, caps);

	/* Hide cursor if the seat doesn't have pointer capability. */
	if(((caps & WL_SEAT_CAPABILITY_POINTER) == 0) ||
	   seat->enable_cursor == false) {
		wlr_cursor_unset_image(seat->cursor);
	} else {
		wlr_cursor_set_xcursor(seat->cursor, seat->xcursor_manager, DEFAULT_XCURSOR);
	}
}

static void
remove_touch(struct cg_seat *seat, struct cg_touch *touch) {
	if(!touch) {
		return;
	}
	wlr_cursor_detach_input_device(seat->cursor, touch->device->wlr_device);
	--seat->num_touch;
	free(touch);
}

static void
new_touch(struct cg_seat *seat, struct cg_input_device *input_device) {
	struct wlr_input_device *device = input_device->wlr_device;
	struct wlr_touch *wlr_touch = wlr_touch_from_input_device(device);
	struct cg_touch *touch = calloc(1, sizeof(struct cg_touch));
	if(!touch) {
		wlr_log(WLR_ERROR, "Cannot allocate touch");
		return;
	}

	touch->seat = seat;
	touch->device = input_device;
	wlr_cursor_attach_input_device(seat->cursor, device);
	input_device->touch = touch;
	++seat->num_touch;

	if(wlr_touch->output_name != NULL) {
		struct cg_output *output;
		wl_list_for_each(output, &seat->server->outputs, link) {
			if(strcmp(wlr_touch->output_name, output->name) == 0) {
				wlr_cursor_map_input_to_output(seat->cursor, device,
				                               output->wlr_output);
				break;
			}
		}
	}
}

static void
remove_pointer(struct cg_seat *seat, struct cg_pointer *pointer) {
	if(!pointer) {
		return;
	}
	wlr_cursor_detach_input_device(seat->cursor, pointer->device->wlr_device);
	--seat->num_pointers;
	free(pointer);
}

static void
new_pointer(struct cg_seat *seat, struct cg_input_device *input_device) {
	struct wlr_input_device *device = input_device->wlr_device;
	struct wlr_pointer *wlr_pointer = wlr_pointer_from_input_device(device);
	struct cg_pointer *pointer = calloc(1, sizeof(struct cg_pointer));
	if(!pointer) {
		wlr_log(WLR_ERROR, "Cannot allocate pointer");
		return;
	}

	pointer->seat = seat;
	pointer->device = input_device;
	wlr_cursor_attach_input_device(seat->cursor, device);
	input_device->pointer = pointer;
	++seat->num_pointers;

	if(wlr_pointer->output_name != NULL) {
		struct cg_output *output;
		wl_list_for_each(output, &seat->server->outputs, link) {
			if(strcmp(wlr_pointer->output_name, output->name) == 0) {
				wlr_cursor_map_input_to_output(seat->cursor, device,
				                               output->wlr_output);
				break;
			}
		}
	}
}

static int
handle_keyboard_repeat(void *data) {
	struct cg_keyboard_group *cg_group = data;
	struct wlr_keyboard *wlr_device = &cg_group->wlr_group->keyboard;
	if(cg_group->repeat_keybinding != NULL) {
		if(wlr_device->repeat_info.rate > 0) {
			if(wl_event_source_timer_update(
			       cg_group->key_repeat_timer,
			       1000 / wlr_device->repeat_info.rate) < 0) {
				wlr_log(WLR_DEBUG, "failed to update key repeat timer");
			}
		}

		run_action((*cg_group->repeat_keybinding)->action,
		           cg_group->seat->server,
		           (*cg_group->repeat_keybinding)->data);
	}
	return 0;
}

static void
handle_modifier_event(struct wlr_input_device *device, struct cg_seat *seat) {
	struct wlr_keyboard *keyboard = wlr_keyboard_from_input_device(device);
	wlr_seat_set_keyboard(seat->seat, keyboard);
	wlr_seat_keyboard_notify_modifiers(seat->seat, &keyboard->modifiers);

	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

void
keyboard_disarm_key_repeat(struct cg_keyboard_group *group) {
	if(!group) {
		return;
	}
	group->repeat_keybinding = NULL;
	if(wl_event_source_timer_update(group->key_repeat_timer, 0) < 0) {
		wlr_log(WLR_DEBUG, "failed to disarm key repeat timer");
	}
}

static bool
handle_command_key_bindings(struct cg_server *server, xkb_keysym_t sym,
                            uint32_t modifiers, uint32_t mode,
                            struct cg_keyboard_group *group) {
	if(group->enable_keybindings == false) {
		return false;
	}
	struct keybinding **keybinding = find_keybinding(
	    server->keybindings,
	    &(struct keybinding){.key = sym, .mode = mode, .modifiers = modifiers});
	if(server->seat->mode != server->seat->default_mode) {
		server->seat->mode =
		    server->seat
		        ->default_mode; // Return to mode we are currently in by default
		double sx, sy;
		struct wlr_seat *wlr_seat = server->seat->seat;
		struct wlr_surface *surface = NULL;

		struct wlr_scene_node *node = wlr_scene_node_at(
		    &server->scene->tree.node, server->seat->cursor->x,
		    server->seat->cursor->y, &sx, &sy);
		if(server->seat->enable_cursor) {
			wlr_cursor_set_xcursor(server->seat->cursor, server->seat->xcursor_manager,
	                                     "left_ptr");
			if(node && node->type == WLR_SCENE_NODE_BUFFER) {
				struct wlr_scene_surface *scene_surface =
				    wlr_scene_surface_try_from_buffer(
				        wlr_scene_buffer_from_node(node));
				if(scene_surface != NULL) {
					surface = scene_surface->surface;
					if(surface != NULL) {
						wlr_seat_pointer_notify_enter(wlr_seat, surface, sx,
						                              sy);
					}
				}
			}
		}
	}

	if(keybinding) {
		wlr_log(
		    WLR_DEBUG,
		    "Recognized keybinding pressed (key: %d, mode: %d, modifiers: %d)",
		    sym, mode, modifiers);

		if(group->wlr_group->keyboard.repeat_info.delay > 0) {
			group->repeat_keybinding = keybinding;
			if(wl_event_source_timer_update(
			       group->key_repeat_timer,
			       group->wlr_group->keyboard.repeat_info.delay) < 0) {
				wlr_log(WLR_DEBUG, "failed to set key repeat timer");
			}
		}
		message_clear(group->seat->server->curr_output);
		run_action((*keybinding)->action, server, (*keybinding)->data);
		wlr_idle_notifier_v1_notify_activity(server->idle, server->seat->seat);
		return true;
	} else if(mode != 0) {
		run_action(KEYBINDING_NOOP, server, (union keybinding_params){NULL});
		message_printf(server->curr_output, "unbound key pressed");
		wlr_log(WLR_DEBUG,
		        "Unknown keybinding pressed (key: %d, mode: %d, modifiers: %d)",
		        sym, mode, modifiers);
		return true;
	} else {
		return false;
	}
}

static bool
key_is_modifier(const xkb_keysym_t key) {
	switch(key) {
	case XKB_KEY_Shift_L:
	case XKB_KEY_Shift_R:
	case XKB_KEY_Control_L:
	case XKB_KEY_Control_R:
	case XKB_KEY_Alt_L:
	case XKB_KEY_Alt_R:
	case XKB_KEY_Super_L:
	case XKB_KEY_Super_R:
	case XKB_KEY_Hyper_L:
	case XKB_KEY_Hyper_R:
		return true;
	default:
		return false;
	}
}

static void
handle_key_event(struct cg_keyboard_group *group, struct cg_seat *seat,
                 void *data) {
	struct wlr_keyboard_key_event *event = data;
	struct wlr_keyboard *keyboard = &group->wlr_group->keyboard;

	/* Translate from libinput keycode to an xkbcommon keycode. */
	xkb_keycode_t keycode = event->keycode + 8;

	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(keyboard->xkb_state, keycode, &syms);

	bool handled = false;

	for(int i = 0; i < nsyms; ++i) {
		if(event->state == WL_KEYBOARD_KEY_STATE_PRESSED &&
		   !key_is_modifier(syms[i])) {
			uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard);
			/* Get the consumed_modifiers and remove them from the modifier list
			 */
			xkb_mod_mask_t consumed_modifiers =
			    xkb_state_key_get_consumed_mods2(keyboard->xkb_state, keycode,
			                                     XKB_CONSUMED_MODE_GTK);
			if(handle_command_key_bindings(seat->server, syms[i],
			                               modifiers & ~consumed_modifiers,
			                               seat->mode, group)) {
				handled = true;
			}
		} else if(group->repeat_keybinding != NULL && handled == false) {
			keyboard_disarm_key_repeat(group);
		}
	}

	if(!handled) {
		/* Otherwise, we pass it along to the client. */
		wlr_seat_set_keyboard(seat->seat, keyboard);
		wlr_seat_keyboard_notify_key(seat->seat, event->time_msec,
		                             event->keycode, event->state);
	}

	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
handle_keyboard_group_key(struct wl_listener *listener, void *data) {
	struct cg_keyboard_group *cg_group =
	    wl_container_of(listener, cg_group, key);
	handle_key_event(cg_group, cg_group->seat, data);
}

static void
handle_keyboard_group_modifiers(struct wl_listener *listener, void *_data) {
	struct cg_keyboard_group *group =
	    wl_container_of(listener, group, modifiers);
	handle_modifier_event(&group->wlr_group->keyboard.base, group->seat);
}

static bool
repeat_info_match(struct wlr_keyboard *a, struct wlr_keyboard *b) {
	return a->repeat_info.rate == b->repeat_info.rate &&
	       a->repeat_info.delay == b->repeat_info.delay;
}

static void
cg_keyboard_group_add(struct cg_input_device *input_device,
                      struct cg_seat *seat) {
	struct wlr_input_device *device = input_device->wlr_device;
	struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);
	// Virtual devices should not be grouped
	if(!input_device->is_virtual) {
		struct cg_keyboard_group *group;
		wl_list_for_each(group, &seat->keyboard_groups, link) {
			struct wlr_keyboard_group *wlr_group = group->wlr_group;
			if(wlr_keyboard_keymaps_match(wlr_keyboard->keymap,
			                              wlr_group->keyboard.keymap) &&
			   repeat_info_match(wlr_keyboard, &wlr_group->keyboard) &&
			   wlr_keyboard_group_add_keyboard(wlr_group, wlr_keyboard)) {
				wlr_log(WLR_DEBUG, "Adding keyboard to existing group.");
				return;
			}
		}
	}
	/* This is reached if and only if the keyboard could not be inserted into
	 * any group */
	struct cg_keyboard_group *cg_group =
	    calloc(1, sizeof(struct cg_keyboard_group));
	if(cg_group == NULL) {
		wlr_log(WLR_ERROR, "Failed to allocate keyboard group.");
		return;
	}
	cg_group->seat = seat;
	cg_group->wlr_group = wlr_keyboard_group_create();
	if(cg_group->wlr_group == NULL) {
		wlr_log(WLR_ERROR, "Failed to create wlr keyboard group.");
		goto cleanup;
	}

	cg_group->wlr_group->data = cg_group;
	wlr_keyboard_set_keymap(&cg_group->wlr_group->keyboard,
	                        wlr_keyboard->keymap);
	wlr_keyboard_set_repeat_info(&cg_group->wlr_group->keyboard,
	                             wlr_keyboard->repeat_info.rate,
	                             wlr_keyboard->repeat_info.delay);
	if(input_device->identifier != NULL) {
		cg_group->identifier = strdup(input_device->identifier);
	} else {
		cg_group->identifier = NULL;
	}
	wlr_log(WLR_DEBUG, "Created keyboard group.");

	wlr_keyboard_group_add_keyboard(cg_group->wlr_group, wlr_keyboard);
	wl_list_insert(&seat->keyboard_groups, &cg_group->link);

	wl_signal_add(&cg_group->wlr_group->keyboard.events.key, &cg_group->key);
	cg_group->key.notify = handle_keyboard_group_key;

	wl_signal_add(&cg_group->wlr_group->keyboard.events.modifiers,
	              &cg_group->modifiers);

	cg_group->key_repeat_timer = wl_event_loop_add_timer(
	    seat->server->event_loop, handle_keyboard_repeat, cg_group);

	cg_group->modifiers.notify = handle_keyboard_group_modifiers;
	cg_group->enable_keybindings = true;
	cg_input_manager_configure_keyboard_group(cg_group);
	return;

cleanup:
	if(cg_group && cg_group->wlr_group) {
		wlr_keyboard_group_destroy(cg_group->wlr_group);
	}
	if(cg_group && cg_group->identifier) {
		free(cg_group->identifier);
	}
	free(cg_group);
}

static void
new_keyboard(struct cg_seat *seat, struct cg_input_device *input_device) {
	struct wlr_input_device *device = input_device->wlr_device;
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if(!context) {
		wlr_log(WLR_ERROR, "Unable to create XKB context");
		return;
	}

	struct xkb_keymap *keymap =
	    xkb_map_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if(!keymap) {
		wlr_log(WLR_ERROR,
		        "Unable to configure keyboard: keymap does not exist");
		xkb_context_unref(context);
		return;
	}

	wlr_keyboard_set_keymap(wlr_keyboard_from_input_device(device), keymap);

	xkb_keymap_unref(keymap);
	xkb_context_unref(context);

	cg_keyboard_group_add(input_device, seat);
	++seat->num_keyboards;

	wlr_seat_set_keyboard(seat->seat, wlr_keyboard_from_input_device(device));
}

void
seat_add_device(struct cg_seat *seat, struct cg_input_device *device) {
	switch(device->wlr_device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		new_keyboard(seat, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		new_pointer(seat, device);
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		new_touch(seat, device);
		break;
	case WLR_INPUT_DEVICE_SWITCH:
		wlr_log(WLR_DEBUG, "Switch input is not implemented");
		return;
	case WLR_INPUT_DEVICE_TABLET_TOOL:
	case WLR_INPUT_DEVICE_TABLET_PAD:
		wlr_log(WLR_DEBUG, "Tablet input is not implemented");
		return;
	}

	update_capabilities(seat);
}

static void
destroy_empty_wlr_keyboard_group(void *data) {
	wlr_keyboard_group_destroy(data);
}

void
remove_keyboard(struct cg_seat *seat, struct cg_input_device *keyboard) {
	if(!keyboard) {
		return;
	}
	struct wlr_seat *wlr_seat = seat->seat;
	struct wlr_keyboard *wlr_keyboard =
	    wlr_keyboard_from_input_device(keyboard->wlr_device);
	struct wlr_keyboard_group *wlr_group = wlr_keyboard->group;
	if(wlr_group) {
		wlr_keyboard_group_remove_keyboard(wlr_group, wlr_keyboard);

		if(wl_list_empty(&wlr_group->devices)) {
			wlr_log(WLR_DEBUG, "Destroying empty keyboard group %p",
			        (void *)wlr_group);
			struct cg_keyboard_group *group = wlr_group->data;
			if(wlr_seat_get_keyboard(wlr_seat) == &wlr_group->keyboard) {
				wlr_seat_set_keyboard(wlr_seat, NULL);
			}
			wlr_group->data = NULL;
			wl_list_remove(&group->link);
			wl_list_remove(&group->key.link);
			wl_list_remove(&group->modifiers.link);
			if(group->identifier != NULL) {
				free(group->identifier);
			}
			free(group);

			// To prevent use-after-free conditions when handling key events,
			// defer freeing the wlr_keyboard_group until idle
			wl_event_loop_add_idle(seat->server->event_loop,
			                       destroy_empty_wlr_keyboard_group, wlr_group);
		}
	}

	--seat->num_keyboards;
}

void
seat_remove_device(struct cg_seat *seat, struct cg_input_device *device) {
	switch(device->wlr_device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		remove_keyboard(seat, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		remove_pointer(seat, device->pointer);
		device->pointer = NULL;
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		remove_touch(seat, device->touch);
		device->touch = NULL;
		break;
	case WLR_INPUT_DEVICE_SWITCH:
		wlr_log(WLR_DEBUG, "Switch input is not implemented");
		return;
	case WLR_INPUT_DEVICE_TABLET_TOOL:
	case WLR_INPUT_DEVICE_TABLET_PAD:
		wlr_log(WLR_DEBUG, "Tablet input is not implemented");
		return;
	}

	update_capabilities(seat);
}

static void
handle_request_set_primary_selection(struct wl_listener *listener, void *data) {
	struct cg_seat *seat =
	    wl_container_of(listener, seat, request_set_primary_selection);
	struct wlr_seat_request_set_primary_selection_event *event = data;

	wlr_seat_set_primary_selection(seat->seat, event->source, event->serial);
}

static void
handle_request_set_selection(struct wl_listener *listener, void *data) {
	struct cg_seat *seat =
	    wl_container_of(listener, seat, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;

	wlr_seat_set_selection(seat->seat, event->source, event->serial);
}

static void
handle_request_set_cursor(struct wl_listener *listener, void *data) {
	struct cg_seat *seat = wl_container_of(listener, seat, request_set_cursor);
	if(seat->enable_cursor == false || seat->mode != seat->default_mode) {
		return;
	}
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_surface *focused_surface =
	    event->seat_client->seat->pointer_state.focused_surface;
	bool has_focused =
	    focused_surface != NULL && focused_surface->resource != NULL;
	struct wl_client *focused_client = NULL;
	if(has_focused) {
		focused_client = wl_resource_get_client(focused_surface->resource);
	}

	/* This can be sent by any client, so we check to make sure
	 * this one actually has pointer focus first. */
	if(focused_client == event->seat_client->client) {
		wlr_cursor_set_surface(seat->cursor, event->surface, event->hotspot_x,
		                       event->hotspot_y);
	}
}

static void
handle_touch_down(struct wl_listener *listener, void *data) {
	struct cg_seat *seat = wl_container_of(listener, seat, touch_down);
	struct wlr_touch_down_event *event = data;

	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(seat->cursor, &event->touch->base,
	                                     event->x, event->y, &lx, &ly);

	double sx, sy;
	struct wlr_scene_node *node =
	    wlr_scene_node_at(&seat->server->scene->tree.node, lx, ly, &sx, &sy);

	uint32_t serial = 0;
	if(node && node->type == WLR_SCENE_NODE_BUFFER) {
		struct wlr_scene_surface *scene_surface =
		    wlr_scene_surface_try_from_buffer(wlr_scene_buffer_from_node(node));
		if(scene_surface != NULL) {
			serial = wlr_seat_touch_notify_down(
			    seat->seat, scene_surface->surface, event->time_msec,
			    event->touch_id, sx, sy);
		}
	}

	if(serial && wlr_seat_touch_num_points(seat->seat) == 1) {
		seat->touch_id = event->touch_id;
		seat->touch_lx = lx;
		seat->touch_ly = ly;
	}

	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
handle_touch_up(struct wl_listener *listener, void *data) {
	struct cg_seat *seat = wl_container_of(listener, seat, touch_up);
	struct wlr_touch_up_event *event = data;

	if(!wlr_seat_touch_get_point(seat->seat, event->touch_id)) {
		return;
	}

	wlr_seat_touch_notify_up(seat->seat, event->time_msec, event->touch_id);
	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
handle_touch_motion(struct wl_listener *listener, void *data) {
	struct cg_seat *seat = wl_container_of(listener, seat, touch_motion);
	struct wlr_touch_motion_event *event = data;

	if(!wlr_seat_touch_get_point(seat->seat, event->touch_id)) {
		return;
	}

	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(seat->cursor, &event->touch->base,
	                                     event->x, event->y, &lx, &ly);

	double sx, sy;
	struct wlr_scene_node *node =
	    wlr_scene_node_at(&seat->server->scene->tree.node, lx, ly, &sx, &sy);

	if(node && node->type == WLR_SCENE_NODE_BUFFER) {
		struct wlr_scene_surface *scene_surface =
		    wlr_scene_surface_try_from_buffer(wlr_scene_buffer_from_node(node));
		if(scene_surface != NULL) {

			wlr_seat_touch_point_focus(seat->seat, scene_surface->surface,
			                           event->time_msec, event->touch_id, sx,
			                           sy);
		}
		wlr_seat_touch_notify_motion(seat->seat, event->time_msec,
		                             event->touch_id, sx, sy);
	} else {
		wlr_seat_touch_point_clear_focus(seat->seat, event->time_msec,
		                                 event->touch_id);
	}

	if(event->touch_id == seat->touch_id) {
		seat->touch_lx = lx;
		seat->touch_ly = ly;
	}

	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
handle_cursor_frame(struct wl_listener *listener, void *_data) {
	struct cg_seat *seat = wl_container_of(listener, seat, cursor_frame);

	wlr_seat_pointer_notify_frame(seat->seat);
	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
handle_cursor_axis(struct wl_listener *listener, void *data) {
	struct cg_seat *seat = wl_container_of(listener, seat, cursor_axis);
	struct wlr_pointer_axis_event *event = data;

	wlr_seat_pointer_notify_axis(seat->seat, event->time_msec,
	                             event->orientation, event->delta,
	                             event->delta_discrete, event->source);
	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
handle_cursor_button(struct wl_listener *listener, void *data) {
	struct cg_seat *seat = wl_container_of(listener, seat, cursor_button);
	struct wlr_pointer_button_event *event = data;

	wlr_seat_pointer_notify_button(seat->seat, event->time_msec, event->button,
	                               event->state);
	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
process_cursor_motion(struct cg_seat *seat, uint32_t time) {
	double sx, sy;
	struct wlr_seat *wlr_seat = seat->seat;
	struct wlr_surface *surface = NULL;

	struct wlr_scene_node *node =
	    wlr_scene_node_at(&seat->server->scene->tree.node, seat->cursor->x,
	                      seat->cursor->y, &sx, &sy);

	if(node && node->type == WLR_SCENE_NODE_BUFFER) {
		struct wlr_scene_surface *scene_surface =
		    wlr_scene_surface_try_from_buffer(wlr_scene_buffer_from_node(node));
		if(scene_surface != NULL) {
			surface = scene_surface->surface;
			if(surface != NULL) {
				wlr_seat_pointer_notify_enter(wlr_seat, surface, sx, sy);
			}
		}

		bool focus_changed = wlr_seat->pointer_state.focused_surface != surface;
		if(!focus_changed && time > 0) {
			wlr_seat_pointer_notify_motion(wlr_seat, time, sx, sy);
		}
	} else {
		wlr_seat_pointer_clear_focus(wlr_seat);
	}

	struct cg_drag_icon *drag_icon;
	wl_list_for_each(drag_icon, &seat->drag_icons, link) {
		drag_icon_update_position(drag_icon);
	}

	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);

	/* Check if cursor switched tile */
	struct wlr_output *c_outp = wlr_output_layout_output_at(
	    seat->server->output_layout, seat->cursor->x, seat->cursor->y);
	struct cg_output *cg_outp = NULL;
	wl_list_for_each(cg_outp, &seat->server->outputs, link) {
		if(cg_outp->wlr_output == c_outp) {
			break;
		}
	}
	struct cg_tile *c_tile;
	bool first = true;
	for(c_tile = cg_outp->workspaces[cg_outp->curr_workspace]->focused_tile;
	    first ||
	    c_tile != cg_outp->workspaces[cg_outp->curr_workspace]->focused_tile;
	    c_tile = c_tile->next) {
		first = false;
		double ox = seat->cursor->x, oy = seat->cursor->y;
		wlr_output_layout_output_coords(seat->server->output_layout, c_outp,
		                                &ox, &oy);
		if(c_tile->tile.x <= ox && c_tile->tile.y <= oy &&
		   c_tile->tile.x + c_tile->tile.width >= ox &&
		   c_tile->tile.y + c_tile->tile.height >= oy) {
			break;
		}
	}
	if(seat->cursor_tile != NULL && seat->cursor_tile != c_tile &&
	   seat->server->running) {
		ipc_send_event(seat->server,
		               "{\"event_name\":\"cursor_switch_tile\",\"old_output\":"
		               "\"%s\",\"old_output_id\":%d,"
		               "\"old_tile\":%d,\"new_output\":\"%s\",\"new_output_"
		               "id\":%d,\"new_tile\":%d}",
		               seat->cursor_tile->workspace->output->name,
		               output_get_num(seat->cursor_tile->workspace->output),
		               seat->cursor_tile->id, c_outp->name,
		               output_get_num(cg_outp), c_tile->id);
	}
	seat->cursor_tile = c_tile;
}

static void
handle_cursor_motion_absolute(struct wl_listener *listener, void *data) {
	struct cg_seat *seat =
	    wl_container_of(listener, seat, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;

	wlr_cursor_warp_absolute(seat->cursor, &event->pointer->base, event->x,
	                         event->y);
	process_cursor_motion(seat, event->time_msec);
	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
handle_cursor_motion(struct wl_listener *listener, void *data) {
	struct cg_seat *seat = wl_container_of(listener, seat, cursor_motion);
	struct wlr_pointer_motion_event *event = data;

	wlr_cursor_move(seat->cursor, &event->pointer->base, event->delta_x,
	                event->delta_y);
	process_cursor_motion(seat, event->time_msec);
	wlr_idle_notifier_v1_notify_activity(seat->server->idle, seat->seat);
}

static void
drag_icon_update_position(struct cg_drag_icon *drag_icon) {
	struct wlr_drag_icon *wlr_icon = drag_icon->wlr_drag_icon;
	struct cg_seat *seat = drag_icon->seat;
	struct wlr_touch_point *point;

	switch(wlr_icon->drag->grab_type) {
	case WLR_DRAG_GRAB_KEYBOARD:
		return;
	case WLR_DRAG_GRAB_KEYBOARD_POINTER:
		drag_icon->lx = seat->cursor->x;
		drag_icon->ly = seat->cursor->y;
		break;
	case WLR_DRAG_GRAB_KEYBOARD_TOUCH:
		point = wlr_seat_touch_get_point(seat->seat, wlr_icon->drag->touch_id);
		if(!point) {
			return;
		}
		drag_icon->lx = seat->touch_lx;
		drag_icon->ly = seat->touch_ly;
		break;
	}
	wlr_scene_node_set_position(&drag_icon->scene_tree->node, drag_icon->lx,
	                            drag_icon->ly);
}

static void
handle_drag_icon_destroy(struct wl_listener *listener, void *_data) {
	struct cg_drag_icon *drag_icon =
	    wl_container_of(listener, drag_icon, destroy);

	wl_list_remove(&drag_icon->link);
	wl_list_remove(&drag_icon->destroy.link);
	wlr_scene_node_destroy(&drag_icon->scene_tree->node);
	free(drag_icon);
}

static void
handle_request_start_drag(struct wl_listener *listener, void *data) {
	struct cg_seat *seat = wl_container_of(listener, seat, request_start_drag);
	struct wlr_seat_request_start_drag_event *event = data;

	if(wlr_seat_validate_pointer_grab_serial(seat->seat, event->origin,
	                                         event->serial)) {
		wlr_seat_start_pointer_drag(seat->seat, event->drag, event->serial);
		return;
	}

	struct wlr_touch_point *point;
	if(wlr_seat_validate_touch_grab_serial(seat->seat, event->origin,
	                                       event->serial, &point)) {
		wlr_seat_start_touch_drag(seat->seat, event->drag, event->serial,
		                          point);
		return;
	}

	wlr_log(WLR_DEBUG,
	        "Ignoring start_drag request: "
	        "could not validate pointer/touch serial %" PRIu32,
	        event->serial);
	wlr_data_source_destroy(event->drag->source);
}

static void
handle_start_drag(struct wl_listener *listener, void *data) {
	struct cg_seat *seat = wl_container_of(listener, seat, start_drag);
	struct wlr_drag *wlr_drag = data;
	struct wlr_drag_icon *wlr_drag_icon = wlr_drag->icon;
	if(wlr_drag_icon == NULL) {
		return;
	}

	struct cg_drag_icon *drag_icon = calloc(1, sizeof(struct cg_drag_icon));
	if(!drag_icon) {
		return;
	}
	drag_icon->seat = seat;
	drag_icon->wlr_drag_icon = wlr_drag_icon;
	drag_icon->scene_tree = wlr_scene_subsurface_tree_create(
	    &seat->server->scene->tree, wlr_drag_icon->surface);
	if(!drag_icon->scene_tree) {
		free(drag_icon);
		return;
	}

	drag_icon->destroy.notify = handle_drag_icon_destroy;
	wl_signal_add(&wlr_drag_icon->events.destroy, &drag_icon->destroy);

	wl_list_insert(&seat->drag_icons, &drag_icon->link);

	drag_icon_update_position(drag_icon);
}

static void
handle_destroy(struct wl_listener *listener, void *_data) {
	struct cg_seat *seat = wl_container_of(listener, seat, destroy);
	wl_list_remove(&seat->destroy.link);

	struct cg_keyboard_group *group, *group_tmp;
	wl_list_for_each_safe(group, group_tmp, &seat->keyboard_groups, link) {
		wl_list_remove(&group->link);
		wlr_keyboard_group_destroy(group->wlr_group);
		wl_event_source_remove(group->key_repeat_timer);
		if(group->identifier) {
			free(group->identifier);
		}
		free(group);
	}

	struct cg_input_device *it, *it_tmp;
	wl_list_for_each_safe(it, it_tmp, &seat->server->input->devices, link) {
		input_manager_handle_device_destroy(&it->device_destroy, NULL);
	}

	wlr_xcursor_manager_destroy(seat->xcursor_manager);
	if(seat->cursor) {
		wlr_cursor_destroy(seat->cursor);
	}
	wl_list_remove(&seat->cursor_motion.link);
	wl_list_remove(&seat->cursor_motion_absolute.link);
	wl_list_remove(&seat->cursor_button.link);
	wl_list_remove(&seat->cursor_axis.link);
	wl_list_remove(&seat->cursor_frame.link);
	wl_list_remove(&seat->touch_down.link);
	wl_list_remove(&seat->touch_up.link);
	wl_list_remove(&seat->touch_motion.link);
	wl_list_remove(&seat->request_set_cursor.link);
	wl_list_remove(&seat->request_set_selection.link);
	wl_list_remove(&seat->request_set_primary_selection.link);
	seat->server->seat = NULL;
	free(seat);
}

struct cg_seat *
seat_create(struct cg_server *server, struct wlr_backend *backend) {
	struct cg_seat *seat = calloc(1, sizeof(struct cg_seat));
	if(!seat) {
		wlr_log(WLR_ERROR, "Cannot allocate seat");
		return NULL;
	}

	seat->enable_cursor = true;
	seat->seat = wlr_seat_create(server->wl_display, "seat0");
	if(!seat->seat) {
		wlr_log(WLR_ERROR, "Cannot allocate seat0");
		free(seat);
		return NULL;
	}
	seat->server = server;
	seat->destroy.notify = handle_destroy;
	wl_signal_add(&seat->seat->events.destroy, &seat->destroy);

	seat->cursor = wlr_cursor_create();
	if(!seat->cursor) {
		wlr_log(WLR_ERROR, "Unable to create cursor");
		wl_list_remove(&seat->destroy.link);
		free(seat);
		return NULL;
	}
	wlr_cursor_attach_output_layout(seat->cursor, server->output_layout);

	if(!seat->xcursor_manager) {
		seat->xcursor_manager =
		    wlr_xcursor_manager_create(NULL, server->xcursor_size);
		if(!seat->xcursor_manager) {
			wlr_log(WLR_ERROR, "Cannot create XCursor manager");
			wlr_cursor_destroy(seat->cursor);
			wl_list_remove(&seat->destroy.link);
			free(seat);
			return NULL;
		}
	}

	seat->cursor_motion.notify = handle_cursor_motion;
	wl_signal_add(&seat->cursor->events.motion, &seat->cursor_motion);
	seat->cursor_motion_absolute.notify = handle_cursor_motion_absolute;
	wl_signal_add(&seat->cursor->events.motion_absolute,
	              &seat->cursor_motion_absolute);
	seat->cursor_button.notify = handle_cursor_button;
	wl_signal_add(&seat->cursor->events.button, &seat->cursor_button);
	seat->cursor_axis.notify = handle_cursor_axis;
	wl_signal_add(&seat->cursor->events.axis, &seat->cursor_axis);
	seat->cursor_frame.notify = handle_cursor_frame;
	wl_signal_add(&seat->cursor->events.frame, &seat->cursor_frame);

	seat->touch_down.notify = handle_touch_down;
	wl_signal_add(&seat->cursor->events.touch_down, &seat->touch_down);
	seat->touch_up.notify = handle_touch_up;
	wl_signal_add(&seat->cursor->events.touch_up, &seat->touch_up);
	seat->touch_motion.notify = handle_touch_motion;
	wl_signal_add(&seat->cursor->events.touch_motion, &seat->touch_motion);

	seat->request_set_cursor.notify = handle_request_set_cursor;
	wl_signal_add(&seat->seat->events.request_set_cursor,
	              &seat->request_set_cursor);
	seat->request_set_selection.notify = handle_request_set_selection;
	wl_signal_add(&seat->seat->events.request_set_selection,
	              &seat->request_set_selection);
	seat->request_set_primary_selection.notify =
	    handle_request_set_primary_selection;
	wl_signal_add(&seat->seat->events.request_set_primary_selection,
	              &seat->request_set_primary_selection);

	wl_list_init(&seat->keyboard_groups);
	seat->num_keyboards = 0;
	seat->num_pointers = 0;
	seat->num_touch = 0;

	wl_list_init(&seat->drag_icons);
	seat->request_start_drag.notify = handle_request_start_drag;
	wl_signal_add(&seat->seat->events.request_start_drag,
	              &seat->request_start_drag);
	seat->start_drag.notify = handle_start_drag;
	wl_signal_add(&seat->seat->events.start_drag, &seat->start_drag);

	seat->mode = 0;
	seat->default_mode = 0;

	return seat;
}

void
seat_destroy(struct cg_seat *seat) {
	if(!seat) {
		return;
	}

	wl_list_remove(&seat->request_start_drag.link);
	wl_list_remove(&seat->start_drag.link);

	// Destroying the wlr seat will trigger the destroy handler on our seat,
	// which will in turn free it.
	wlr_seat_destroy(seat->seat);
}

/* Important: this function returns NULL if we are focused on the background */
struct cg_view *
seat_get_focus(const struct cg_seat *seat) {
	return seat->focused_view;
}

void
seat_set_focus(struct cg_seat *seat, struct cg_view *view) {
	struct cg_server *server = seat->server;
	struct wlr_seat *wlr_seat = seat->seat;
	struct cg_view *prev_view = seat_get_focus(seat);

	/* Focusing the background */
	if(view == NULL) {
		workspace_tile_update_view(
		    server->curr_output->workspaces[server->curr_output->curr_workspace]
		        ->focused_tile,
		    NULL);
		seat->focused_view = NULL;
		if(prev_view != NULL) {
			view_activate(prev_view, false);
		}
		wlr_seat_keyboard_clear_focus(wlr_seat);
		process_cursor_motion(seat, -1);
		return;
	}

#if CG_HAS_XWAYLAND
	if(view->type != CG_XWAYLAND_VIEW || xwayland_view_should_manage(view))
#endif
	{
		struct cg_workspace *curr_workspace =
		    server->curr_output
		        ->workspaces[server->curr_output->curr_workspace];
		workspace_tile_update_view(curr_workspace->focused_tile, view);
		wl_list_remove(&curr_workspace->views);
		wl_list_insert(&view->link, &curr_workspace->views);
	}

#if CG_HAS_XWAYLAND
	if(view->type == CG_XWAYLAND_VIEW) {
		wlr_xwayland_set_seat(server->xwayland, seat->seat);
	}
	if(view->type == CG_XWAYLAND_VIEW && !xwayland_view_should_manage(view)) {
		const struct cg_xwayland_view *xwayland_view =
		    xwayland_view_from_view(view);
		if(!wlr_xwayland_or_surface_wants_focus(
		       xwayland_view->xwayland_surface)) {
			return;
		}
	} else
#endif
	{
		if(prev_view != NULL) {
			view_activate(prev_view, false);
		}

		seat->focused_view = view;
	}

	view_activate(view, true);

	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(wlr_seat);
	wlr_seat_keyboard_end_grab(wlr_seat);
	if(keyboard) {
		wlr_seat_keyboard_notify_enter(
		    wlr_seat, view->wlr_surface, keyboard->keycodes,
		    keyboard->num_keycodes, &keyboard->modifiers);
	} else {
		wlr_seat_keyboard_notify_enter(wlr_seat, view->wlr_surface, NULL, 0,
		                               NULL);
	}

	wlr_scene_node_raise_to_top(&view->scene_tree->node);
	process_cursor_motion(seat, -1);
	wlr_scene_node_set_position(
	    &view->workspace->output->bg->node,
	    output_get_layout_box(view->workspace->output).x,
	    output_get_layout_box(view->workspace->output).y);
}
