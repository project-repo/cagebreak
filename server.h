#ifndef CG_SERVER_H
#define CG_SERVER_H

#include "config.h"

#include <wayland-server-core.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>

struct cg_seat;
struct cg_output;
struct keybinding_list;
struct wlr_output_layout;
struct wlr_idle_inhibit_manager_v1;

struct cg_server {
	struct wl_display *wl_display;
	struct wl_event_loop *event_loop;

	struct cg_seat *seat;
	struct wlr_backend *backend;
	struct wlr_idle *idle;
	struct wlr_idle_inhibit_manager_v1 *idle_inhibit_v1;
	struct wl_listener new_idle_inhibitor_v1;
	struct wl_list inhibitors;

	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
	struct cg_output *curr_output;
	struct wl_listener new_output;

	struct wl_listener xdg_toplevel_decoration;
	struct wl_listener new_xdg_shell_surface;
#if CG_HAS_XWAYLAND
	struct wl_listener new_xwayland_surface;
#endif

	struct keybinding_list *keybindings;

	enum wl_output_transform output_transform;

	bool running;
	char **modes;
	uint16_t nws;
	uint16_t message_timeout;
	float *bg_color;
#ifdef DEBUG
	bool debug_damage_tracking;
#endif
};

void
display_terminate(struct cg_server *server);
int
get_mode_index_from_name(char *const *modes, const char *mode_name);

#endif
