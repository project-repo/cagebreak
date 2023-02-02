// Copyright 2020 - 2023, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#ifndef CG_FUZZ_LIB_H
#define CG_FUZZ_LIB_H

#define _POSIX_C_SOURCE 200812L

#include "../server.h"

#ifndef WAIT_ANY
#define WAIT_ANY -1
#endif

extern struct cg_server server;
extern struct wlr_xdg_shell *xdg_shell;

extern struct wlr_xwayland *xwayland;
#if CG_HAS_XWAYLAND
extern struct wlr_xcursor_manager *xcursor_manager;
#endif

void
cleanup();

int
LLVMFuzzerInitialize(int *argc, char ***argv);

void
move_cursor(char *line, struct cg_server *server);

void
create_output(char *line, struct cg_server *server);

void
create_input_device(char *line, struct cg_server *server);

void
destroy_input_device(char *line, struct cg_server *server);

void
destroy_output(char *line, struct cg_server *server);

#endif
