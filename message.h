// Copyright 2020 - 2024, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#ifndef MESSAGE_H

#define MESSAGE_H

#include <wayland-server-core.h>

struct cg_output;
struct wlr_box;
struct wlr_buffer;

enum cg_message_anchor {
	CG_MESSAGE_TOP_LEFT,
	CG_MESSAGE_TOP_CENTER,
	CG_MESSAGE_TOP_RIGHT,
	CG_MESSAGE_BOTTOM_LEFT,
	CG_MESSAGE_BOTTOM_CENTER,
	CG_MESSAGE_BOTTOM_RIGHT,
	CG_MESSAGE_CENTER,
	CG_MESSAGE_NOPT
};

struct cg_message_config {
	char *font;
	int display_time;
	float bg_color[4];
	float fg_color[4];
	int enabled;
	enum cg_message_anchor anchor;
};

struct cg_message {
	struct wlr_box *position;
	struct wlr_scene_buffer *message;
	struct wl_surface *surface;
	struct msg_buffer *buf;
	struct wl_list link;
};

void
message_printf(struct cg_output *output, const char *fmt, ...);
void
message_printf_pos(struct cg_output *output, struct wlr_box *position,
                   enum cg_message_anchor, const char *fmt, ...);
void
message_clear(struct cg_output *output);

#endif /* end of include guard MESSAGE_H */
