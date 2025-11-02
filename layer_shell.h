// Copyright 2020 - 2025, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#ifndef CG_LAYER_SHELL_H
#define CG_LAYER_SHELL_H

struct cg_server;

void cg_layer_shell_init(struct cg_server *server);
void cg_layer_shell_destroy(struct cg_server *server);

#endif // CG_LAYER_SHELL_H