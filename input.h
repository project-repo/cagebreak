// Copyright 2020 - 2025, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#ifndef _CG_INPUT_H
#define _CG_INPUT_H

#include <stdbool.h>

struct cg_input_device;
struct cg_input_config;
struct cg_server;

void
cg_input_configure_libinput_device(struct cg_input_device *device);

void
cg_input_apply_config(struct cg_input_config *config, struct cg_server *server);

bool
cg_libinput_device_is_builtin(struct cg_input_device *device);

#endif
