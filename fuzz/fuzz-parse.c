#define _POSIX_C_SOURCE 200812L
#define FUZZING

#include "../keybinding.h"
#include "../parse.h"
#include "../seat.h"
#include "../server.h"
#include "../output.h"
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wayland-server-core.h>

#ifndef WAIT_ANY
#define WAIT_ANY -1
#endif

void
set_sig_handler(int sig) {
	struct sigaction sa;
	sa.sa_handler = SIG_IGN; // handle signal by ignoring
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if(sigaction(SIGCHLD, &sa, 0) == -1) {
		perror(0);
		exit(1);
	}
}

struct cg_server server;

int
LLVMFuzzerInitialize(int *argc, char ***argv) {
	set_sig_handler(SIGCHLD);

	server.wl_display = NULL;
	server.event_loop = NULL;

	server.seat = malloc(sizeof(struct cg_seat));
	server.seat->mode = 0;
	server.seat->default_mode = 0;

	server.idle = NULL;
	server.idle_inhibit_v1 = NULL;
	/* new_idle_inhibitor_v1 */
	wl_list_init(&server.inhibitors);

	server.output_layout = NULL;
	wl_list_init(&server.outputs);
	server.curr_output = NULL;

	/* new_output */

	/* xdg_toplevel_decoration */
	/* new_xdg_shell_surface */
	/* new_xwayland_surface */
	server.keybindings = keybinding_list_init();
	server.output_transform = 0;
	wl_list_init(&server.output_config);

	server.running = true;
	server.modes = malloc(sizeof(char *));
	server.modes[0] = NULL;
	server.nws = 1;
	server.message_timeout = 0;
	server.bg_color = malloc(4 * sizeof(float));
	server.bg_color[0] = 0;
	server.bg_color[1] = 0;
	server.bg_color[2] = 0;
	server.bg_color[3] = 1;

	return 0;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	char *str = malloc(size * sizeof(char) + 1);
	strncpy(str, (char *)data, size);
	str[size] = 0;
	parse_rc_line(&server, str);
	free(str);
	keybinding_list_free(server.keybindings);
	server.keybindings = keybinding_list_init();

	struct cg_output_config *output_config, *output_config_tmp;
	wl_list_for_each_safe(output_config, output_config_tmp, &server.output_config, link) {
		wl_list_remove(&output_config->link);
		free(output_config->output_name);
		free(output_config);
	}

	return 0;
}
