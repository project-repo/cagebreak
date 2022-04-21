#ifndef KEYBINDING_H

#define KEYBINDING_H

#include "config.h"

#include <stdbool.h>
#include <stdint.h>
#include <xkbcommon/xkbcommon.h>

struct cg_server;

/* Important: if you add a keybinding which uses data.c or requires "free"
 * to be called, don't forget to add it to the function "keybinding_list_free"
 * in keybinding.c */
enum keybinding_action {
	KEYBINDING_RUN_COMMAND, // data.c is the string to execute
	KEYBINDING_CLOSE_VIEW,
	KEYBINDING_SPLIT_VERTICAL,
	KEYBINDING_SPLIT_HORIZONTAL,
	KEYBINDING_CHANGE_TTY, // data.u is the desired tty
	KEYBINDING_LAYOUT_FULLSCREEN,
	KEYBINDING_CYCLE_VIEWS,       // data.b is 0 if forward, 1 if reverse
	KEYBINDING_CYCLE_TILES,       // data.b is 0 if forward, 1 if reverse
	KEYBINDING_CYCLE_OUTPUT,      // data.b is 0 if forward, 1 if reverse
	KEYBINDING_CONFIGURE_OUTPUT,  // data.o_cfg is the desired output
	                              // configuration
	KEYBINDING_CONFIGURE_MESSAGE, // data.m_cfg is the desired config
	KEYBINDING_CONFIGURE_INPUT,   // data.i_cfg is the desired input
	                              // configuration
	KEYBINDING_QUIT,
	KEYBINDING_NOOP,
	KEYBINDING_SWITCH_OUTPUT,          // data.u is the desired output
	KEYBINDING_SWITCH_WORKSPACE,       // data.u is the desired workspace
	KEYBINDING_SWITCH_MODE,            // data.u is the desired mode
	KEYBINDING_SWITCH_DEFAULT_MODE,    // data.u is the desired mode
	KEYBINDING_RESIZE_TILE_HORIZONTAL, // data.i is the number of pixels to add
	                                   // to the current width
	KEYBINDING_RESIZE_TILE_VERTICAL, // data.i is the number of pixels to add to
	                                 // the current height
	KEYBINDING_MOVE_VIEW_TO_WORKSPACE,    // data.u is the desired workspace
	KEYBINDING_MOVE_VIEW_TO_OUTPUT,       // data.u is the desired output
	KEYBINDING_MOVE_VIEW_TO_CYCLE_OUTPUT, // data.b is 0 if forward, 1 if
	                                      // reverse
	KEYBINDING_SHOW_TIME,
	KEYBINDING_SHOW_INFO,
	KEYBINDING_DISPLAY_MESSAGE,

	KEYBINDING_SWAP_LEFT,
	KEYBINDING_SWAP_RIGHT,
	KEYBINDING_SWAP_TOP,
	KEYBINDING_SWAP_BOTTOM,

	KEYBINDING_FOCUS_LEFT,
	KEYBINDING_FOCUS_RIGHT,
	KEYBINDING_FOCUS_TOP,
	KEYBINDING_FOCUS_BOTTOM,

	KEYBINDING_DEFINEKEY,  // data.kb is the keybinding definition
	KEYBINDING_BACKGROUND, // data.color is the background color
	KEYBINDING_DEFINEMODE, // data.c is the mode name
	KEYBINDING_WORKSPACES, // data.i is the number of workspaces
};

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
keybinding_cycle_outputs(struct cg_server *server, bool reverse);
struct keybinding **
find_keybinding(const struct keybinding_list *list,
                const struct keybinding *keybinding);
struct keybinding_list *
keybinding_list_init();

int
run_action(enum keybinding_action action, struct cg_server *server,
           union keybinding_params data);
void
keybinding_free(struct keybinding *keybinding, bool recursive);

#endif /* end of include guard KEYBINDINGS_H */
