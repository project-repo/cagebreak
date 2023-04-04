// Copyright 2020 - 2023, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#include <wlr/util/box.h>

#include "util.h"
#include <math.h>
#include <stdlib.h>

int
scale_length(int length, int offset, double scale) {
	/**
	 * One does not simply multiply the width by the scale. We allow fractional
	 * scaling, which means the resulting scaled width might be a decimal.
	 * So we round it.
	 *
	 * But even this can produce undesirable results depending on the X or Y
	 * offset of the box. For example, with a scale of 1.5, a box with
	 * width=1 should not scale to 2px if its X coordinate is 1, because the
	 * X coordinate would have scaled to 2px.
	 */
	return (int)(round((offset + length) * scale) - round(offset * scale));
}

void
scale_box(struct wlr_box *box, double scale) {
	box->width = scale_length(box->width, box->x, scale);
	box->height = scale_length(box->height, box->y, scale);
	box->x = (int)round(box->x * scale);
	box->y = (int)round(box->y * scale);
}

char *
malloc_vsprintf_va_list(const char *fmt, va_list ap) {
	va_list ap2;
	va_copy(ap2, ap);
	int len = vsnprintf(NULL, 0, fmt, ap);
	char *ret = malloc(sizeof(char) * (len + 1));
	vsnprintf(ret, len + 1, fmt, ap2);
	va_end(ap2);
	return ret;
}

char *
malloc_vsprintf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	char *ret = malloc_vsprintf_va_list(fmt, args);
	return ret;
}
