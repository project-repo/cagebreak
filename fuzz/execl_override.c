// Copyright 2020 - 2025, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT
// This file is used by the fuzzer in order to prevent executing shell commands.

#define _GNU_SOURCE
#include <cairo.h>
#include <cairo/cairo.h>
#include <stdlib.h>

int
fork(void) {
	return 1;
}

void
wlr_texture_get_size(struct wlr_texture *texture, int *width, int *height) {
	if(width != NULL) {
		*width = 0;
	}

	if(height != NULL) {
		*height = 0;
	}
}

cairo_surface_t *
cairo_image_surface_create(cairo_format_t fmt, int width, int height) {
	return NULL;
}
