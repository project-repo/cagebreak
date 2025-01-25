// Copyright 2020 - 2025, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#include "input.h"
#include "input_manager.h"
#include "output.h"
#include <float.h>
#include <libinput.h>
#include <libudev.h>
#include <limits.h>
#include <strings.h>
#include <wlr/backend/libinput.h>
#include <wlr/util/log.h>

static void
log_status(enum libinput_config_status status) {
	if(status != LIBINPUT_CONFIG_STATUS_SUCCESS) {
		wlr_log(WLR_ERROR, "Failed to apply libinput config: %s",
		        libinput_config_status_to_str(status));
	}
}

static bool
set_send_events(struct libinput_device *device, uint32_t mode) {
	if(libinput_device_config_send_events_get_mode(device) == mode) {
		return false;
	}
	wlr_log(WLR_DEBUG, "send_events_set_mode(%" PRIu32 ")", mode);
	log_status(libinput_device_config_send_events_set_mode(device, mode));
	return true;
}

static bool
set_tap(struct libinput_device *device, enum libinput_config_tap_state tap) {
	if(libinput_device_config_tap_get_finger_count(device) <= 0 ||
	   libinput_device_config_tap_get_enabled(device) == tap) {
		return false;
	}
	wlr_log(WLR_DEBUG, "tap_set_enabled(%d)", tap);
	log_status(libinput_device_config_tap_set_enabled(device, tap));
	return true;
}

static bool
set_tap_button_map(struct libinput_device *device,
                   enum libinput_config_tap_button_map map) {
	if(libinput_device_config_tap_get_finger_count(device) <= 0 ||
	   libinput_device_config_tap_get_button_map(device) == map) {
		return false;
	}
	wlr_log(WLR_DEBUG, "tap_set_button_map(%d)", map);
	log_status(libinput_device_config_tap_set_button_map(device, map));
	return true;
}

static bool
set_tap_drag(struct libinput_device *device,
             enum libinput_config_drag_state drag) {
	if(libinput_device_config_tap_get_finger_count(device) <= 0 ||
	   libinput_device_config_tap_get_drag_enabled(device) == drag) {
		return false;
	}
	wlr_log(WLR_DEBUG, "tap_set_drag_enabled(%d)", drag);
	log_status(libinput_device_config_tap_set_drag_enabled(device, drag));
	return true;
}

static bool
set_tap_drag_lock(struct libinput_device *device,
                  enum libinput_config_drag_lock_state lock) {
	if(libinput_device_config_tap_get_finger_count(device) <= 0 ||
	   libinput_device_config_tap_get_drag_lock_enabled(device) == lock) {
		return false;
	}
	wlr_log(WLR_DEBUG, "tap_set_drag_lock_enabled(%d)", lock);
	log_status(libinput_device_config_tap_set_drag_lock_enabled(device, lock));
	return true;
}

static bool
set_accel_speed(struct libinput_device *device, double speed) {
	if(!libinput_device_config_accel_is_available(device) ||
	   libinput_device_config_accel_get_speed(device) == speed) {
		return false;
	}
	wlr_log(WLR_DEBUG, "accel_set_speed(%f)", speed);
	log_status(libinput_device_config_accel_set_speed(device, speed));
	return true;
}

static bool
set_accel_profile(struct libinput_device *device,
                  enum libinput_config_accel_profile profile) {
	if(!libinput_device_config_accel_is_available(device) ||
	   libinput_device_config_accel_get_profile(device) == profile) {
		return false;
	}
	wlr_log(WLR_DEBUG, "accel_set_profile(%d)", profile);
	log_status(libinput_device_config_accel_set_profile(device, profile));
	return true;
}

static bool
set_natural_scroll(struct libinput_device *d, bool n) {
	if(!libinput_device_config_scroll_has_natural_scroll(d) ||
	   libinput_device_config_scroll_get_natural_scroll_enabled(d) == n) {
		return false;
	}
	wlr_log(WLR_DEBUG, "scroll_set_natural_scroll(%d)", n);
	log_status(libinput_device_config_scroll_set_natural_scroll_enabled(d, n));
	return true;
}

static bool
set_left_handed(struct libinput_device *device, bool left) {
	if(!libinput_device_config_left_handed_is_available(device) ||
	   libinput_device_config_left_handed_get(device) == left) {
		return false;
	}
	wlr_log(WLR_DEBUG, "left_handed_set(%d)", left);
	log_status(libinput_device_config_left_handed_set(device, left));
	return true;
}

