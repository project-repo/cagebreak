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

static bool device_is_touchpad(struct cg_input_device *device) {
	if (device->wlr_device->type != WLR_INPUT_DEVICE_POINTER ||
			!wlr_input_device_is_libinput(device->wlr_device)) {
		return false;
	}

	struct libinput_device *libinput_device =
		wlr_libinput_get_device_handle(device->wlr_device);

	return libinput_device_config_tap_get_finger_count(libinput_device) > 0;
}

const char *input_device_get_type(struct cg_input_device *device) {
	switch (device->wlr_device->type) {
	case WLR_INPUT_DEVICE_POINTER:
		if (device_is_touchpad(device)) {
			return "touchpad";
		} else {
			return "pointer";
		}
	case WLR_INPUT_DEVICE_KEYBOARD:
		return "keyboard";
	case WLR_INPUT_DEVICE_TOUCH:
		return "touch";
	case WLR_INPUT_DEVICE_TABLET_TOOL:
		return "tablet_tool";
	case WLR_INPUT_DEVICE_TABLET_PAD:
		return "tablet_pad";
	case WLR_INPUT_DEVICE_SWITCH:
		return "switch";
	}
	return "unknown";
}

struct cg_input_config *input_device_get_config(struct cg_input_device *device) {
	struct cg_server *server=device->server;
	struct cg_input_config *wildcard_config = NULL;
	struct cg_input_config *config = NULL;
	wl_list_for_each(config, &server->input_config, link) {
		if (strcmp(config->identifier, device->identifier) == 0) {
			return config;
		} else if (strcmp(config->identifier, "*") == 0) {
			wildcard_config = config;
		}
	}

	const char *device_type = input_device_get_type(device);
	wl_list_for_each(config, &server->input_config, link) {
		if (strncmp(config->identifier,"type:",5)&&strcmp(config->identifier + 5, device_type) == 0) {
			return config;
		}
	}

	return wildcard_config;
}
