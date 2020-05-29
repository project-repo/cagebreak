/*
 * Cagebreak: A Wayland tiling compositor.
 *
 * Copyright (C) 2018-2020 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#ifndef CG_FUZZ_LIB_H
#define CG_FUZZ_LIB_H

#define _POSIX_C_SOURCE 200812L

#include "../server.h"

#ifndef WAIT_ANY
#define WAIT_ANY -1
#endif

struct cg_server server;
struct wlr_xdg_shell *xdg_shell;

struct wlr_xwayland *xwayland;
#if CG_HAS_XWAYLAND
struct wlr_xcursor_manager *xcursor_manager;
#endif

void cleanup();

int LLVMFuzzerInitialize(int *argc, char ***argv);

void move_cursor(char *line, struct cg_server *server);

void create_output(char *line, struct cg_server *server);

void create_input_device(char *line, struct cg_server *server);

void destroy_input_device(char *line, struct cg_server *server);

void destroy_output(char *line, struct cg_server *server);

#endif