static bool
set_click_method(struct libinput_device *device,
                 enum libinput_config_click_method method) {
	uint32_t click = libinput_device_config_click_get_methods(device);
	if((click & ~LIBINPUT_CONFIG_CLICK_METHOD_NONE) == 0 ||
	   libinput_device_config_click_get_method(device) == method) {
		return false;
	}
	wlr_log(WLR_DEBUG, "click_set_method(%d)", method);
	log_status(libinput_device_config_click_set_method(device, method));
	return true;
}

static bool
set_middle_emulation(struct libinput_device *dev,
                     enum libinput_config_middle_emulation_state mid) {
	if(!libinput_device_config_middle_emulation_is_available(dev) ||
	   libinput_device_config_middle_emulation_get_enabled(dev) == mid) {
		return false;
	}
	wlr_log(WLR_DEBUG, "middle_emulation_set_enabled(%d)", mid);
	log_status(libinput_device_config_middle_emulation_set_enabled(dev, mid));
	return true;
}

static bool
set_scroll_method(struct libinput_device *device,
                  enum libinput_config_scroll_method method) {
	uint32_t scroll = libinput_device_config_scroll_get_methods(device);
	if((scroll & ~LIBINPUT_CONFIG_SCROLL_NO_SCROLL) == 0 ||
	   libinput_device_config_scroll_get_method(device) == method) {
		return false;
	}
	wlr_log(WLR_DEBUG, "scroll_set_method(%d)", method);
	log_status(libinput_device_config_scroll_set_method(device, method));
	return true;
}

static bool
set_scroll_button(struct libinput_device *dev, uint32_t button) {
	uint32_t scroll = libinput_device_config_scroll_get_methods(dev);
	if((scroll & ~LIBINPUT_CONFIG_SCROLL_NO_SCROLL) == 0 ||
	   libinput_device_config_scroll_get_button(dev) == button) {
		return false;
	}
	wlr_log(WLR_DEBUG, "scroll_set_button(%" PRIu32 ")", button);
	log_status(libinput_device_config_scroll_set_button(dev, button));
	return true;
}

static bool
set_dwt(struct libinput_device *device, bool dwt) {
	if(!libinput_device_config_dwt_is_available(device) ||
	   libinput_device_config_dwt_get_enabled(device) == dwt) {
		return false;
	}
	wlr_log(WLR_DEBUG, "dwt_set_enabled(%d)", dwt);
	log_status(libinput_device_config_dwt_set_enabled(device, dwt));
	return true;
}

static bool
set_calibration_matrix(struct libinput_device *dev, float mat[6]) {
	if(!libinput_device_config_calibration_has_matrix(dev)) {
		return false;
	}
	bool changed = false;
	float current[6];
	libinput_device_config_calibration_get_matrix(dev, current);
	for(int i = 0; i < 6; i++) {
		if(current[i] != mat[i]) {
			changed = true;
			break;
		}
	}
	if(changed) {
		wlr_log(WLR_DEBUG, "calibration_set_matrix(%f, %f, %f, %f, %f, %f)",
		        mat[0], mat[1], mat[2], mat[3], mat[4], mat[5]);
		log_status(libinput_device_config_calibration_set_matrix(dev, mat));
	}
	return changed;
}

void
output_get_identifier(char *identifier, size_t len, struct cg_output *output) {
	struct wlr_output *wlr_output = output->wlr_output;
	snprintf(identifier, len, "%s %s %s", wlr_output->make, wlr_output->model,
	         wlr_output->serial);
}

struct cg_output *
output_by_name_or_id(const char *name_or_id, struct cg_server *server) {
	struct cg_output *output = NULL;
	wl_list_for_each(output, &server->outputs, link) {
		char identifier[128];
		output_get_identifier(identifier, sizeof(identifier), output);
		if(strcasecmp(identifier, name_or_id) == 0 ||
		   strcasecmp(output->name, name_or_id) == 0) {
			return output;
		}
	}
	return NULL;
}

static bool
device_is_touchpad(struct cg_input_device *device) {
	if(device->wlr_device->type != WLR_INPUT_DEVICE_POINTER ||
	   !wlr_input_device_is_libinput(device->wlr_device)) {
		return false;
	}

	struct libinput_device *libinput_device =
	    wlr_libinput_get_device_handle(device->wlr_device);

	return libinput_device_config_tap_get_finger_count(libinput_device) > 0;
}

