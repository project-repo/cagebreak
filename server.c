/*
 * Cagebreak: A Wayland tiling compositor.
 *
 * Copyright (C) 2020 The Cagebreak Authors
 * Copyright (C) 2018-2019 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200809L

#include <string.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_box.h>

#include "server.h"

void
display_terminate(struct cg_server *server) {
	if(server == NULL) {
		return;
	}
	server->running = false;
	wl_display_terminate(server->wl_display);
}

/* Returns the index of a mode given its name or "-1" if the mode is not found.
 */
int
get_mode_index_from_name(char *const *modes, const char *mode_name) {
	for(int i = 0; modes[i] != NULL; ++i) {
		if(strcmp(modes[i], mode_name) == 0) {
			return i;
		}
	}
	return -1;
}
