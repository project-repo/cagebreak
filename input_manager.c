// Copyright 2020 - 2023, project-repo and the cagebreak contributors
// SPDX -License-Identifier: MIT

#define _POSIX_C_SOURCE 200812L

#include "input_manager.h"
#include "config.h"
#include "input.h"
#include "seat.h"
#include "server.h"

#include <ctype.h>
#include <float.h>
#include <libevdev/libevdev.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_input_inhibitor.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/util/log.h>

void
strip_whitespace(char *str) {
	static const char whitespace[] = " \f\n\r\t\v";
	size_t len = strlen(str);
	size_t start = strspn(str, whitespace);
	memmove(str, &str[start], len + 1 - start);

	if(*str) {
		for(len -= start + 1; isspace(str[len]); --len) {
		}
		str[len + 1] = '\0';
	}
}

char *
input_device_get_identifier(struct wlr_input_device *device) {
	int vendor = device->vendor;
	int product = device->product;
	char *name = strdup(device->name ? device->name : "");
	strip_whitespace(name);

	char *p = name;
	for(; *p; ++p) {
		// There are in fact input devices with unprintable characters in its
		// name
		if(*p == ' ' || !isprint(*p)) {
			*p = '_';
		}
	}

	const char *fmt = "%d:%d:%s";
	int len = snprintf(NULL, 0, fmt, vendor, product, name) + 1;
	char *identifier = malloc(len);
	if(!identifier) {
		wlr_log(WLR_ERROR, "Unable to allocate unique input device name");
		free(name);
		return NULL;
	}

	snprintf(identifier, len, fmt, vendor, product, name);
	free(name);
	return identifier;
}

void
input_manager_handle_device_destroy(struct wl_listener *listener, void *data) {
	struct cg_input_device *input_device =
	    wl_container_of(listener, input_device, device_destroy);

	if(!input_device) {
		wlr_log(WLR_ERROR, "Could not find cagebreak input device to destroy");
		return;
	}

	seat_remove_device(input_device->server->seat, input_device);

	wl_list_remove(&input_device->link);
	wl_list_remove(&input_device->device_destroy.link);
	free(input_device->identifier);
	free(input_device);
}

struct cg_input_config *
input_manager_create_empty_input_config() {
	struct cg_input_config *cfg = calloc(1, sizeof(struct cg_input_config));
	if(cfg == NULL) {
		return NULL;
	}
	/* Libinput devices */
	cfg->tap = INT_MIN;
	cfg->tap_button_map = INT_MIN;
	cfg->drag = INT_MIN;
	cfg->drag_lock = INT_MIN;
	cfg->dwt = INT_MIN;
	cfg->send_events = INT_MIN;
	cfg->click_method = INT_MIN;
	cfg->middle_emulation = INT_MIN;
	cfg->natural_scroll = INT_MIN;
	cfg->accel_profile = INT_MIN;
	cfg->pointer_accel = FLT_MIN;
	cfg->scroll_factor = FLT_MIN;
	cfg->scroll_button = INT_MIN;
	cfg->scroll_method = INT_MIN;
	cfg->left_handed = INT_MIN;
	/*cfg->repeat_delay = INT_MIN;
	cfg->repeat_rate = INT_MIN;
	cfg->xkb_numlock = INT_MIN;
	cfg->xkb_capslock = INT_MIN;
	cfg->xkb_file_is_set = false;
	wl_list_init(&cfg->tools);*/

	/* Keyboards */
	cfg->enable_keybindings = -1;
	cfg->repeat_delay = -1;
	cfg->repeat_rate = -1;
	return cfg;
}

