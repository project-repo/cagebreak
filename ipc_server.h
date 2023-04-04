// Copyright 2020 - 2023, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#ifndef CG_IPC_SERVER_H
#define CG_IPC_SERVER_H

#include "config.h"

#include <stdint.h>
#include <stdlib.h>
#include <wayland-server-core.h>

struct cg_server;

struct cg_ipc_client {
	struct wl_event_source *event_source;
	struct wl_event_source *writable_event_source;
	struct cg_server *server;
	struct wl_list link;
	int fd;
	uint32_t security_policy;
	size_t write_buffer_len;
	size_t write_buffer_size;
	char *write_buffer;
	// The following is for storing data between event_loop calls
	uint16_t read_buf_len;
	size_t read_buf_cap;
	uint8_t read_discard; // 1 if the current line is to be discarded
	char *read_buffer;
};

struct cg_ipc_handle {
	int socket;
	struct wl_event_source *event_source;
	struct wl_list client_list;
	struct wl_listener display_destroy;
	struct sockaddr_un *sockaddr;
};

void
ipc_send_event(struct cg_server *server, const char *fmt, ...);
int
ipc_init(struct cg_server *server);
int
ipc_handle_connection(int fd, uint32_t mask, void *data);
int
ipc_client_handle_readable(int client_fd, uint32_t mask, void *data);
// int ipc_client_handle_writable(int client_fd, uint32_t mask, void *data);
void
ipc_client_disconnect(struct cg_ipc_client *client);
void
ipc_client_handle_command(struct cg_ipc_client *client);
// bool ipc_send_reply(struct ipc_client *client, const char *payload, uint32_t
// payload_length);

#endif
