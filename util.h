#ifndef CG_UTIL_H
#define CG_UTIL_H

#include <stdio.h>

struct wlr_box;

/** Apply scale to a width or height. */
int
scale_length(int length, int offset, double scale);

void
scale_box(struct wlr_box *box, double scale);

char *
malloc_vsprintf(const char *fmt, ...);
char *
malloc_vsprintf_va_list(const char *fmt, va_list list);

#endif