/* cfg1 has precedence */
struct cg_input_config *
input_manager_merge_input_configs(struct cg_input_config *cfg1,
                                  struct cg_input_config *cfg2) {
	struct cg_input_config *out_cfg = calloc(1, sizeof(struct cg_input_config));
	if(cfg1->identifier == NULL) {
		if(cfg2->identifier != NULL) {
			out_cfg->identifier = strdup(cfg2->identifier);
		}
	} else {
		out_cfg->identifier = strdup(cfg1->identifier);
	}
	if(out_cfg == NULL) {
		return NULL;
	}
	if(cfg1->tap == INT_MIN) {
		out_cfg->tap = cfg2->tap;
	} else {
		out_cfg->tap = cfg1->tap;
	}
	if(cfg1->send_events == INT_MIN) {
		out_cfg->send_events = cfg2->send_events;
	} else {
		out_cfg->send_events = cfg1->send_events;
	}
	if(cfg1->dwt == INT_MIN) {
		out_cfg->dwt = cfg2->dwt;
	} else {
		out_cfg->dwt = cfg1->dwt;
	}
	if(cfg1->drag_lock == INT_MIN) {
		out_cfg->drag_lock = cfg2->drag_lock;
	} else {
		out_cfg->drag_lock = cfg1->drag_lock;
	}
	if(cfg1->drag == INT_MIN) {
		out_cfg->drag = cfg2->drag;
	} else {
		out_cfg->drag = cfg1->drag;
	}
	if(cfg1->tap_button_map == INT_MIN) {
		out_cfg->tap_button_map = cfg2->tap_button_map;
	} else {
		out_cfg->tap_button_map = cfg1->tap_button_map;
	}
	if(cfg1->left_handed == INT_MIN) {
		out_cfg->left_handed = cfg2->left_handed;
	} else {
		out_cfg->left_handed = cfg1->left_handed;
	}
	if(cfg1->scroll_method == INT_MIN) {
		out_cfg->scroll_method = cfg2->scroll_method;
	} else {
		out_cfg->scroll_method = cfg1->scroll_method;
	}
	if(cfg1->scroll_button == INT_MIN) {
		out_cfg->scroll_button = cfg2->scroll_button;
	} else {
		out_cfg->scroll_button = cfg1->scroll_button;
	}
	if(cfg1->scroll_factor == FLT_MIN) {
		out_cfg->scroll_factor = cfg2->scroll_factor;
	} else {
		out_cfg->scroll_factor = cfg1->scroll_factor;
	}
	if(cfg1->pointer_accel == FLT_MIN) {
		out_cfg->pointer_accel = cfg2->pointer_accel;
	} else {
		out_cfg->pointer_accel = cfg1->pointer_accel;
	}
	if(cfg1->accel_profile == INT_MIN) {
		out_cfg->accel_profile = cfg2->accel_profile;
	} else {
		out_cfg->accel_profile = cfg1->accel_profile;
	}
	if(cfg1->natural_scroll == INT_MIN) {
		out_cfg->natural_scroll = cfg2->natural_scroll;
	} else {
		out_cfg->natural_scroll = cfg1->natural_scroll;
	}
	if(cfg1->middle_emulation == INT_MIN) {
		out_cfg->middle_emulation = cfg2->middle_emulation;
	} else {
		out_cfg->middle_emulation = cfg1->middle_emulation;
	}
	if(cfg1->click_method == INT_MIN) {
		out_cfg->click_method = cfg2->click_method;
	} else {
		out_cfg->click_method = cfg1->click_method;
	}
	if(cfg1->enable_keybindings == -1) {
		out_cfg->enable_keybindings = cfg2->enable_keybindings;
	} else {
		out_cfg->enable_keybindings = cfg1->enable_keybindings;
	}
	if(cfg1->repeat_delay == -1) {
		out_cfg->repeat_delay = cfg2->repeat_delay;
	} else {
		out_cfg->repeat_delay = cfg1->repeat_delay;
	}
	if(cfg1->repeat_rate == -1) {
		out_cfg->repeat_rate = cfg2->repeat_rate;
	} else {
		out_cfg->repeat_rate = cfg1->repeat_rate;
	}
	return out_cfg;
}

void
apply_keyboard_group_config(struct cg_input_config *config,
                            struct cg_keyboard_group *group) {
	if(config->enable_keybindings != -1) {
		group->enable_keybindings = config->enable_keybindings;
	}
	int repeat_delay = 600, repeat_rate = 25;
	if(config->repeat_delay != -1) {
		repeat_delay = config->repeat_delay;
	}
	if(config->repeat_rate != -1) {
		repeat_rate = config->repeat_rate;
	}
	wlr_keyboard_set_repeat_info(&group->wlr_group->keyboard, repeat_rate,
	                             repeat_delay);
}

void
cg_input_manager_configure_keyboard_group(struct cg_keyboard_group *group) {
	struct cg_server *server = group->seat->server;
	struct cg_input_config *tot_cfg = input_manager_create_empty_input_config();
	if(tot_cfg == NULL) {
		return;
	}

	struct cg_input_config *tmp_cfg, *config = NULL;
	wl_list_for_each(config, &server->input_config, link) {
		if(strcmp(config->identifier, group->identifier) == 0 ||
		   strcmp(config->identifier, "*") == 0 ||
		   (strncmp(config->identifier, "type:", 5) == 0 &&
		    strcmp(config->identifier + 5, "keyboard") == 0)) {
			tmp_cfg = tot_cfg;
			if(tot_cfg->identifier == NULL ||
			   strcmp(tot_cfg->identifier, "*") == 0 ||
			   strcmp(config->identifier, group->identifier)) {
				tot_cfg = input_manager_merge_input_configs(config, tot_cfg);
			} else {
				tot_cfg = input_manager_merge_input_configs(tot_cfg, config);
			}
			if(tmp_cfg->identifier != NULL) {
				free(tmp_cfg->identifier);
			}
			free(tmp_cfg);
		}
	}
	apply_keyboard_group_config(tot_cfg, group);
	if(tot_cfg->identifier != NULL) {
		free(tot_cfg->identifier);
	}
	free(tot_cfg);
}

