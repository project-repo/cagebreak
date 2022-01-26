/*
 * Cagebreak: A Wayland tiling compositor.
 *
 * Copyright (C) 2020-2022 The Cagebreak Authors
 * Copyright (C) 2018-2019 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/util/box.h>
#include <wlr/types/wlr_output.h>

#include "input_manager.h"
#include "output.h"
#include "server.h"
#include "util.h"

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

char *
server_show_info(struct cg_server *server) {
	char *output_str = strdup(""), *output_str_tmp;
	struct cg_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if(!output_str) {
			return NULL;
		}
		output_str_tmp = output_str;
		output_str = malloc_vsprintf("%s\t * %s\n", output_str,
		                             output->wlr_output->name);
		free(output_str_tmp);
	}
	char *input_str = strdup(""), *input_str_tmp;
	struct cg_input_device *input;
	wl_list_for_each(input, &server->input->devices, link) {
		if(!input_str) {
			return NULL;
		}
		input_str_tmp = input_str;
		if(strcmp(input->identifier, "") != 0) {
			input_str =
			    malloc_vsprintf("%s\t * %s\n", input_str, input->identifier);
		}
		free(input_str_tmp);
	}
	char *ret =
	    malloc_vsprintf("Outputs:\n%sInputs:\n%s", output_str, input_str);
	free(output_str);
	free(input_str);
	return ret;
}
