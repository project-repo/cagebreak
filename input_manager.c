/*
 * Cagebreak: A Wayland tiling compositor.
 *
 * Copyright (C) 2020 The Cagebreak Authors
 * Copyright (C) 2018-2020 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200812L

#include "config.h"
#include "server.h"
#include "seat.h"
#include "input_manager.h"
#include "input.h"

#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_input_inhibitor.h>
#include <wayland-server-core.h>
#include <libevdev/libevdev.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

void strip_whitespace(char *str) {
	static const char whitespace[] = " \f\n\r\t\v";
	size_t len = strlen(str);
	size_t start = strspn(str, whitespace);
	memmove(str, &str[start], len + 1 - start);

	if (*str) {
		for (len -= start + 1; isspace(str[len]); --len) {}
		str[len + 1] = '\0';
	}
}

char *input_device_get_identifier(struct wlr_input_device *device) {
	int vendor = device->vendor;
	int product = device->product;
	char *name = strdup(device->name ? device->name : "");
	strip_whitespace(name);

	char *p = name;
	for (; *p; ++p) {
		// There are in fact input devices with unprintable characters in its name
		if (*p == ' ' || !isprint(*p)) {
			*p = '_';
		}
	}

	const char *fmt = "%d:%d:%s";
	int len = snprintf(NULL, 0, fmt, vendor, product, name) + 1;
	char *identifier = malloc(len);
	if (!identifier) {
		wlr_log(WLR_ERROR, "Unable to allocate unique input device name");
		return NULL;
	}

	snprintf(identifier, len, fmt, vendor, product, name);
	free(name);
	return identifier;
}

void input_manager_handle_device_destroy(struct wl_listener *listener, void *data) {
	struct cg_input_device *input_device =
		wl_container_of(listener, input_device, device_destroy);

	if (!input_device) {
		wlr_log(WLR_ERROR, "Could not find cagebreak input device to destroy");
		return;
	}

	seat_remove_device(input_device->server->seat, input_device);

	wl_list_remove(&input_device->link);
	wl_list_remove(&input_device->device_destroy.link);
	free(input_device->identifier);
	free(input_device);
}

static void handle_new_input(struct wl_listener *listener, void *data) {
	struct cg_input_manager *input =
		wl_container_of(listener, input, new_input);
	struct cg_server *server= input->server;
	struct wlr_input_device *device = data;

	struct cg_input_device *input_device =
		calloc(1, sizeof(struct cg_input_device));
	if (!input_device) {
		wlr_log(WLR_ERROR,"Could not allocate input device");
		return;
	}
	device->data = input_device;

	input_device->wlr_device = device;
	input_device->identifier = input_device_get_identifier(device);
	input_device->server=server;
	input_device->pointer=NULL;
	input_device->touch=NULL;
	
	wl_list_insert(&input->devices, &input_device->link);

	cg_input_configure_libinput_device(input_device);

	wl_signal_add(&device->events.destroy, &input_device->device_destroy);
	input_device->device_destroy.notify = input_manager_handle_device_destroy;

	struct cg_seat *seat = server->seat;
	seat_add_device(seat, input_device);
}

void handle_virtual_keyboard(struct wl_listener *listener, void *data) {
	struct cg_input_manager *input_manager =
		wl_container_of(listener, input_manager, virtual_keyboard_new);
	struct wlr_virtual_keyboard_v1 *keyboard = data;
	struct wlr_input_device *device = &keyboard->input_device;

	struct cg_seat *seat = input_manager->server->seat;

	struct cg_input_device *input_device =
		calloc(1, sizeof(struct cg_input_device));
	if (!input_device) {
		wlr_log(WLR_ERROR, "could not allocate input device");
		return;
	}
	device->data = input_device;

	input_device->is_virtual = true;
	input_device->wlr_device = device;
	input_device->identifier = input_device_get_identifier(device);
	wl_list_insert(&input_manager->devices, &input_device->link);
	input_device->server=input_manager->server;
	input_device->pointer=NULL;
	input_device->touch=NULL;

	wl_signal_add(&device->events.destroy, &input_device->device_destroy);
	input_device->device_destroy.notify = input_manager_handle_device_destroy;

	seat_add_device(seat, input_device);
}

void handle_virtual_pointer(struct wl_listener *listener, void *data) {
	struct cg_input_manager *input_manager =
		wl_container_of(listener, input_manager, virtual_pointer_new);
	struct wlr_virtual_pointer_v1_new_pointer_event *event = data;
	struct wlr_virtual_pointer_v1 *pointer = event->new_pointer;
	struct wlr_input_device *device = &pointer->input_device;

	struct cg_seat *seat = input_manager->server->seat;

	struct cg_input_device *input_device =
		calloc(1, sizeof(struct cg_input_device));
	if (!input_device) {
		wlr_log(WLR_ERROR, "could not allocate input device");
		return;
	}
	device->data = input_device;

	input_device->is_virtual = true;
	input_device->wlr_device = device;
	input_device->identifier = input_device_get_identifier(device);
	wl_list_insert(&input_manager->devices, &input_device->link);
	input_device->server=input_manager->server;
	input_device->pointer=NULL;
	input_device->touch=NULL;

	wl_signal_add(&device->events.destroy, &input_device->device_destroy);
	input_device->device_destroy.notify = input_manager_handle_device_destroy;

	seat_add_device(seat, input_device);

	if (event->suggested_output) {
		wlr_cursor_map_input_to_output(seat->cursor, device,
			event->suggested_output);
	}
}

struct cg_input_manager *input_manager_create(struct cg_server *server) {
	struct cg_input_manager *input =
		calloc(1, sizeof(struct cg_input_manager));
	if (!input) {
		return NULL;
	}

	wl_list_init(&input->devices);
	input->server=server;

	input->new_input.notify = handle_new_input;
	wl_signal_add(&server->backend->events.new_input, &input->new_input);

	input->virtual_keyboard = wlr_virtual_keyboard_manager_v1_create(
		server->wl_display);
	wl_signal_add(&input->virtual_keyboard->events.new_virtual_keyboard,
		&input->virtual_keyboard_new);
	input->virtual_keyboard_new.notify = handle_virtual_keyboard;

	input->virtual_pointer = wlr_virtual_pointer_manager_v1_create(
		server->wl_display
	);
	wl_signal_add(&input->virtual_pointer->events.new_virtual_pointer,
		&input->virtual_pointer_new);
	input->virtual_pointer_new.notify = handle_virtual_pointer;

	return input;
}

uint32_t get_mouse_bindsym(const char *name, char **error) {
	// Get event code from name
	int code = libevdev_event_code_from_name(EV_KEY, name);
	if (code == -1) {
		size_t len = snprintf(NULL, 0, "Unknown event %s", name) + 1;
		*error = malloc(len);
		if (*error) {
			snprintf(*error, len, "Unknown event %s", name);
		}
		return 0;
	}
	return code;
}

uint32_t get_mouse_bindcode(const char *name, char **error) {
	// Validate event code
	errno = 0;
	char *endptr;
	int code = strtol(name, &endptr, 10);
	if (endptr == name && code <= 0) {
		*error = strdup("Button event code must be a positive integer.");
		return 0;
	} else if (errno == ERANGE) {
		*error = strdup("Button event code out of range.");
		return 0;
	}
	const char *event = libevdev_event_code_get_name(EV_KEY, code);
	if (!event || strncmp(event, "BTN_", strlen("BTN_")) != 0) {
		size_t len = snprintf(NULL, 0, "Event code %d (%s) is not a button",
				code, event ? event : "(null)") + 1;
		*error = malloc(len);
		if (*error) {
			snprintf(*error, len, "Event code %d (%s) is not a button",
					code, event ? event : "(null)");
		}
		return 0;
	}
	return code;
}

uint32_t input_manager_get_mouse_button(const char *name, char **error) {
	uint32_t button = get_mouse_bindsym(name, error);
	if (!button && !*error) {
		button = get_mouse_bindcode(name, error);
	}
	return button;
}
