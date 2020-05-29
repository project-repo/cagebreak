/*
 * Cagebreak: A Wayland tiling compositor.
 *
 * Copyright (C) 2018-2020 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200812L

#include <stdlib.h>

#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "config.h"
#include "../keybinding.h"
#include "../message.h"
#include "../output.h"
#include "../parse.h"
#include "../seat.h"
#include "../server.h"
#include "../workspace.h"
#if CG_HAS_XWAYLAND
#include "../xwayland.h"
#endif

#include "fuzz-lib.h"

int
fuzz_cmds(struct cg_server *server, char *line) {
	if(strncmp(line, "mcurs",5) == 0) {
		move_cursor(&line[5], server);
		return 0;
	} else if(strncmp(line, "noutp",5) == 0) {
		create_output(&line[5], server);
		return 0;
	} else if(strncmp(line, "doutp",5) == 0) {
		destroy_output(&line[5], server);
		return 0;
	} else if(strncmp(line, "crdev",5) == 0) {
		return -1;
		create_input_device(&line[5], server);
		return 0;
	} else if(strncmp(line, "ddev",5) == 0) {
		destroy_input_device(&line[5], server);
		return 0;
	}
	return -1;
}

/* Parse config file. Lines longer than "max_line_size" are ignored */
int
set_configuration(struct cg_server *server, char *content) {
	char *line;
	for(unsigned int line_num = 1;
	    (line = strtok_r(NULL, "\n", &content)) != NULL; ++line_num) {
		line[strcspn(line, "\n")] = '\0';
		if(*line != '\0' && *line != '#') {
			char *errstr;
			server->running=true;
			if(fuzz_cmds(server, line) == 0) {
				continue;
			}
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
	int max_line_size = 256 > size ? size : 256;
	char *str = malloc(sizeof(char) * max_line_size);
	strncpy(str, (char *)data, max_line_size);
	str[max_line_size - 1] = 0;
	set_configuration(&server, str);
	free(str);
	keybinding_list_free(server.keybindings);
	server.keybindings = keybinding_list_init();
	run_action(KEYBINDING_WORKSPACES, &server,
	           (union keybinding_params){.i = 1});
	run_action(KEYBINDING_LAYOUT_FULLSCREEN, &server,
	           (union keybinding_params){.c = NULL});
	wl_display_flush_clients(server.wl_display);
	wl_display_destroy_clients(server.wl_display);
	struct cg_output *output;
	wl_list_for_each(output, &server.outputs, link) {
		message_clear(output);
		struct cg_view *view;
		wl_list_for_each(view, &(*output->workspaces)->views, link) {
			view_unmap(view);
			view_destroy(view);
		}
		wl_list_for_each(view, &(*output->workspaces)->unmanaged_views, link) {
			view_unmap(view);
			view_destroy(view);
		}
	}
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
	struct cg_keyboard_group *group, *group_tmp;
	wl_list_for_each_safe(group, group_tmp, &server.seat->keyboard_groups, link) {
		wl_list_remove(&group->link);
		wlr_keyboard_group_destroy(group->wlr_group);
		wl_event_source_remove(group->key_repeat_timer);
		free(group);
	}
	struct cg_pointer *pointer, *pointer_tmp;
	wl_list_for_each_safe(pointer, pointer_tmp, &server.seat->pointers, link) {
		pointer->destroy.notify(&pointer->destroy, NULL);
	}
	struct cg_touch *touch, *touch_tmp;
	wl_list_for_each_safe(touch, touch_tmp, &server.seat->touch, link) {
		touch->destroy.notify(&touch->destroy, NULL);
	}

	while(wl_list_length(&server.outputs) != 1) {
		server.curr_output->damage_destroy.notify(&server.curr_output->damage_destroy,NULL);
	}

	return 0;
}
