#define _POSIX_C_SOURCE 200812L

#include <limits.h>
#include <string.h>
#include <wlr/util/log.h>

#include "keybinding.h"
#include "output.h"
#include "parse.h"
#include "server.h"

char *
malloc_vsprintf(const char *fmt, va_list ap) {
	va_list ap2;
	va_copy(ap2, ap);
	int len = vsnprintf(NULL, 0, fmt, ap);
	char *ret = malloc(sizeof(char) * (len + 1));
	vsnprintf(ret, len + 1, fmt, ap2);
	va_end(ap2);
	return ret;
}

char *
log_error(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	char *ret = malloc_vsprintf(fmt, args);
	if(ret != NULL) {
		wlr_log(WLR_ERROR, "%s", ret);
	}
	va_end(args);
	return ret;
}

/* parses a key definition (e.g. "S-Tab") and sets key and modifiers in
 * keybinding respectivly */
int
parse_key(struct keybinding *keybinding, const char *key_def, char **errstr) {
	if(key_def == NULL) {
		*errstr = log_error("Expected key definition, got nothing.");
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
			*errstr = log_error("Unknown modifier \"%c\"", key_def[0]);
			return -1;
		}
		key_def += 2;
	}
	xkb_keysym_t keysym = xkb_keysym_from_name(key_def, XKB_KEYSYM_NO_FLAGS);
	if(keysym == XKB_KEY_NoSymbol) {
		*errstr = log_error("Could not convert key \"%s\" to keysym.", key_def);
		return -1;
	}
	keybinding->key = keysym;
	return 0;
}

int
parse_command(struct cg_server *server, struct keybinding *keybinding,
              char *saveptr, char **errstr);

/* Parse a keybinding definition and return it if successful, else return NULL
 */
struct keybinding *
parse_keybinding(struct cg_server *server, char **saveptr, char **errstr) {
	struct keybinding *keybinding = malloc(sizeof(struct keybinding));
	if(keybinding == NULL) {
		*errstr = log_error(
		    "Failed to allocate memory for keybinding in parse_keybinding");
		return NULL;
	}
	char *key = strtok_r(NULL, " ", saveptr);
	if(parse_key(keybinding, key, errstr) != 0) {
		wlr_log(WLR_ERROR, "Could not parse key definition \"%s\"", key);
		free(keybinding);
		return NULL;
	}
	if(parse_command(server, keybinding, *saveptr, errstr) != 0) {
		free(keybinding);
		return NULL;
	}
	return keybinding;
}

struct keybinding *
parse_bind(struct cg_server *server, char **saveptr, char **errstr) {
	struct keybinding *keybinding = parse_keybinding(server, saveptr, errstr);
	if(keybinding == NULL) {
		wlr_log(WLR_ERROR, "Could not parse keybinding for \"bind\".");
		return NULL;
	}
	keybinding->mode = 1;
	return keybinding;
}

struct keybinding *
parse_definekey(struct cg_server *server, char **saveptr, char **errstr) {
	char *mode = strtok_r(NULL, " ", saveptr);
	if(mode == NULL) {
		*errstr =
		    log_error("Too few arguments to \"definekey\". Expected mode");
		return NULL;
	}
	int mode_idx = get_mode_index_from_name(server->modes, mode);
	if(mode_idx == -1) {
		*errstr = log_error("Unknown mode \"%s\"", mode);
		return NULL;
	}
	struct keybinding *keybinding = parse_keybinding(server, saveptr, errstr);
	if(keybinding == NULL) {
		wlr_log(WLR_ERROR, "Could not parse keybinding for \"definekey\"");
		return NULL;
	}
	keybinding->mode = mode_idx;
	return keybinding;
}

