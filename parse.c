#define _POSIX_C_SOURCE 200812L

#include <stdlib.h>
#include <string.h>
#include <wlr/util/log.h>

#include "keybinding.h"
#include "output.h"
#include "parse.h"
#include "server.h"
#include "workspace.h"

/* parses a key definition (e.g. "S-Tab") and sets key and modifiers in
 * keybinding respectivly */
int
parse_key(struct keybinding *keybinding, const char *key_def) {
	if(key_def == NULL) {
		wlr_log(WLR_ERROR, "Expected key definition, got nothing.");
		return -1;
	}
	keybinding->modifiers = 0;
	while(strlen(key_def) > 1 && key_def[1] == '-') {
		switch(key_def[0]) {
		case 'S':
			keybinding->modifiers |= WLR_MODIFIER_SHIFT;
			break;
		case 'A':
			keybinding->modifiers |= WLR_MODIFIER_ALT;
			break;
		case 'C':
			keybinding->modifiers |= WLR_MODIFIER_CTRL;
			break;
		case 'L':
			keybinding->modifiers |= WLR_MODIFIER_LOGO;
			break;
		case '2':
			keybinding->modifiers |= WLR_MODIFIER_MOD2;
			break;
		case '3':
			keybinding->modifiers |= WLR_MODIFIER_MOD3;
			break;
		case '5':
			keybinding->modifiers |= WLR_MODIFIER_MOD5;
			break;
		default:
			wlr_log(WLR_ERROR, "Unknown modifier \"%c\"", key_def[0]);
			return -1;
		}
		key_def += 2;
	}
	xkb_keysym_t keysym = xkb_keysym_from_name(key_def, XKB_KEYSYM_NO_FLAGS);
	if(keysym == XKB_KEY_NoSymbol) {
		wlr_log(WLR_ERROR, "Could not convert key \"%s\" to keysym.", key_def);
		return -1;
	}
	keybinding->key = keysym;
	return 0;
}

int
parse_command(struct cg_server *server, struct keybinding *keybinding, char *saveptr);

/* Parse a keybinding definition and return it if successful, else return NULL
 */
struct keybinding *
parse_keybinding(struct cg_server *server, char **saveptr) {
	struct keybinding *keybinding = malloc(sizeof(struct keybinding));
	char *key = strtok_r(NULL, " ", saveptr);
	if(parse_key(keybinding, key) != 0) {
		wlr_log(WLR_ERROR, "Could not parse key definition \"%s\"", key);
		free(keybinding);
		return NULL;
	}
	if(parse_command(server, keybinding, *saveptr) != 0) {
		free(keybinding);
		return NULL;
	}
	return keybinding;
}

struct keybinding *
parse_bind(struct cg_server *server, char **saveptr) {
	struct keybinding *keybinding = parse_keybinding(server, saveptr);
	if(keybinding == NULL) {
		wlr_log(WLR_ERROR, "Could not parse keybinding for \"bind\".");
		return NULL;
	}
	keybinding->mode = 1;
	return keybinding;
}

struct keybinding *
parse_definekey(struct cg_server *server, char **saveptr) {
	char *mode = strtok_r(NULL, " ", saveptr);
	if(mode == NULL) {
		wlr_log(WLR_ERROR, "Too few arguments to \"definekey\". Expected mode");
		return NULL;
	}
	int mode_idx = get_mode_index_from_name(server->modes, mode);
	if(mode_idx == -1) {
		wlr_log(WLR_ERROR, "Unknown mode \"%s\"", mode);
		return NULL;
	}
	struct keybinding *keybinding = parse_keybinding(server, saveptr);
	if(keybinding == NULL) {
		wlr_log(WLR_ERROR, "Could not parse keybinding for \"definekey\"");
		return NULL;
	}
	keybinding->mode = mode_idx;
	return keybinding;
}

