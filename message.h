#ifndef MESSAGE_H

#define MESSAGE_H

#include <wayland-server-core.h>

struct cg_output;
struct wlr_box;
struct wlr_texture;

enum cg_message_align {
	CG_MESSAGE_TOP_LEFT,
	CG_MESSAGE_TOP_RIGHT,
	CG_MESSAGE_BOTTOM_LEFT,
	CG_MESSAGE_BOTTOM_RIGHT,
	CG_MESSAGE_CENTER,
};

struct cg_message_config {
	char *font;
	int display_time;
	float bg_color[4];
	float fg_color[4];
};

struct cg_message {
	struct wlr_box *position;
	struct wlr_texture *message;
	struct wl_list link;
};

void
message_printf(struct cg_output *output, const char *fmt, ...);
void
message_printf_pos(struct cg_output *output, struct wlr_box *position,
                   enum cg_message_align, const char *fmt, ...);
void
message_clear(struct cg_output *output);

#endif /* end of include guard MESSAGE_H */
