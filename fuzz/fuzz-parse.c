/*
 * Cagebreak: A Wayland tiling compositor.
 *
 * Copyright (C) 2018-2020 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200812L

#include <stdlib.h>
#include <string.h>

#include <wayland-server-core.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "../keybinding.h"
#include "../message.h"
#include "../output.h"
#include "../parse.h"
#include "../seat.h"
#include "../server.h"
#include "../view.h"
#include "../workspace.h"
#include "config.h"
#if CG_HAS_XWAYLAND
#include "../xwayland.h"
#endif

#include "fuzz-lib.h"

int
set_configuration(struct cg_server *server, char *content) {
	char *line;
	for(unsigned int line_num = 1;
	    (line = strtok_r(NULL, "\n", &content)) != NULL; ++line_num) {
		line[strcspn(line, "\n")] = '\0';
		if(*line != '\0' && *line != '#') {
			char *errstr = NULL;
			server->running = true;
			if(parse_rc_line(server, line, &errstr) != 0) {
				if(errstr != NULL) {
					free(errstr);
				}
				return -1;
			}
		}
	}
	return 0;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if(size == 0) {
		return 0;
	}
	char *str = malloc(sizeof(char) * size);
	strncpy(str, (char *)data, size);
	str[size - 1] = 0;
	set_configuration(&server, str);
	free(str);
	keybinding_list_free(server.keybindings);
	server.keybindings = keybinding_list_init();
	run_action(KEYBINDING_WORKSPACES, &server,
	           (union keybinding_params){.i = 1});
	run_action(KEYBINDING_LAYOUT_FULLSCREEN, &server,
	           (union keybinding_params){.c = NULL});
	struct cg_output *output;
	wl_list_for_each(output, &server.outputs, link) { message_clear(output); }
	for(unsigned int i = 3; server.modes[i] != NULL; ++i) {
		free(server.modes[i]);
	}
	server.modes[3] = NULL;
	server.modes = realloc(server.modes, 4 * sizeof(char *));

	struct cg_output_config *output_config, *output_config_tmp;
	wl_list_for_each_safe(output_config, output_config_tmp,
	                      &server.output_config, link) {
		wl_list_remove(&output_config->link);
		free(output_config->output_name);
		free(output_config);
	}
	return 0;
}