int
parse_background(struct cg_server *server, float *color, char **saveptr) {
	/* Read rgb numbers */
	for(unsigned int i = 0; i < 3; ++i) {
		char *nstr = strtok_r(NULL, " \n", saveptr);
		int nstrlen;
		if(nstr == NULL||(nstrlen = strlen(nstr))== 0) {
			wlr_log(WLR_ERROR,
			        "Expected three space-separated numbers (rgb) for "
			        "background color setting. Got %d.",
			        i);
			return -1;
		}
		if(nstr[nstrlen-1] == '\n') {
			nstr[nstrlen-1] = '\0';
			--nstrlen;
		}
		char *endptr = NULL;
		float nval = strtof(nstr, &endptr);
		if(endptr != nstr+nstrlen) {
			wlr_log(
			    WLR_ERROR,
			    "Could not parse number \"%s\" for background color setting.",
			    nstr);
			return -1;
		}
		if(nval<0||nval>1) {
			wlr_log(WLR_ERROR, "Expected a number between 0 and 1 for setting of background color. Got %f.",nval);
			return -1;
		}
		color[i] = nval;
	}
	return 0;
}

struct keybinding *
parse_escape(char **saveptr) {
	struct keybinding *keybinding = malloc(sizeof(struct keybinding));
	char *key = strtok_r(NULL, " ", saveptr);
	if(parse_key(keybinding, key) != 0) {
		wlr_log(WLR_ERROR,
		        "Could not parse key definition \"%s\" for \"escape\"", key);
		free(keybinding);
		return NULL;
	}
	keybinding->mode = 0; //"top" mode
	keybinding->action = KEYBINDING_SWITCH_MODE;
	keybinding->data.u = 1; //"root" mode
	return keybinding;
}

char*
parse_definemode(char **saveptr) {
	char *mode = strtok_r(NULL, " ", saveptr);
	if(mode == NULL) {
		wlr_log(WLR_ERROR, "Expected mode to succeed \"definemode\" keyword.");
		return NULL;
	}
	return strdup(mode);
}

int
parse_workspaces(char **saveptr) {
	char *nws_str = strtok_r(NULL, " ", saveptr);
	if(nws_str == NULL) {
		wlr_log(WLR_ERROR,
		        "Expected argument for \"workspaces\" command, got none.");
		return -1;
	}
	long nws = strtol(nws_str, NULL, 10);
	if(!(1 <= nws && nws <= 30)) {
		wlr_log(WLR_ERROR,
		        "More than 30 workspaces are not supported. Received %li",
		        nws);
		return -1;
	}
	return nws;
}

