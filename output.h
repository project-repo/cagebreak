#ifndef CG_OUTPUT_H
#define CG_OUTPUT_H

#include <wayland-server-core.h>
#include <wlr/util/box.h>

struct cg_server;
struct cg_view;
struct wlr_output;
struct wlr_surface;

struct cg_output {
	struct cg_server *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_rect *bg;

	struct wl_listener mode;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener frame;
	struct cg_workspace **workspaces;
	struct wl_list messages;
	int curr_workspace;
	int priority;
	struct cg_view *last_scanned_out_view;

	struct wl_list link; // cg_server::outputs
};

struct cg_output_priorities {
	char *ident;
	int priority;
	struct wl_list link;
};

enum output_status { OUTPUT_ENABLE, OUTPUT_DISABLE, OUTPUT_DEFAULT };

struct cg_output_config {
	enum output_status status;
	struct wlr_box pos;
	char *output_name;
	float refresh_rate;
	float scale;
	int priority;
	int angle;//enum wl_output_transform, -1 signifies "unspecified"
	struct wl_list link; // cg_server::output_config
};

typedef void (*cg_surface_iterator_func_t)(struct cg_output *output,
                                           struct wlr_surface *surface,
                                           struct wlr_box *box,
                                           void *user_data);

void
handle_new_output(struct wl_listener *listener, void *data);
void
output_configure(struct cg_server *server, struct cg_output *output);
void
output_set_window_title(struct cg_output *output, const char *title);
void
output_make_workspace_fullscreen(struct cg_output *output, int ws);
int
output_get_num(const struct cg_output* output);
#endif
