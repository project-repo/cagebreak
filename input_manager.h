#ifndef CG_INPUT_MANAGER_H
#define CG_INPUT_MANAGER_H

#include "seat.h"
#include "server.h"
#include <wayland-server-core.h>
#include <wlr/util/box.h>

struct cg_input_manager *
input_manager_create(struct cg_server *server);
void
input_manager_handle_device_destroy(struct wl_listener *listener, void *data);
uint32_t
input_manager_get_mouse_button(const char *name, char **error);
struct cg_input_config*
input_manager_create_empty_input_config();
struct cg_input_config*
input_manager_merge_input_configs(struct cg_input_config* cfg1, struct cg_input_config* cfg2);
void
cg_input_manager_configure(struct cg_server *server);
void
cg_input_manager_configure_keyboard_group(struct cg_keyboard_group *group);


struct cg_input_manager {
	struct wl_list devices;
	struct cg_server *server;

	struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard;
	struct wlr_virtual_pointer_manager_v1 *virtual_pointer;

	struct wl_listener new_input;
	struct wl_listener virtual_keyboard_new;
	struct wl_listener virtual_pointer_new;
};

struct cg_input_config_mapped_from_region {
	double x1, y1;
	double x2, y2;
	bool mm;
};

struct calibration_matrix {
	bool configured;
	float matrix[6];
};

enum cg_input_config_mapped_to {
	MAPPED_TO_DEFAULT,
	MAPPED_TO_OUTPUT,
	MAPPED_TO_REGION,
};

/**
 * options for input devices
 */
struct cg_input_config {
	char *identifier;

	/* Libinput devices */
	int accel_profile;
	struct calibration_matrix calibration_matrix;
	int click_method;
	int drag;
	int drag_lock;
	int dwt;
	int left_handed;
	int middle_emulation;
	int natural_scroll;
	float pointer_accel;
	float scroll_factor;
	/*int repeat_delay;
	int repeat_rate;*/
	int scroll_button;
	int scroll_method;
	int send_events;
	int tap;
	int tap_button_map;

	/*char *xkb_layout;
	char *xkb_model;
	char *xkb_options;
	char *xkb_rules;
	char *xkb_variant;
	char *xkb_file;

	bool xkb_file_is_set;

	int xkb_numlock;
	int xkb_capslock;*/

	struct wl_list link;
	struct cg_input_config_mapped_from_region *mapped_from_region;

	enum cg_input_config_mapped_to mapped_to;
	char *mapped_to_output;
	/*struct wlr_box *mapped_to_region;*/

	/*struct wl_list tools;*/

	bool capturable;
	struct wlr_box region;

	/* Keyboards */
	int enable_keybindings;
	int repeat_delay;
	int repeat_rate;
};

struct cg_input_device {
	char *identifier;
	struct cg_server *server;
	struct wlr_input_device *wlr_device;
	struct wl_list link;
	struct wl_listener device_destroy;
	bool is_virtual;

	/* Only one of the following is non-NULL depending on the type of the input
	 * device */
	struct cg_pointer *pointer;
	struct cg_touch *touch;
};

#endif
