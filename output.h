#ifndef CG_OUTPUT_H
#define CG_OUTPUT_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_box.h>

struct cg_server;
struct cg_view;
struct wlr_output;
struct wlr_output_damage;
struct wlr_surface;

struct cg_output {
	struct cg_server *server;
	struct wlr_output *wlr_output;
	struct wlr_output_damage *damage;

	struct wl_listener mode;
	struct wl_listener transform;
	struct wl_listener destroy;
	struct wl_listener damage_frame;
	struct wl_listener damage_destroy;
	struct cg_workspace **workspaces;
	struct wl_list messages;
	int curr_workspace;

	struct wl_list link; // cg_server::outputs
};

struct cg_output_config {
	struct wlr_box pos;
	char* output_name;
	float refresh_rate;
	struct wl_list link;// cg_server::output_config
};

typedef void (*cg_surface_iterator_func_t)(struct cg_output *output,
                                           struct wlr_surface *surface,
                                           struct wlr_box *box,
                                           void *user_data);

void
handle_new_output(struct wl_listener *listener, void *data);
void
output_configure(struct cg_server* server, struct cg_output* output);
void
output_surface_for_each_surface(struct cg_output *output,
                                struct wlr_surface *surface, double ox,
                                double oy, cg_surface_iterator_func_t iterator,
                                void *user_data);
void
output_view_for_each_popup(struct cg_output *output, struct cg_view *view,
                           cg_surface_iterator_func_t iterator,
                           void *user_data);
void
output_drag_icons_for_each_surface(struct cg_output *output,
                                   struct wl_list *drag_icons,
                                   cg_surface_iterator_func_t iterator,
                                   void *user_data);
void
output_damage_surface(struct cg_output *output, struct wlr_surface *surface,
                      double ox, double oy, bool whole);
void
output_set_window_title(struct cg_output *output, const char *title);

#endif