int
parse_command(struct cg_server *server, struct keybinding *keybinding,
             char *saveptr) {
	char *action = strtok_r(NULL, " ", &saveptr);
	if(action == NULL) {
		wlr_log(
		    WLR_ERROR,
		    "Expected an action to parse, got none.");
		return -1;
	}
	keybinding->data = (union keybinding_params){.c = NULL};
	if(strcmp(action, "vsplit") == 0) {
		keybinding->action = KEYBINDING_SPLIT_VERTICAL;
	} else if(strcmp(action, "hsplit") == 0) {
		keybinding->action = KEYBINDING_SPLIT_HORIZONTAL;
	} else if(strcmp(action, "quit") == 0) {
		keybinding->action = KEYBINDING_QUIT;
	} else if(strcmp(action, "focus") == 0) {
		keybinding->action = KEYBINDING_CYCLE_TILES;
		keybinding->data.b = false;
	} else if(strcmp(action, "focusprev") == 0) {
		keybinding->action = KEYBINDING_CYCLE_TILES;
		keybinding->data.b = true;
	} else if(strcmp(action, "next") == 0) {
		keybinding->action = KEYBINDING_CYCLE_VIEWS;
		keybinding->data.b = false;
	} else if(strcmp(action, "prev") == 0) {
		keybinding->action = KEYBINDING_CYCLE_VIEWS;
		keybinding->data.b = true;
	} else if(strcmp(action, "only") == 0) {
		keybinding->action = KEYBINDING_LAYOUT_FULLSCREEN;
	} else if(strcmp(action, "abort") == 0) {
		keybinding->action = KEYBINDING_NOOP;
	} else if(strcmp(action, "time") == 0) {
		keybinding->action = KEYBINDING_SHOW_TIME;
	} else if(strcmp(action, "nextscreen") == 0) {
		keybinding->action = KEYBINDING_CYCLE_OUTPUT;
		keybinding->data.b = false;
	} else if(strcmp(action, "prevscreen") == 0) {
		keybinding->action = KEYBINDING_CYCLE_OUTPUT;
		keybinding->data.b = true;
	} else if(strcmp(action, "exec") == 0) {
		keybinding->action = KEYBINDING_RUN_COMMAND;
		if(saveptr == NULL) {
			wlr_log(WLR_ERROR, "Not enough paramaters to \"exec\". Expected "
			                   "string to execute.");
			return -1;
		}
		keybinding->data.c = strdup(saveptr);
	} else if(strcmp(action, "resizeleft") == 0) {
		keybinding->action = KEYBINDING_RESIZE_TILE_HORIZONTAL;
		keybinding->data.i = -10;
	} else if(strcmp(action, "resizeright") == 0) {
		keybinding->action = KEYBINDING_RESIZE_TILE_HORIZONTAL;
		keybinding->data.i = 10;
	} else if(strcmp(action, "resizedown") == 0) {
		keybinding->action = KEYBINDING_RESIZE_TILE_VERTICAL;
		keybinding->data.i = 10;
	} else if(strcmp(action, "resizeup") == 0) {
		keybinding->action = KEYBINDING_RESIZE_TILE_VERTICAL;
		keybinding->data.i = -10;
	} else if(strcmp(action, "workspace") == 0) {
		keybinding->action = KEYBINDING_SWITCH_WORKSPACE;
		char *nws_str = strtok_r(NULL, " ", &saveptr);
		if(nws_str == NULL) {
			wlr_log(WLR_ERROR,
			        "Expected argument for \"workspace\" action, got none.");
			return -1;
		}

		long ws = strtol(nws_str, NULL, 10);
		if(!(1 <= ws && ws <= server->nws)) {
			wlr_log(WLR_ERROR,
			        "Requested binding for workspace %li, but have %u", ws,
			        server->nws);
			return -1;
		}
		keybinding->data.u = ws - 1;
	} else if(strcmp(action, "movetoworkspace") == 0) {
		keybinding->action = KEYBINDING_MOVE_VIEW_TO_WORKSPACE;
		char *nws_str = strtok_r(NULL, " ", &saveptr);
		if(nws_str == NULL) {
			wlr_log(WLR_ERROR,
			        "Expected argument for \"workspace\" action, got none.");
			return -1;
		}

		long ws = strtol(nws_str, NULL, 10);
		if(!(1 <= ws && ws <= server->nws)) {
			wlr_log(
			    WLR_ERROR,
			    "Requested binding for moving to workspace %li, but have %u",
			    ws, server->nws);
			return -1;
		}
		keybinding->data.u = ws - 1;
	} else if(strcmp(action, "exchangeleft") == 0) {
		keybinding->action = KEYBINDING_SWAP_LEFT;
	} else if(strcmp(action, "exchangeright") == 0) {
		keybinding->action = KEYBINDING_SWAP_RIGHT;
	} else if(strcmp(action, "exchangeup") == 0) {
		keybinding->action = KEYBINDING_SWAP_TOP;
	} else if(strcmp(action, "exchangedown") == 0) {
		keybinding->action = KEYBINDING_SWAP_BOTTOM;
	} else if(strcmp(action, "focusleft") == 0) {
		keybinding->action = KEYBINDING_FOCUS_LEFT;
	} else if(strcmp(action, "focusright") == 0) {
		keybinding->action = KEYBINDING_FOCUS_RIGHT;
	} else if(strcmp(action, "focusup") == 0) {
		keybinding->action = KEYBINDING_FOCUS_TOP;
	} else if(strcmp(action, "focusdown") == 0) {
		keybinding->action = KEYBINDING_FOCUS_BOTTOM;
	} else if(strcmp(action, "movetonextscreen") == 0) {
		keybinding->action = KEYBINDING_MOVE_VIEW_TO_NEXT_OUTPUT;
	} else if(strcmp(action, "switchvt") == 0) {
		keybinding->action = KEYBINDING_CHANGE_TTY;
		char *ntty = strtok_r(NULL, " ", &saveptr);
		if(ntty == NULL) {
			wlr_log(WLR_ERROR,
			        "Expected argument for \"switchvt\" command, got none.");
			return -1;
		}
		long tty = strtol(ntty, NULL, 10);
		keybinding->data.u = tty;
	} else if(strcmp(action, "mode") == 0) {
		keybinding->action = KEYBINDING_SWITCH_MODE;
		char *mode = strtok_r(NULL, " ", &saveptr);
		if(mode == NULL) {
			wlr_log(WLR_ERROR,
			        "Expected mode after \"switch_mode\". Got nothing.");
			return -1;
		}
		int mode_idx = get_mode_index_from_name(server->modes, mode);
		if(mode_idx == -1) {
			wlr_log(WLR_ERROR, "Unknown mode \"%s\" for switch_mode", mode);
			return -1;
		}
		keybinding->data.u = (unsigned int)mode_idx;
	} else if(strcmp(action, "setmode") == 0) {
		keybinding->action = KEYBINDING_SWITCH_DEFAULT_MODE;
		char *mode = strtok_r(NULL, " ", &saveptr);
		if(mode == NULL) {
			wlr_log(
			    WLR_ERROR,
			    "Expected mode after \"switch_default_mode\". Got nothing.");
			return -1;
		}
		int mode_idx = get_mode_index_from_name(server->modes, mode);
		if(mode_idx == -1) {
			wlr_log(WLR_ERROR, "Unknown mode \"%s\" for switch_default_mode",
			        mode);
			return -1;
		}
		keybinding->data.u = (unsigned int)mode_idx;
	} else if(strcmp(action, "bind") == 0) {
		keybinding->action = KEYBINDING_DEFINEKEY;
		keybinding->data.kb = parse_bind(server, &saveptr);
		if(keybinding->data.kb == NULL) {
			return -1;
		}
	} else if(strcmp(action, "definekey") == 0) {
		keybinding->action = KEYBINDING_DEFINEKEY;
		keybinding->data.kb = parse_definekey(server, &saveptr);
		if(keybinding->data.kb == NULL) {
			return -1;
		}
	} else if(strcmp(action, "background") == 0) {
		keybinding->action = KEYBINDING_BACKGROUND;
		if(parse_background(server, keybinding->data.color,&saveptr) != 0) {
			return -1;
		}
	} else if(strcmp(action, "escape") == 0) {
		keybinding->action = KEYBINDING_DEFINEKEY;
		keybinding->data.kb = parse_escape(&saveptr);
		if(keybinding->data.kb == NULL) {
			return -1;
		}
	} else if(strcmp(action, "definemode") == 0) {
		keybinding->action = KEYBINDING_DEFINEMODE;
		keybinding->data.c = parse_definemode(&saveptr);
		if(keybinding->data.c == NULL) {
			return -1;
		}
	} else if(strcmp(action, "workspaces") == 0) {
		keybinding->action = KEYBINDING_WORKSPACES;
		keybinding->data.i=parse_workspaces(&saveptr);
		if(keybinding->data.i<0) {
			return -1;
		}
	} else {
		wlr_log(WLR_ERROR, "Error, unsupported action \"%s\".", action);
		return -1;
	}
	return 0;
}

int
parse_rc_line(struct cg_server *server, char *line) {
	char *saveptr = strdup(line); // Used internally by strtok_r

	struct keybinding *keybinding = malloc(sizeof(struct keybinding));
	if(keybinding == NULL) {
		wlr_log(WLR_ERROR, "Failed to allocate memory for temporary keybinding struct.");
		free(keybinding);
		free(saveptr);
		return -1;
	}
	if(parse_command(server, keybinding, saveptr) != 0) {
		wlr_log(WLR_ERROR, "Error parsing config file.");
		free(keybinding);
		free(saveptr);
		return -1;
	}
	run_action(keybinding->action, server, keybinding->data);
	keybinding_free(keybinding, false);
	free(saveptr);
	return 0;
}
