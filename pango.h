#ifndef _SWAY_PANGO_H
#define _SWAY_PANGO_H
#include <cairo/cairo.h>

void
get_text_size(cairo_t *cairo, const char *font, int *width, int *height,
              int *baseline, double scale, const char *fmt, ...);
void
pango_printf(cairo_t *cairo, const char *font, double scale, const char *fmt,
             ...);

#endif
