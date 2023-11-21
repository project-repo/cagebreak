// Copyright 2020 - 2023, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#include <cairo/cairo.h>
#include <drm_fourcc.h>
#include <pango/pangocairo.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wlr/backend.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "message.h"
#include "output.h"
#include "pango.h"
#include "server.h"
#include "util.h"

struct msg_buffer {
	struct wlr_buffer base;
	void *data;
	uint32_t format;
	size_t stride;
};

static void
msg_buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct msg_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	free(buffer->data);
	free(buffer);
}

static bool
msg_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer, uint32_t flags,
                                 void **data, uint32_t *format,
                                 size_t *stride) {
	struct msg_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	if(data != NULL) {
		*data = (void *)buffer->data;
	}
	if(format != NULL) {
		*format = buffer->format;
	}
	if(stride != NULL) {
		*stride = buffer->stride;
	}
	return true;
}

static void
msg_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
	// This space is intentionally left blank
}

static const struct wlr_buffer_impl msg_buffer_impl = {
    .destroy = msg_buffer_destroy,
    .begin_data_ptr_access = msg_buffer_begin_data_ptr_access,
    .end_data_ptr_access = msg_buffer_end_data_ptr_access,
};

static struct msg_buffer *
msg_buffer_create(uint32_t width, uint32_t height, uint32_t stride) {
	struct msg_buffer *buffer = calloc(1, sizeof(*buffer));
	if(buffer == NULL) {
		return NULL;
	}

	wlr_buffer_init(&buffer->base, &msg_buffer_impl, width, height);
	buffer->format = DRM_FORMAT_ARGB8888;
	buffer->stride = stride;

	buffer->data = malloc(buffer->stride * height);
	if(buffer->data == NULL) {
		free(buffer);
		return NULL;
	}

	return buffer;
}
cairo_subpixel_order_t
to_cairo_subpixel_order(const enum wl_output_subpixel subpixel) {
	switch(subpixel) {
	case WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB:
		return CAIRO_SUBPIXEL_ORDER_RGB;
	case WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR:
		return CAIRO_SUBPIXEL_ORDER_BGR;
	case WL_OUTPUT_SUBPIXEL_VERTICAL_RGB:
		return CAIRO_SUBPIXEL_ORDER_VRGB;
	case WL_OUTPUT_SUBPIXEL_VERTICAL_BGR:
		return CAIRO_SUBPIXEL_ORDER_VBGR;
	default:
		return CAIRO_SUBPIXEL_ORDER_DEFAULT;
	}
	return CAIRO_SUBPIXEL_ORDER_DEFAULT;
}

struct msg_buffer *
create_message_texture(const char *string, const struct cg_output *output) {
	const int WIDTH_PADDING = 8;
	const int HEIGHT_PADDING = 2;

	double scale = output->wlr_output->scale;
	int width = 0;
	int height = 0;

	// We must use a non-nil cairo_t for cairo_set_font_options to work.
	// Therefore, we cannot use cairo_create(NULL).
	cairo_surface_t *dummy_surface =
	    cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
	// This occurs when we are fuzzing. In that case, do nothing
	if(dummy_surface == NULL) {
		return NULL;
	}

	cairo_t *c = cairo_create(dummy_surface);

	cairo_set_antialias(c, CAIRO_ANTIALIAS_BEST);
	cairo_font_options_t *fo = cairo_font_options_create();
	cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_FULL);
	cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_SUBPIXEL);
	cairo_font_options_set_subpixel_order(
	    fo, to_cairo_subpixel_order(output->wlr_output->subpixel));
	cairo_set_font_options(c, fo);
	get_text_size(c, output->server->message_config.font, &width, &height, NULL,
	              scale, "%s", string);
	width += 2 * WIDTH_PADDING;
	height += 2 * HEIGHT_PADDING;
	cairo_surface_destroy(dummy_surface);
	cairo_destroy(c);

	cairo_surface_t *surface =
	    cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cairo = cairo_create(surface);
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_BEST);
	cairo_set_font_options(cairo, fo);
	cairo_font_options_destroy(fo);
	float *bg_col = output->server->message_config.bg_color;
	cairo_set_source_rgba(cairo, bg_col[0], bg_col[1], bg_col[2], bg_col[3]);
	cairo_paint(cairo);
	float *fg_col = output->server->message_config.fg_color;
	cairo_set_source_rgba(cairo, fg_col[0], fg_col[1], fg_col[2], fg_col[3]);
	cairo_set_line_width(cairo, 2);
	cairo_rectangle(cairo, 0, 0, width, height);
	cairo_stroke(cairo);
	cairo_move_to(cairo, WIDTH_PADDING, HEIGHT_PADDING);

	pango_printf(cairo, output->server->message_config.font, scale, "%s",
	             string);

	cairo_surface_flush(surface);
	unsigned char *data = cairo_image_surface_get_data(surface);
	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);

	struct msg_buffer *buf = msg_buffer_create(width, height, stride);
	void *data_ptr;

	if(!wlr_buffer_begin_data_ptr_access(&buf->base,
	                                     WLR_BUFFER_DATA_PTR_ACCESS_WRITE,
	                                     &data_ptr, NULL, NULL)) {
		wlr_log(WLR_ERROR, "Failed to get pointer access to message buffer");
		return NULL;
	}
	memcpy(data_ptr, data, stride * height);
	wlr_buffer_end_data_ptr_access(&buf->base);

	cairo_surface_destroy(surface);
	cairo_destroy(cairo);
	return buf;
}

