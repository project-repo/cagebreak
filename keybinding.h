// Copyright 2020 - 2023, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#ifndef KEYBINDING_H

#define KEYBINDING_H

#include "config.h"

#include <stdbool.h>
#include <stdint.h>
#include <xkbcommon/xkbcommon.h>

struct cg_server;

#define FOREACH_KEYBINDING(KEYBINDING)                                         \
	KEYBINDING(KEYBINDING_RUN_COMMAND,                                         \
	           exec) /* data.c is the string to execute */                     \
	KEYBINDING(KEYBINDING_CLOSE_VIEW, close)                                   \
	KEYBINDING(KEYBINDING_SPLIT_VERTICAL, vsplit)                              \
	KEYBINDING(KEYBINDING_SPLIT_HORIZONTAL, hsplit)                            \
	KEYBINDING(KEYBINDING_CHANGE_TTY, switchvt) /*data.u is the desired tty*/  \
	KEYBINDING(KEYBINDING_LAYOUT_FULLSCREEN, only)                             \
	KEYBINDING(KEYBINDING_CYCLE_VIEWS,                                         \
	           cycle_views) /* data.b is 0 if forward, 1 if reverse */         \
	KEYBINDING(KEYBINDING_CYCLE_TILES,                                         \
	           cycle_tiles) /* data.b is 0 if forward, 1 if reverse */         \
	KEYBINDING(KEYBINDING_CYCLE_OUTPUT,                                        \
	           cycle_outputs) /* data.b is 0 if forward, 1 if reverse */       \
	KEYBINDING(KEYBINDING_CONFIGURE_OUTPUT,                                    \
	           output) /* data.o_cfg is the desired output */                  \
                                                                               \
	KEYBINDING(KEYBINDING_CONFIGURE_MESSAGE,                                   \
	           configure_message) /* data.m_cfg is the desired config */       \
	KEYBINDING(KEYBINDING_CONFIGURE_INPUT,                                     \
	           input) /* data.i_cfg is the desired input configuration */      \
                                                                               \
	KEYBINDING(KEYBINDING_QUIT, quit)                                          \
	KEYBINDING(KEYBINDING_NOOP, abort)                                         \
	KEYBINDING(KEYBINDING_SWITCH_OUTPUT,                                       \
	           screen) /* data.u is the desired output */                      \
	KEYBINDING(KEYBINDING_SWITCH_WORKSPACE,                                    \
	           workspace) /* data.u is the desired workspace */                \
	KEYBINDING(KEYBINDING_SWITCH_MODE, mode) /* data.u is the desired mode */  \
	KEYBINDING(KEYBINDING_SWITCH_DEFAULT_MODE,                                 \
	           setmode) /* data.u is the desired mode */                       \
	KEYBINDING(                                                                \
	    KEYBINDING_RESIZE_TILE_HORIZONTAL,                                     \
	    resize_tile_horizontal) /* data.i is the number of pixels to add */    \
                                                                               \
	KEYBINDING(                                                                \
	    KEYBINDING_RESIZE_TILE_VERTICAL,                                       \
	    resize_tile_vertical) /* data.i is the number of pixels to add to */   \
                                                                               \
	KEYBINDING(KEYBINDING_MOVE_VIEW_TO_WORKSPACE,                              \
	           movetoworkspace) /* data.u is the desired workspace */          \
	KEYBINDING(KEYBINDING_MOVE_VIEW_TO_OUTPUT,                                 \
	           movetoscreen) /* data.u is the desired output */                \
	KEYBINDING(KEYBINDING_MOVE_VIEW_TO_CYCLE_OUTPUT,                           \
	           move_view_to_cycle_output) /* data.b is 0 if forward, 1 if */   \
                                                                               \
	KEYBINDING(KEYBINDING_DUMP, dump)                                          \
	KEYBINDING(KEYBINDING_SHOW_TIME, time)                                     \
	KEYBINDING(KEYBINDING_SHOW_INFO, show_info)                                \
	KEYBINDING(KEYBINDING_DISPLAY_MESSAGE, message)                            \
	KEYBINDING(KEYBINDING_CURSOR, cursor)                                      \
                                                                               \
	KEYBINDING(KEYBINDING_SWAP_LEFT, exchangeleft)                             \
	KEYBINDING(KEYBINDING_SWAP_RIGHT, exchangeright)                           \
	KEYBINDING(KEYBINDING_SWAP_TOP, exchangeup)                                \
	KEYBINDING(KEYBINDING_SWAP_BOTTOM, exchangedown)                           \
                                                                               \
	KEYBINDING(KEYBINDING_FOCUS_LEFT, focusleft)                               \
	KEYBINDING(KEYBINDING_FOCUS_RIGHT, focusright)                             \
	KEYBINDING(KEYBINDING_FOCUS_TOP, focusup)                                  \
	KEYBINDING(KEYBINDING_FOCUS_BOTTOM, focusdown)                             \
                                                                               \
	KEYBINDING(KEYBINDING_DEFINEKEY,                                           \
	           definekey) /* data.kb is the keybinding definition */           \
	KEYBINDING(KEYBINDING_BACKGROUND,                                          \
	           background) /* data.color is the background color */            \
	KEYBINDING(KEYBINDING_DEFINEMODE,                                          \
	           definemode) /* data.c is the mode name */                       \
	KEYBINDING(KEYBINDING_WORKSPACES,                                          \
	           workspaces) /* data.i is the number of workspaces */

#define GENERATE_ENUM(ENUM, NAME) ENUM,
#define GENERATE_STRING(STRING, NAME) #NAME,

/* Important: if you add a keybinding which uses data.c or requires "free"
 * to be called, don't forget to add it to the function "keybinding_list_free"
 * in keybinding.c */
enum keybinding_action { FOREACH_KEYBINDING(GENERATE_ENUM) };

extern char *keybinding_action_string[];

union keybinding_params {
	char *c;
	uint32_t u;
	int32_t i;
	bool b;
	float color[3];
	struct keybinding *kb;
	struct cg_output_config *o_cfg;
	struct cg_input_config *i_cfg;
	struct cg_message_config *m_cfg;
};

struct keybinding {
	uint16_t mode;
	xkb_mod_mask_t modifiers;
	xkb_keysym_t key;
	enum keybinding_action action;
	union keybinding_params data; // See enum keybinding_action for details
};

struct keybinding_list {
	uint32_t length;
	uint32_t capacity;
	struct keybinding **keybindings;
};

int
keybinding_list_push(struct keybinding_list *list,
                     struct keybinding *keybinding);
void
keybinding_list_free(struct keybinding_list *list);
void
keybinding_cycle_outputs(struct cg_server *server, bool reverse,
                         bool trigger_event);
struct keybinding **
find_keybinding(const struct keybinding_list *list,
                const struct keybinding *keybinding);
struct keybinding_list *
keybinding_list_init(void);

int
run_action(enum keybinding_action action, struct cg_server *server,
           union keybinding_params data);
void
keybinding_free(struct keybinding *keybinding, bool recursive);

#endif /* end of include guard KEYBINDINGS_H */