int
parse_background(struct cg_server *server, float *color, char **saveptr,
                 char **errstr) {
	/* Read rgb numbers */
	for(unsigned int i = 0; i < 3; ++i) {
		char *nstr = strtok_r(NULL, " \n", saveptr);
		int nstrlen;
		if(nstr == NULL || (nstrlen = strlen(nstr)) == 0) {
			*errstr = log_error("Expected three space-separated numbers (rgb) "
			                    "for background color setting. Got %d.",
			                    i);
			return -1;
		}
		if(nstr[nstrlen - 1] == '\n') {
			nstr[nstrlen - 1] = '\0';
			--nstrlen;
		}
		char *endptr = NULL;
		float nval = strtof(nstr, &endptr);
		if(endptr != nstr + nstrlen) {
			*errstr = log_error(
			    "Could not parse number \"%s\" for background color setting.",
			    nstr);
			return -1;
		}
		if(nval < 0 || nval > 1) {
			*errstr = log_error("Expected a number between 0 and 1 for setting "
			                    "of background color. Got %f.",
			                    nval);
			return -1;
		}
		color[i] = nval;
	}
	return 0;
}

struct keybinding *
parse_escape(char **saveptr, char **errstr) {
	struct keybinding *keybinding = malloc(sizeof(struct keybinding));
	if(keybinding == NULL) {
		*errstr = log_error(
		    "Failed to allocate memory for keybinding in parse_escape");
		return NULL;
	}
	char *key = strtok_r(NULL, " ", saveptr);
	if(parse_key(keybinding, key, errstr) != 0) {
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

char *
parse_definemode(char **saveptr, char **errstr) {
	char *mode = strtok_r(NULL, " ", saveptr);
	if(mode == NULL) {
		*errstr = log_error("Expected mode to succeed \"definemode\" keyword.");
		return NULL;
	}
	return strdup(mode);
}

int
parse_workspaces(char **saveptr, char **errstr) {
	char *nws_str = strtok_r(NULL, " ", saveptr);
	if(nws_str == NULL) {
		*errstr = log_error(
		    "Expected argument for \"workspaces\" command, got none.");
		return -1;
	}
	long nws = strtol(nws_str, NULL, 10);
	if(!(1 <= nws && nws <= 30)) {
		*errstr = log_error(
		    "More than 30 workspaces are not supported. Received %li", nws);
		return -1;
	}
	return nws;
}

int
parse_uint(char **saveptr, const char *delim) {
	char *uint_str = strtok_r(NULL, delim, saveptr);
	if(uint_str == NULL) {
		wlr_log(WLR_ERROR, "Expected a non-negative integer, got nothing");
		return -1;
	}
	long uint = strtol(uint_str, NULL, 10);
	if(uint >= 0 && uint <= INT_MAX) {
		return uint;
	} else {
		wlr_log(WLR_ERROR,
		        "Error parsing non-negative integer. Must be a number larger "
		        "or equal to 0 and less or equal to %d",
		        INT_MAX);
		return -1;
	}
}

float
parse_float(char **saveptr, const char *delim) {
	char *uint_str = strtok_r(NULL, delim, saveptr);
	if(uint_str == NULL) {
		wlr_log(WLR_ERROR, "Expected a non-negative float, got nothing");
		return -1;
	}
	float ufloat = strtof(uint_str, NULL);
	if(ufloat >= 0) {
		return ufloat;
	} else {
		wlr_log(WLR_ERROR, "Error parsing non-negative float. Must be a number "
		                   "larger or equal to 0");
		return -1;
	}
}

int
parse_output_config_keyword(char *key_str, enum output_status *status) {
	if(key_str == NULL) {
		return -1;
	}
	if(strcmp(key_str, "pos") == 0) {
		*status = OUTPUT_DEFAULT;
	} else if(strcmp(key_str, "enable") == 0) {
		*status = OUTPUT_ENABLE;
	} else if(strcmp(key_str, "disable") == 0) {
		*status = OUTPUT_DISABLE;
	} else {
		return -1;
	}
	return 0;
}

struct cg_output_config *
parse_output_config(char **saveptr, char **errstr) {
	struct cg_output_config *cfg = malloc(sizeof(struct cg_output_config));
	if(cfg == NULL) {
		*errstr =
		    log_error("Failed to allocate memory for output configuration");
		goto error;
	}
	char *name = strtok_r(NULL, " ", saveptr);
	if(name == NULL) {
		*errstr =
		    log_error("Expected name of output to be configured, got none");
		goto error;
	}
	char *key_str = strtok_r(NULL, " ", saveptr);
	if(parse_output_config_keyword(key_str, &(cfg->status)) != 0) {
		*errstr = log_error("Expected keyword \"pos\", \"enable\" or "
		                    "\"disable\" in output configuration for output %s",
		                    name);
		goto error;
	}
	if(cfg->status == OUTPUT_ENABLE || cfg->status == OUTPUT_DISABLE) {
		cfg->output_name = strdup(name);
		return cfg;
	}

	cfg->pos.x = parse_uint(saveptr, " ");
	if(cfg->pos.x < 0) {
		*errstr = log_error(
		    "Error parsing x coordinate of output configuration for output %s",
		    name);
		goto error;
	}

	cfg->pos.y = parse_uint(saveptr, " ");
	if(cfg->pos.y < 0) {
		*errstr = log_error(
		    "Error parsing y coordinate of output configuration for output %s",
		    name);
		goto error;
	}

	char *res_str = strtok_r(NULL, " ", saveptr);
	if(res_str == NULL || strcmp(res_str, "res") != 0) {
		*errstr = log_error(
		    "Expected keyword \"res\" in output configuration for output %s",
		    name);
		goto error;
	}

	cfg->pos.width = parse_uint(saveptr, "x");
	if(cfg->pos.width <= 0) {
		*errstr = log_error("Error parsing width of output configuration for "
		                    "output %s (hint: width must be larger than 0)",
		                    name);
		goto error;
	}

	cfg->pos.height = parse_uint(saveptr, " ");
	if(cfg->pos.height <= 0) {
		*errstr = log_error("Error parsing height of output configuration for "
		                    "output %s (hint: height must e larger than 0)",
		                    name);
		goto error;
	}

	char *rate_str = strtok_r(NULL, " ", saveptr);
	if(rate_str == NULL || strcmp(rate_str, "rate") != 0) {
		*errstr = log_error(
		    "Expected keyword \"rate\" in output configuration for output %s",
		    name);
		goto error;
	}

	cfg->refresh_rate = parse_float(saveptr, " ");
	if(cfg->refresh_rate <= 0.0) {
		*errstr = log_error(
		    "Error parsing refresh rate of output configuration for output %s",
		    name);
		goto error;
	}

	cfg->output_name = strdup(name);
	return cfg;
error:
	free(cfg);
	wlr_log(WLR_ERROR,
	        "Output configuration must be of the form \"output <name> pos <x> "
	        "<y> res <width>x<height> rate <refresh_rate>");
	return NULL;
}

int
parse_command(struct cg_server *server, struct keybinding *keybinding,
              char *saveptr, char **errstr) {
	char *action = strtok_r(NULL, " ", &saveptr);
	if(action == NULL) {
		*errstr = log_error("Expexted an action to parse, got none.");
		return -1;
	}
	keybinding->data = (union keybinding_params){.c = NULL};
	if(strcmp(action, "vsplit") == 0) {
		keybinding->action = KEYBINDING_SPLIT_VERTICAL;
	} else if(strcmp(action, "hsplit") == 0) {
		keybinding->action = KEYBINDING_SPLIT_HORIZONTAL;
	} else if(strcmp(action, "quit") == 0) {
		keybinding->action = KEYBINDING_QUIT;
	} else if(strcmp(action, "close") == 0) {
		keybinding->action = KEYBINDING_CLOSE_VIEW;
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
			*errstr = log_error("Not enough paramaters to \"exec\". Expected "
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
			*errstr = log_error(
			    "Expected argument for \"workspace\" action, got none.");
			return -1;
		}

		long ws = strtol(nws_str, NULL, 10);
		if(ws < 1) {
			*errstr = log_error("Workspace number must be an integer number "
			                    "larger or equal to 1. Got %ld",
			                    ws);
			return -1;
		}
		keybinding->data.u = ws - 1;
	} else if(strcmp(action, "movetoworkspace") == 0) {
		keybinding->action = KEYBINDING_MOVE_VIEW_TO_WORKSPACE;
		char *nws_str = strtok_r(NULL, " ", &saveptr);
		if(nws_str == NULL) {
			*errstr = log_error(
			    "Expected argument for \"workspace\" action, got none.");
			return -1;
		}

		long ws = strtol(nws_str, NULL, 10);
		if(ws < 1) {
			*errstr = log_error("Workspace number must be an integer larger or "
			                    "equal to 1. Got %ld",
			                    ws);
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
			*errstr = log_error(
			    "Expected argument for \"switchvt\" command, got none.");
			return -1;
		}
		long tty = strtol(ntty, NULL, 10);
		keybinding->data.u = tty;
	} else if(strcmp(action, "mode") == 0) {
		keybinding->action = KEYBINDING_SWITCH_MODE;
		char *mode = strtok_r(NULL, " ", &saveptr);
		if(mode == NULL) {
			*errstr =
			    log_error("Expected mode after \"switch_mode\". Got nothing.");
			return -1;
		}
		int mode_idx = get_mode_index_from_name(server->modes, mode);
		if(mode_idx == -1) {
			*errstr = log_error("Unknown mode \"%s\" for switch_mode", mode);
			return -1;
		}
		keybinding->data.u = (unsigned int)mode_idx;
	} else if(strcmp(action, "setmode") == 0) {
		keybinding->action = KEYBINDING_SWITCH_DEFAULT_MODE;
		char *mode = strtok_r(NULL, " ", &saveptr);
		if(mode == NULL) {
			*errstr = log_error(
			    "Expected mode after \"switch_default_mode\". Got nothing.");
			return -1;
		}
		int mode_idx = get_mode_index_from_name(server->modes, mode);
		if(mode_idx == -1) {
			*errstr =
			    log_error("Unknown mode \"%s\" for switch_default_mode", mode);
			return -1;
		}
		keybinding->data.u = (unsigned int)mode_idx;
	} else if(strcmp(action, "bind") == 0) {
		keybinding->action = KEYBINDING_DEFINEKEY;
		keybinding->data.kb = parse_bind(server, &saveptr, errstr);
		if(keybinding->data.kb == NULL) {
			return -1;
		}
	} else if(strcmp(action, "definekey") == 0) {
		keybinding->action = KEYBINDING_DEFINEKEY;
		keybinding->data.kb = parse_definekey(server, &saveptr, errstr);
		if(keybinding->data.kb == NULL) {
			return -1;
		}
	} else if(strcmp(action, "background") == 0) {
		keybinding->action = KEYBINDING_BACKGROUND;
		if(parse_background(server, keybinding->data.color, &saveptr, errstr) !=
		   0) {
			return -1;
		}
	} else if(strcmp(action, "escape") == 0) {
		keybinding->action = KEYBINDING_DEFINEKEY;
		keybinding->data.kb = parse_escape(&saveptr, errstr);
		if(keybinding->data.kb == NULL) {
			return -1;
		}
	} else if(strcmp(action, "definemode") == 0) {
		keybinding->action = KEYBINDING_DEFINEMODE;
		keybinding->data.c = parse_definemode(&saveptr, errstr);
		if(keybinding->data.c == NULL) {
			return -1;
		}
	} else if(strcmp(action, "workspaces") == 0) {
		keybinding->action = KEYBINDING_WORKSPACES;
		keybinding->data.i = parse_workspaces(&saveptr, errstr);
		if(keybinding->data.i < 0) {
			return -1;
		}
	} else if(strcmp(action, "output") == 0) {
		keybinding->action = KEYBINDING_CONFIGURE_OUTPUT;
		keybinding->data.o_cfg = parse_output_config(&saveptr, errstr);
		if(keybinding->data.o_cfg == NULL) {
			return -1;
		}
	} else {
		*errstr = log_error("Error, unsupported action \"%s\".", action);
		return -1;
	}
	return 0;
}

int
parse_rc_line(struct cg_server *server, char *line, char **errstr) {
	char *saveptr = strdup(line); // Used internally by strtok_r

	struct keybinding *keybinding = malloc(sizeof(struct keybinding));
	if(keybinding == NULL) {
		*errstr = log_error(
		    "Failed to allocate memory for temporary keybinding struct.");
		free(keybinding);
		free(saveptr);
		return -1;
	}
	if(parse_command(server, keybinding, saveptr, errstr) != 0) {
		wlr_log(WLR_ERROR, "Error parsing command.");
		free(keybinding);
		free(saveptr);
		return -1;
	}
	run_action(keybinding->action, server, keybinding->data);
	keybinding_free(keybinding, false);
	free(saveptr);
	return 0;
}
