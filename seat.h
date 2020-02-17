#ifndef CG_SEAT_H
#define CG_SEAT_H

#include <wayland-server-core.h>

struct cg_server;
struct cg_view;
struct wlr_cursor;
struct wlr_input_device;
struct wlr_seat;
struct wlr_xcursor_manager;
struct wlr_backend;

#define DEFAULT_XCURSOR "left_ptr"
#define XCURSOR_SIZE 24

struct cg_keyboard_group {
	struct cg_keyboard *keyboard;

	struct wlr_keyboard_group *wlr_group;
	struct cg_seat *seat;
	struct wl_listener key;
	struct wl_listener modifiers;
	struct wl_list link;
};

struct cg_seat {
	struct wlr_seat *seat;
	struct cg_server *server;
	struct wl_listener destroy;

	struct wl_list keyboards;
	struct wl_list keyboard_groups;
	struct wl_list pointers;
	struct wl_list touch;
	struct wl_listener new_input;

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *xcursor_manager;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	int32_t touch_id;
	double touch_lx;
	double touch_ly;
	struct wl_listener touch_down;
	struct wl_listener touch_up;
	struct wl_listener touch_motion;

	struct wl_list drag_icons;
	struct wl_listener request_start_drag;
	struct wl_listener start_drag;

	struct wl_listener request_set_cursor;
	struct wl_listener request_set_selection;
	struct wl_listener request_set_primary_selection;

	uint16_t mode;
	uint16_t default_mode;

	struct wl_shm *shm; // Shared memory

	struct cg_view *focused_view;
};

struct cg_keyboard {
	struct wl_list link; // seat::keyboards
	struct cg_seat *seat;
	struct wlr_input_device *device;

	struct wl_listener destroy;
	struct keybinding **repeat_keybinding;
	struct wl_event_source *key_repeat_timer;
};

struct cg_pointer {
	struct wl_list link; // seat::pointers
	struct cg_seat *seat;
	struct wlr_input_device *device;

	struct wl_listener destroy;
};

struct cg_touch {
	struct wl_list link; // seat::touch
	struct cg_seat *seat;
	struct wlr_input_device *device;

	struct wl_listener destroy;
};

struct cg_drag_icon {
	struct wl_list link; // seat::drag_icons
	struct cg_seat *seat;
	struct wlr_drag_icon *wlr_drag_icon;

	/* The drag icon has a position in layout coordinates. */
	double lx, ly;

	struct wl_listener destroy;
};

struct cg_seat *
seat_create(struct cg_server *server, struct wlr_backend *backend);
void
seat_destroy(struct cg_seat *seat);
struct cg_view *
seat_get_focus(const struct cg_seat *seat);
void
seat_set_focus(struct cg_seat *seat, struct cg_view *view);

#endif