void
cg_input_manager_configure(struct cg_server *server) {
	struct cg_input_device *device = NULL;
	wl_list_for_each(device, &server->input->devices, link) {
		cg_input_configure_libinput_device(device);
	}
	struct cg_keyboard_group *group = NULL;
	wl_list_for_each(group, &server->seat->keyboard_groups, link) {
		cg_input_manager_configure_keyboard_group(group);
	}
}

static void
new_input(struct cg_input_manager *input, struct wlr_input_device *device,
          bool virtual) {
	struct cg_input_device *input_device =
	    calloc(1, sizeof(struct cg_input_device));
	if(!input_device) {
		wlr_log(WLR_ERROR, "Could not allocate input device");
		return;
	}
	device->data = input_device;

	input_device->is_virtual = virtual;
	input_device->wlr_device = device;
	input_device->identifier = input_device_get_identifier(device);
	input_device->server = input->server;
	input_device->pointer = NULL;
	input_device->touch = NULL;

	wl_list_insert(&input->devices, &input_device->link);

	cg_input_configure_libinput_device(input_device);

	wl_signal_add(&device->events.destroy, &input_device->device_destroy);
	input_device->device_destroy.notify = input_manager_handle_device_destroy;

	struct cg_seat *seat = input->server->seat;
	seat_add_device(seat, input_device);
}

static void
handle_new_input(struct wl_listener *listener, void *data) {
	struct cg_input_manager *input =
	    wl_container_of(listener, input, new_input);
	struct wlr_input_device *device = data;

	new_input(input, device, false);
}

void
handle_virtual_keyboard(struct wl_listener *listener, void *data) {

	struct cg_input_manager *input =
	    wl_container_of(listener, input, virtual_keyboard_new);
	struct wlr_virtual_keyboard_v1 *keyboard = data;
	struct wlr_input_device *device = &keyboard->keyboard.base;

	new_input(input, device, true);
}

void
handle_virtual_pointer(struct wl_listener *listener, void *data) {
	struct cg_input_manager *input_manager =
	    wl_container_of(listener, input_manager, virtual_pointer_new);
	struct wlr_virtual_pointer_v1_new_pointer_event *event = data;
	struct wlr_virtual_pointer_v1 *pointer = event->new_pointer;
	struct wlr_input_device *device = &pointer->pointer.base;

	new_input(input_manager, device, true);

	if(event->suggested_output) {
		wlr_cursor_map_input_to_output(input_manager->server->seat->cursor,
		                               device, event->suggested_output);
	}
}

struct cg_input_manager *
input_manager_create(struct cg_server *server) {
	struct cg_input_manager *input = calloc(1, sizeof(struct cg_input_manager));
	if(!input) {
		return NULL;
	}

	wl_list_init(&input->devices);
	input->server = server;

	input->new_input.notify = handle_new_input;
	wl_signal_add(&server->backend->events.new_input, &input->new_input);

	input->virtual_keyboard =
	    wlr_virtual_keyboard_manager_v1_create(server->wl_display);
	wl_signal_add(&input->virtual_keyboard->events.new_virtual_keyboard,
	              &input->virtual_keyboard_new);
	input->virtual_keyboard_new.notify = handle_virtual_keyboard;

	input->virtual_pointer =
	    wlr_virtual_pointer_manager_v1_create(server->wl_display);
	wl_signal_add(&input->virtual_pointer->events.new_virtual_pointer,
	              &input->virtual_pointer_new);
	input->virtual_pointer_new.notify = handle_virtual_pointer;

	return input;
}

uint32_t
get_mouse_bindsym(const char *name, char **error) {
	// Get event code from name
	int code = libevdev_event_code_from_name(EV_KEY, name);
	if(code == -1) {
		size_t len = snprintf(NULL, 0, "Unknown event %s", name) + 1;
		*error = malloc(len);
		if(*error) {
			snprintf(*error, len, "Unknown event %s", name);
		}
		return 0;
	}
	return code;
}

uint32_t
get_mouse_bindcode(const char *name, char **error) {
	// Validate event code
	errno = 0;
	char *endptr;
	int code = strtol(name, &endptr, 10);
	if(endptr == name && code <= 0) {
		*error = strdup("Button event code must be a positive integer.");
		return 0;
	} else if(errno == ERANGE) {
		*error = strdup("Button event code out of range.");
		return 0;
	}
	const char *event = libevdev_event_code_get_name(EV_KEY, code);
	if(!event || strncmp(event, "BTN_", strlen("BTN_")) != 0) {
		size_t len = snprintf(NULL, 0, "Event code %d (%s) is not a button",
		                      code, event ? event : "(null)") +
		             1;
		*error = malloc(len);
		if(*error) {
			snprintf(*error, len, "Event code %d (%s) is not a button", code,
			         event ? event : "(null)");
		}
		return 0;
	}
	return code;
}

uint32_t
input_manager_get_mouse_button(const char *name, char **error) {
	uint32_t button = get_mouse_bindsym(name, error);
	if(!button && !*error) {
		button = get_mouse_bindcode(name, error);
	}
	return button;
}
