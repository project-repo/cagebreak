// Copyright 2020 - 2025, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#ifndef PARSE_H

#define PARSE_H

#include <stdio.h>

struct cg_server;

int
parse_rc_line(struct cg_server *server, char *line, char **errstr);
char *
parse_malloc_vsprintf(const char *fmt, ...);
char *
parse_malloc_vsprintf_va_list(const char *fmt, va_list list);

#endif /* end of include guard PARSE_H */