const char *
input_device_get_type(struct cg_input_device *device) {
	switch(device->wlr_device->type) {
	case WLR_INPUT_DEVICE_POINTER:
		if(device_is_touchpad(device)) {
			return "touchpad";
		} else {
			return "pointer";
		}
	case WLR_INPUT_DEVICE_KEYBOARD:
		return "keyboard";
	case WLR_INPUT_DEVICE_TOUCH:
		return "touch";
	case WLR_INPUT_DEVICE_TABLET:
		return "tablet_tool";
	case WLR_INPUT_DEVICE_TABLET_PAD:
		return "tablet_pad";
	case WLR_INPUT_DEVICE_SWITCH:
		return "switch";
	}
	return "unknown";
}

void
apply_config_to_device(struct cg_input_config *config,
                       struct cg_input_device *input_device) {

	if(wlr_input_device_is_libinput(input_device->wlr_device)) {
		struct libinput_device *device =
		    wlr_libinput_get_device_handle(input_device->wlr_device);
		if(config->mapped_to_output &&
		   !output_by_name_or_id(config->mapped_to_output,
		                         input_device->server)) {
			wlr_log(WLR_DEBUG,
			        "'%s' is mapped to offline output '%s'; disabling input",
			        config->identifier, config->mapped_to_output);
			set_send_events(device, LIBINPUT_CONFIG_SEND_EVENTS_DISABLED);
		} else if(config->send_events != INT_MIN) {
			set_send_events(device, config->send_events);
		} else {
			// Have to reset to the default mode here, otherwise if
			// ic->send_events is unset and a mapped output just came online
			// after being disabled, we'd remain stuck sending no events.
			set_send_events(
			    device,
			    libinput_device_config_send_events_get_default_mode(device));
		}

		if(config->tap != INT_MIN) {
			set_tap(device, config->tap);
		}
		if(config->tap_button_map != INT_MIN) {
			set_tap_button_map(device, config->tap_button_map);
		}
		if(config->drag != INT_MIN) {
			set_tap_drag(device, config->drag);
		}
		if(config->drag_lock != INT_MIN) {
			set_tap_drag_lock(device, config->drag_lock);
		}
		if(config->pointer_accel != FLT_MIN) {
			set_accel_speed(device, config->pointer_accel);
		}
		if(config->accel_profile != INT_MIN) {
			set_accel_profile(device, config->accel_profile);
		}
		if(config->natural_scroll != INT_MIN) {
			set_natural_scroll(device, config->natural_scroll);
		}
		if(config->left_handed != INT_MIN) {
			set_left_handed(device, config->left_handed);
		}
		if(config->click_method != INT_MIN) {
			set_click_method(device, config->click_method);
		}
		if(config->middle_emulation != INT_MIN) {
			set_middle_emulation(device, config->middle_emulation);
		}
		if(config->scroll_method != INT_MIN) {
			set_scroll_method(device, config->scroll_method);
		}
		if(config->scroll_button != INT_MIN) {
			set_scroll_button(device, config->scroll_button);
		}
		if(config->dwt != INT_MIN) {
			set_dwt(device, config->dwt);
		}
		if(config->calibration_matrix.configured) {
			set_calibration_matrix(device, config->calibration_matrix.matrix);
		}
	}
}

void
cg_input_configure_libinput_device(struct cg_input_device *input_device) {
	struct cg_server *server = input_device->server;
	struct cg_input_config *config = NULL;

	wl_list_for_each(config, &server->input_config, link) {
		const char *device_type = input_device_get_type(input_device);
		if(strcmp(config->identifier, input_device->identifier) == 0 ||
		   strcmp(config->identifier, "*") == 0 ||
		   (strncmp(config->identifier, "type:", 5) == 0 &&
		    strcmp(config->identifier + 5, device_type) == 0)) {
			apply_config_to_device(config, input_device);
		}
	}
}

bool
cg_libinput_device_is_builtin(struct cg_input_device *cg_device) {
	if(!wlr_input_device_is_libinput(cg_device->wlr_device)) {
		return false;
	}

	struct libinput_device *device =
	    wlr_libinput_get_device_handle(cg_device->wlr_device);
	struct udev_device *udev_device = libinput_device_get_udev_device(device);
	if(!udev_device) {
		return false;
	}

	const char *id_path =
	    udev_device_get_property_value(udev_device, "ID_PATH");
	if(!id_path) {
		return false;
	}

	const char prefix[] = "platform-";
	return strncmp(id_path, prefix, strlen(prefix)) == 0;
}
