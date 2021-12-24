#define _POSIX_C_SOURCE 200812L

#include <float.h>
#include <libinput.h>
#include <limits.h>
#include <string.h>
#include <wlr/util/log.h>

#include "input_manager.h"
#include "keybinding.h"
#include "output.h"
#include "parse.h"
#include "server.h"
#include "util.h"

char *
log_error(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	char *ret = malloc_vsprintf_va_list(fmt, args);
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

float
parse_float(char **saveptr, const char *delim) {
	char *uint_str = strtok_r(NULL, delim, saveptr);
	if(uint_str == NULL) {
		wlr_log(WLR_ERROR, "Expected a float, got nothing");
		return -1;
	}
	float ufloat = strtof(uint_str, NULL);
	if(ufloat != NAN && ufloat != INFINITY && errno != ERANGE) {
		return ufloat;
	} else {
		wlr_log(WLR_ERROR, "Error parsing float");
		return FLT_MIN;
	}
}

struct cg_input_config *
parse_input_config(char **saveptr, char **errstr) {
	struct cg_input_config *cfg = calloc(1, sizeof(struct cg_input_config));
	char *value = NULL;
	char *ident = NULL;
	if(cfg == NULL) {
		*errstr =
		    log_error("Failed to allocate memory for input configuration");
		goto error;
	}

	cfg->tap = INT_MIN;
	cfg->tap_button_map = INT_MIN;
	cfg->drag = INT_MIN;
	cfg->drag_lock = INT_MIN;
	cfg->dwt = INT_MIN;
	cfg->send_events = INT_MIN;
	cfg->click_method = INT_MIN;
	cfg->middle_emulation = INT_MIN;
	cfg->natural_scroll = INT_MIN;
	cfg->accel_profile = INT_MIN;
	cfg->pointer_accel = FLT_MIN;
	cfg->scroll_factor = FLT_MIN;
	cfg->scroll_button = INT_MIN;
	cfg->scroll_method = INT_MIN;
	cfg->left_handed = INT_MIN;
	/*cfg->repeat_delay = INT_MIN;
	cfg->repeat_rate = INT_MIN;
	cfg->xkb_numlock = INT_MIN;
	cfg->xkb_capslock = INT_MIN;
	cfg->xkb_file_is_set = false;
	wl_list_init(&cfg->tools);*/

	ident = strtok_r(NULL, " ", saveptr);

	if(ident == NULL) {
		*errstr = log_error(
		    "Expected identifier of input device to configure, got none");
		goto error;
	}
	cfg->identifier = strdup(ident);

	char *setting = strtok_r(NULL, " ", saveptr);
	if(setting == NULL) {
		*errstr =
		    log_error("Expected setting to be set on input device, got none");
		goto error;
	}

	value = strdup(*saveptr);

	if(value == NULL) {
		*errstr = log_error("Failed to obtain value for input device "
		                    "configuration of device \"%s\"",
		                    ident);
		goto error;
	}

	if(strcmp(setting, "accel_profile") == 0) {
		if(strcmp(value, "adaptive") == 0) {
			cfg->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
		} else if(strcmp(value, "flat") == 0) {
			cfg->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
		} else {
			*errstr = log_error(
			    "Invalid profile \"%s\" for accel_profile configuration",
			    value);
			goto error;
		}
	} else if(strcmp(setting, "calibration_matrix") == 0) {
		cfg->calibration_matrix.configured = true;
		for(int i = 0; i < 6; ++i) {
			cfg->calibration_matrix.matrix[i] = parse_float(saveptr, " ");
			if(cfg->calibration_matrix.matrix[i] == FLT_MIN) {
				*errstr =
				    log_error("Failed to read calibration matrix, expected 6 "
				              "floating point values separated by spaces");
				goto error;
			}
		}
	} else if(strcmp(setting, "click_method") == 0) {
		if(strcmp(value, "none")) {
			cfg->click_method = LIBINPUT_CONFIG_CLICK_METHOD_NONE;
		} else if(strcmp(value, "button_areas")) {
			cfg->click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
		} else if(strcmp(value, "clickfinger")) {
			cfg->click_method = LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
		} else {
			*errstr = log_error(
			    "Invalid method \"%s\" for click_method configuration", value);
			goto error;
		}
	} else if(strcmp(setting, "drag") == 0) {
		if(strcmp(value, "enabled") == 0) {
			cfg->drag = LIBINPUT_CONFIG_DRAG_ENABLED;
		} else if(strcmp(value, "disabled") == 0) {
			cfg->drag = LIBINPUT_CONFIG_DRAG_DISABLED;
		} else {
			*errstr =
			    log_error("Invalid option \"%s\" to setting \"drag\"", value);
			goto error;
		}
	} else if(strcmp(setting, "drag_lock") == 0) {
		if(strcmp(value, "enabled") == 0) {
			cfg->drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_ENABLED;
		} else if(strcmp(value, "disabled") == 0) {
			cfg->drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_DISABLED;
		} else {
			*errstr = log_error(
			    "Invalid option \"%s\" to setting \"drag_lock\"", value);
			goto error;
		}
	} else if(strcmp(setting, "dwt") == 0) {
		if(strcmp(value, "enabled") == 0) {
			cfg->dwt = LIBINPUT_CONFIG_DWT_ENABLED;
		} else if(strcmp(value, "disabled") == 0) {
			cfg->dwt = LIBINPUT_CONFIG_DWT_DISABLED;
		} else {
			*errstr =
			    log_error("Invalid option \"%s\" to setting \"dwt\"", value);
			goto error;
		}
	} else if(strcmp(setting, "events") == 0) {
		if(strcmp(value, "enabled") == 0) {
			cfg->send_events = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
		} else if(strcmp(value, "disabled") == 0) {
			cfg->send_events = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
		} else if(strcmp(value, "disabled_on_external_mouse") == 0) {
			cfg->send_events =
			    LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
		} else {
			*errstr =
			    log_error("Invalid option \"%s\" to setting \"events\"", value);
			goto error;
		}
	} else if(strcmp(setting, "left_handed") == 0) {
		if(strcmp(value, "enabled") == 0) {
			cfg->left_handed = true;
		} else if(strcmp(value, "disabled") == 0) {
			cfg->left_handed = false;
		} else {
			*errstr = log_error(
			    "Invalid option \"%s\" to setting \"left_handed\"", value);
			goto error;
		}
	} else if(strcmp(setting, "middle_emulation") == 0) {
		if(strcmp(value, "enabled") == 0) {
			cfg->middle_emulation = LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED;
		} else if(strcmp(value, "disabled") == 0) {
			cfg->middle_emulation = LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED;
		} else {
			*errstr = log_error(
			    "Invalid option \"%s\" to setting \"middle_emulation\"", value);
			goto error;
		}
	} else if(strcmp(setting, "natural_scroll") == 0) {
		if(strcmp(value, "enabled") == 0) {
			cfg->natural_scroll = true;
		} else if(strcmp(value, "disabled") == 0) {
			cfg->natural_scroll = false;
		} else {
			*errstr = log_error(
			    "Invalid option \"%s\" to setting \"natural_scroll\"", value);
			goto error;
		}
	} else if(strcmp(setting, "pointer_accel") == 0) {
		cfg->pointer_accel = parse_float(saveptr, " ");
		if(cfg->pointer_accel == FLT_MIN) {
			*errstr = log_error("Invalid option \"%s\" to setting "
			                    "\"pointer_accel\", expected float value",
			                    value);
			goto error;
		}
	} else if(strcmp(setting, "scroll_button") == 0) {
		char *err = NULL;
		cfg->scroll_button = input_manager_get_mouse_button(value, &err);
		if(err) {
			*errstr = log_error("Error parsing button for \"scroll_button\" "
			                    "setting. Returned error \"%s\"",
			                    err);
			free(err);
			goto error;
		}
	} else if(strcmp(setting, "scroll_factor") == 0) {
		cfg->scroll_factor = parse_float(saveptr, " ");
		if(cfg->scroll_factor == FLT_MIN) {
			*errstr = log_error("Invalid option \"%s\" to setting "
			                    "\"scroll_factor\", expected float value",
			                    value);
			goto error;
		}
	} else if(strcmp(setting, "scroll_method") == 0) {
		if(strcmp(value, "none") == 0) {
			cfg->scroll_method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
		} else if(strcmp(value, "two_finger") == 0) {
			cfg->scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
		} else if(strcmp(value, "edge") == 0) {
			cfg->scroll_method = LIBINPUT_CONFIG_SCROLL_EDGE;
		} else if(strcmp(value, "on_button_down") == 0) {
			cfg->scroll_method = LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
		} else {
			*errstr = log_error(
			    "Invalid option \"%s\" to setting \"scroll_method\"", value);
			goto error;
		}
	} else if(strcmp(setting, "tap") == 0) {
		if(strcmp(value, "enabled") == 0) {
			cfg->tap = LIBINPUT_CONFIG_TAP_ENABLED;
		} else if(strcmp(value, "disabled") == 0) {
			cfg->tap = LIBINPUT_CONFIG_TAP_DISABLED;
		} else {
			*errstr =
			    log_error("Invalid option \"%s\" to setting \"tap\"", value);
			goto error;
		}
	} else if(strcmp(setting, "tap_button_map") == 0) {
		if(strcmp(value, "lrm") == 0) {
			cfg->tap = LIBINPUT_CONFIG_TAP_MAP_LRM;
		} else if(strcmp(value, "lmr") == 0) {
			cfg->tap = LIBINPUT_CONFIG_TAP_MAP_LMR;
		} else {
			*errstr = log_error(
			    "Invalid option \"%s\" to setting \"tap_button_map\"", value);
			goto error;
		}
	} else {
		*errstr = log_error("Invalid option to command \"input\"");
		goto error;
	}

	free(value);
	return cfg;

error:
	if(cfg) {
		if(cfg->identifier) {
			free(cfg->identifier);
		}
		free(cfg);
	}
	if(value) {
		free(value);
	}
	wlr_log(WLR_ERROR, "Input configuration must be of the form 'input "
	                   "\"<ident>\" <setting> <value>'");
	return NULL;
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

int
parse_output_config_keyword(char *key_str, enum output_status *status) {
	if(key_str == NULL) {
		return -1;
	}
	if(strcmp(key_str, "pos") == 0) {
		*status = OUTPUT_DEFAULT;
	} else if(strcmp(key_str, "prio") == 0) {
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
	cfg->status = OUTPUT_DEFAULT;
	cfg->pos.x = -1;
	cfg->output_name = NULL;
	cfg->refresh_rate = 0;
	cfg->priority = -1;
	char *name = strtok_r(NULL, " ", saveptr);
	if(name == NULL) {
		*errstr =
		    log_error("Expected name of output to be configured, got none");
		goto error;
	}
	char *key_str = strtok_r(NULL, " ", saveptr);
	if(parse_output_config_keyword(key_str, &(cfg->status)) != 0) {
		*errstr = log_error("Expected keyword \"pos\", \"prio\", \"enable\" or "
		                    "\"disable\" in output configuration for output %s",
		                    name);
		goto error;
	}
	if(cfg->status == OUTPUT_ENABLE || cfg->status == OUTPUT_DISABLE) {
		cfg->output_name = strdup(name);
		return cfg;
	}

	if(strcmp(key_str, "prio") == 0) {
		cfg->priority = parse_uint(saveptr, " ");
		if(cfg->priority < 0) {
			*errstr = log_error(
			    "Error parsing priority of output configuration for output %s",
			    name);
			goto error;
		}
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
		*errstr =
		    log_error("Error parsing refresh rate of output configuration for "
		              "output %s, expected positive float",
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

struct cg_message_config *parse_message_config(char **saveptr, char **errstr) {
	struct cg_message_config *cfg = calloc(1, sizeof(struct cg_message_config));
	if(cfg == NULL) {
		*errstr =
		    log_error("Failed to allocate memory for message configuration");
		goto error;
	}

	cfg->bg_color[0]=-1;
	cfg->fg_color[0]=-1;
	cfg->display_time=-1;
	cfg->font=NULL;

	char *setting = strtok_r(NULL, " ", saveptr);
	if(setting == NULL) {
		*errstr =
		    log_error("Expected setting to be set for message configuration, got none");
		goto error;
	}

	if(strcmp(setting, "font") == 0) {
		cfg->font=strdup(*saveptr);
		if(cfg->font == NULL) {
			*errstr = log_error( "Unable to allocate memory for font descrition in command \"message\"");
			goto error;
		}
	} else if(strcmp(setting, "display_time") == 0) {
		cfg->display_time=parse_uint(saveptr, " ");
		if(cfg->display_time<0) {
			*errstr = log_error("Error parsing command \"configure_message display_time\", expected a non-negative integer");
			goto error;
		}
	} else if(strcmp(setting, "bg_color") == 0) {
		for(int i = 0; i < 4; ++i) {
			cfg->bg_color[i]=parse_float(saveptr, " ");
			if(cfg->bg_color[i] == FLT_MIN) {
				*errstr =
				    log_error("Error parsing command \"configure_message bg_color\", expected 4 float values separated by spaces");
				goto error;
			}
		}
	} else if(strcmp(setting, "fg_color") == 0) {
		for(int i = 0; i < 4; ++i) {
			cfg->fg_color[i]=parse_float(saveptr, " ");
			if(cfg->fg_color[i] == FLT_MIN) {
				*errstr =
				    log_error("Error parsing command \"configure_message bg_color\", expected 4 float values separated by spaces");
				goto error;
			}
		}
	} else {
		*errstr = log_error("Invalid option to command \"configure_message\"");
		goto error;
	}
	return cfg;

error:
	wlr_log(WLR_ERROR, "Message configuration must be of the form 'configure_message <setting> <value>'");
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
	} else if(strcmp(action, "show_info") == 0) {
		keybinding->action = KEYBINDING_SHOW_INFO;
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
	} else if(strcmp(action, "screen") == 0) {
		keybinding->action = KEYBINDING_SWITCH_OUTPUT;
		char *noutp_str = strtok_r(NULL, " ", &saveptr);
		if(noutp_str == NULL) {
			*errstr =
			    log_error("Expected argument for \"output\" action, got none.");
			return -1;
		}

		long outp = strtol(noutp_str, NULL, 10);
		if(outp < 1) {
			*errstr = log_error("Workspace number must be an integer number "
			                    "larger or equal to 1. Got %ld",
			                    outp);
			return -1;
		}
		keybinding->data.u = outp;
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
	} else if(strcmp(action, "movetoscreen") == 0) {
		keybinding->action = KEYBINDING_MOVE_VIEW_TO_OUTPUT;
		char *noutp_str = strtok_r(NULL, " ", &saveptr);
		if(noutp_str == NULL) {
			*errstr = log_error(
			    "Expected argument for \"movetoscreen\" action, got none.");
			return -1;
		}

		long outp = strtol(noutp_str, NULL, 10);
		if(outp < 1) {
			*errstr = log_error("Output number must be an integer larger or "
			                    "equal to 1. Got %ld",
			                    outp);
			return -1;
		}
		keybinding->data.u = outp;
	} else if(strcmp(action, "movetoworkspace") == 0) {
		keybinding->action = KEYBINDING_MOVE_VIEW_TO_WORKSPACE;
		char *nws_str = strtok_r(NULL, " ", &saveptr);
		if(nws_str == NULL) {
			*errstr = log_error(
			    "Expected argument for \"movetoworkspace\" action, got none.");
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
		keybinding->action = KEYBINDING_MOVE_VIEW_TO_CYCLE_OUTPUT;
		keybinding->data.b = false;
	} else if(strcmp(action, "movetoprevscreen") == 0) {
		keybinding->action = KEYBINDING_MOVE_VIEW_TO_CYCLE_OUTPUT;
		keybinding->data.b = true;
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
	} else if(strcmp(action, "input") == 0) {
		keybinding->action = KEYBINDING_CONFIGURE_INPUT;
		keybinding->data.i_cfg = parse_input_config(&saveptr, errstr);
		if(keybinding->data.i_cfg == NULL) {
			return -1;
		}
	} else if(strcmp(action, "configure_message") == 0) {
		keybinding->action = KEYBINDING_CONFIGURE_MESSAGE;
		keybinding->data.m_cfg = parse_message_config(&saveptr, errstr);
		if(keybinding->data.m_cfg == NULL) {
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