#if CG_HAS_FANALYZE
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak" //NOLINT
#endif
void
message_set_output(struct cg_output *output, const char *string,
                   struct wlr_box *box, enum cg_message_anchor anchor) {
	struct cg_message *message = malloc(sizeof(struct cg_message));
	if(!message) {
		wlr_log(WLR_ERROR, "Error allocating message structure");
		free(box);
		return;
	}
	struct msg_buffer *buf = create_message_texture(string, output);
	if(!buf) {
		wlr_log(WLR_ERROR, "Could not create message texture");
		free(box);
		free(message);
		return;
	}
	message->position = box;
	wl_list_insert(&output->messages, &message->link);

	double scale = output->wlr_output->scale;
	int width = buf->base.width / scale;
	int height = buf->base.height / scale;
	message->position->width = width;
	message->position->height = height;
	switch(anchor) {
	case CG_MESSAGE_TOP_LEFT:
      message->position->x = 0;
      message->position->y = 0;
      break;
	case CG_MESSAGE_TOP_CENTER:
      message->position->x -= width / 2;
      message->position->y = 0;
      break;
	case CG_MESSAGE_TOP_RIGHT:
		message->position->x -= width;
      message->position->y = 0;
		break;
	case CG_MESSAGE_BOTTOM_LEFT:
      message->position->x = 0;
		message->position->y -= height;
		break;
	case CG_MESSAGE_BOTTOM_CENTER:
      message->position->x -= width / 2;
		message->position->y -= height;
		break;
	case CG_MESSAGE_BOTTOM_RIGHT:
		message->position->x -= width;
		message->position->y -= height;
		break;
	case CG_MESSAGE_CENTER:
		message->position->x -= width / 2;
		message->position->y -= height / 2;
		break;
	default:
		break;
	}

	struct wlr_scene_output *scene_output =
	    wlr_scene_get_scene_output(output->server->scene, output->wlr_output);
	if(scene_output == NULL) {
		return;
	}
	message->message =
	    wlr_scene_buffer_create(&scene_output->scene->tree, &buf->base);
	wlr_scene_node_raise_to_top(&message->message->node);
	wlr_scene_node_set_enabled(&message->message->node, true);
	struct wlr_box outp_box;
	wlr_output_layout_get_box(output->server->output_layout, output->wlr_output,
	                          &outp_box);
	wlr_scene_buffer_set_dest_size(message->message, width, height);
	wlr_scene_node_set_position(&message->message->node,
	                            message->position->x + outp_box.x,
	                            message->position->y + outp_box.y);
}

void
message_printf(struct cg_output *output, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	char *buffer = malloc_vsprintf_va_list(fmt, ap);
	va_end(ap);
	if(buffer == NULL) {
		wlr_log(WLR_ERROR, "Failed to allocate buffer in message_printf");
		return;
	}

	struct wlr_box *box = malloc(sizeof(struct wlr_box));
	if(box == NULL) {
		wlr_log(WLR_ERROR, "Failed to allocate box in message_printf");
		free(buffer);
		return;
	}
	struct wlr_box output_box;
	wlr_output_layout_get_box(output->server->output_layout, output->wlr_output,
	                          &output_box);
	
   box->width  = 0;
	box->height = 0;
	switch(output->server->message_config.anchor) {
	case CG_MESSAGE_TOP_LEFT:
	   box->x = 0;
	   box->y = 0;
      break;
   case CG_MESSAGE_TOP_CENTER:
      box->x = output_box.width /2;
      box->y = 0;
      break;
	case CG_MESSAGE_TOP_RIGHT:
	   box->x = output_box.width;
	   box->y = 0;
		break;
	case CG_MESSAGE_BOTTOM_LEFT:
	   box->x = 0;
	   box->y = output_box.height;
		break;
   case CG_MESSAGE_BOTTOM_CENTER:
      box->x = output_box.width /2;
      box->y = output_box.height;
      break;
	case CG_MESSAGE_BOTTOM_RIGHT:
	   box->x = output_box.width;
	   box->y = output_box.height;
		break;
	case CG_MESSAGE_CENTER:
	   box->x = output_box.width / 2;
	   box->y = output_box.height / 2;
		break;
	default:
		break;
	}
	
   message_set_output(output, buffer, box, output->server->message_config.anchor);
	free(buffer);
	alarm(output->server->message_config.display_time);
}
#if CG_HAS_FANALYZE
#pragma GCC diagnostic pop
#endif

void
message_printf_pos(struct cg_output *output, struct wlr_box *position,
                   const enum cg_message_anchor anchor, const char *fmt, ...) {
	uint16_t buf_len = 256;
	char *buffer = (char *)malloc(buf_len * sizeof(char));
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buffer, buf_len, fmt, ap);
	va_end(ap);

	message_set_output(output, buffer, position, anchor);
	free(buffer);
	alarm(output->server->message_config.display_time);
}

void
message_clear(struct cg_output *output) {
	struct cg_message *message, *tmp;
	wl_list_for_each_safe(message, tmp, &output->messages, link) {
		wl_list_remove(&message->link);
		struct wlr_buffer *buf = message->message->buffer;
		wlr_scene_node_destroy(&message->message->node);
		free(message->position);
		buf->impl->destroy(buf);
		free(message);
	}
}
