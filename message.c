#include "pango.h"
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <unistd.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output_layout.h>

#include "cairo.h"
#include "message.h"
#include "output.h"
#include "server.h"

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

struct wlr_texture *
create_message_texture(const char *string, const struct cg_output *output) {
	const int PADDING = 2;
	const int MESSAGE_HEIGHT = 17 + 2 * PADDING;
	const char *font = "pango:Monospace 10";

	struct wlr_texture *texture;
	double scale = output->wlr_output->scale;
	int width = 0;
	int height = MESSAGE_HEIGHT * scale;

	// We must use a non-nil cairo_t for cairo_set_font_options to work.
	// Therefore, we cannot use cairo_create(NULL).
	cairo_surface_t *dummy_surface =
	    cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
	cairo_t *c = cairo_create(dummy_surface);
	cairo_set_antialias(c, CAIRO_ANTIALIAS_BEST);
	cairo_font_options_t *fo = cairo_font_options_create();
	cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_FULL);
	cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_SUBPIXEL);
	cairo_font_options_set_subpixel_order(
	    fo, to_cairo_subpixel_order(output->wlr_output->subpixel));
	cairo_set_font_options(c, fo);
	get_text_size(c, font, &width, NULL, NULL, scale, "%s", string);
	width += 2 * PADDING;
	cairo_surface_destroy(dummy_surface);
	cairo_destroy(c);

	cairo_surface_t *surface =
	    cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cairo = cairo_create(surface);
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_BEST);
	cairo_set_font_options(cairo, fo);
	cairo_font_options_destroy(fo);
	cairo_set_source_rgba(cairo, 0.9, 0.85, 0.85, 1.0);
	cairo_paint(cairo);
	PangoContext *pango = pango_cairo_create_context(cairo);
	cairo_set_source_rgba(cairo, 0, 0, 0, 1);
	cairo_set_line_width(cairo, 2);
	cairo_rectangle(cairo, 0, 0, width, height);
	cairo_stroke(cairo);
	cairo_move_to(cairo, PADDING, PADDING);

	pango_printf(cairo, font, scale, "%s", string);

	cairo_surface_flush(surface);
	unsigned char *data = cairo_image_surface_get_data(surface);
	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
	struct wlr_renderer *renderer =
	    wlr_backend_get_renderer(output->wlr_output->backend);
	texture = wlr_texture_from_pixels(renderer, WL_SHM_FORMAT_ARGB8888, stride,
	                                  width, height, data);
	cairo_surface_destroy(surface);
	g_object_unref(pango);
	cairo_destroy(cairo);
	return texture;
}

void
message_set_output(struct cg_output *output, const char *string,
                   struct wlr_box *box, enum cg_message_align align) {
	struct cg_message *message = malloc(sizeof(struct cg_message));
	message->message = create_message_texture(string, output);
	message->position = box;
	wl_list_insert(&output->messages, &message->link);

	int width, height;
	wlr_texture_get_size(message->message, &width, &height);
	message->position->width = width;
	message->position->height = height;
	switch(align) {
	case CG_MESSAGE_TOP_RIGHT: {
		message->position->x -= width;
		break;
	}
	case CG_MESSAGE_BOTTOM_LEFT: {
		message->position->y -= height;
		break;
	}
	case CG_MESSAGE_BOTTOM_RIGHT: {
		message->position->x -= width;
		message->position->y -= height;
		break;
	}
	case CG_MESSAGE_CENTER: {
		message->position->x -= width / 2;
		message->position->y -= height / 2;
		break;
	}
	case CG_MESSAGE_TOP_LEFT:
	default:
		break;
	}
	wlr_output_damage_add_box(output->damage, message->position);
}

void
message_printf(struct cg_output *output, const char *fmt, ...) {
	uint16_t buf_len = 256;
	char *buffer = (char *)malloc(buf_len * sizeof(char));
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buffer, buf_len, fmt, ap);
	va_end(ap);

	struct wlr_box *box = malloc(sizeof(struct wlr_box));
	struct wlr_box *output_box = wlr_output_layout_get_box(
	    output->server->output_layout, output->wlr_output);

	box->x = output_box->width;
	box->y = 0;
	box->width = 0;
	box->height = 0;

	message_set_output(output, buffer, box, CG_MESSAGE_TOP_RIGHT);
	free(buffer);
	free(box);
	alarm(output->server->message_timeout);
}

void
message_printf_pos(struct cg_output *output, struct wlr_box *position,
                   const enum cg_message_align align, const char *fmt, ...) {
	uint16_t buf_len = 256;
	char *buffer = (char *)malloc(buf_len * sizeof(char));
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buffer, buf_len, fmt, ap);
	va_end(ap);

	message_set_output(output, buffer, position, align);
	free(buffer);
	alarm(output->server->message_timeout);
}

void
message_clear(struct cg_output *output) {
	struct cg_message *message, *tmp;
	wl_list_for_each_safe(message, tmp, &output->messages, link) {
		wl_list_remove(&message->link);
		wlr_output_damage_add_box(output->damage, message->position);
		wlr_texture_destroy(message->message);
		free(message->position);
		free(message);
	}
}
