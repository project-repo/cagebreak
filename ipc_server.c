/*
 * Cagebreak: A Wayland tiling compositor.
 *
 * Copyright (C) 2020 The Cagebreak Authors
 * Copyright (C) 2018-2020 Jente Hidskes
 *
 * See the LICENSE file accompanying this file.
 */

#define _POSIX_C_SOURCE 200112L

#include "ipc_server.h"
#include "server.h"
#include "parse.h"
#include "message.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <wlr/util/log.h>
#include <stdlib.h>

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct cg_ipc_handle *ipc = wl_container_of(listener, ipc,display_destroy);
	if (ipc->event_source != NULL) {
		wl_event_source_remove(ipc->event_source);
	}
	close(ipc->socket);
	unlink(ipc->sockaddr->sun_path);

	struct cg_ipc_client *tmp_client, *client;
	wl_list_for_each_safe(client, tmp_client, &ipc->client_list, link) {
		ipc_client_disconnect(client);
	}

	free(ipc->sockaddr);

	wl_list_remove(&ipc->display_destroy.link);
}

int ipc_init(struct cg_server *server) {
	struct cg_ipc_handle *ipc = &server->ipc;
	ipc->socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ipc->socket == -1) {
		wlr_log(WLR_ERROR, "Unable to create IPC socket");
		return -1;
	}
	if (fcntl(ipc->socket, F_SETFD, FD_CLOEXEC) == -1) {
		wlr_log(WLR_ERROR,"Unable to set CLOEXEC on IPC socket");
		return -1;
	}
	if (fcntl(ipc->socket, F_SETFL, O_NONBLOCK) == -1) {
		wlr_log(WLR_ERROR, "Unable to set NONBLOCK on IPC socket");
		return -1;
	}

	ipc->sockaddr = malloc(sizeof(struct sockaddr_un));

	if(ipc->sockaddr == NULL) {
		wlr_log(WLR_ERROR, "Unable to allocate socket address");
		return -1;
	}

	ipc->sockaddr->sun_family=AF_UNIX;
	int max_path_size = sizeof(ipc->sockaddr->sun_path);
	const char *sockdir=getenv("XDG_RUNTIME_DIR");
	if(sockdir == NULL) {
		sockdir = "/tmp";
	}

	if (max_path_size <= snprintf(ipc->sockaddr->sun_path, max_path_size,
			"%s/cagebreak-ipc.%i.%i.sock", sockdir, getuid(), getpid())) {
		wlr_log(WLR_ERROR,"Unable to write socket path to ipc->sockaddr->sun_path. Path too long");
		return -1;
	}

	unlink(ipc->sockaddr->sun_path);

	if (bind(ipc->socket, (struct sockaddr *)ipc->sockaddr, sizeof(*ipc->sockaddr)) == -1) {
		wlr_log(WLR_ERROR, "Unable to bind IPC socket");
		return -1;
	}

	if (listen(ipc->socket, 3) == -1) {
		wlr_log(WLR_ERROR, "Unable to listen on IPC socket");
	}

	setenv("CAGEBREAK_SOCKET", ipc->sockaddr->sun_path, 1);

	wl_list_init(&ipc->client_list);

	ipc->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(server->wl_display, &ipc->display_destroy);

	ipc->event_source = wl_event_loop_add_fd(server->event_loop, ipc->socket,
			WL_EVENT_READABLE, ipc_handle_connection, server);
	return 0;
}

int ipc_handle_connection(int fd, uint32_t mask, void *data) {
	(void) fd;
	struct cg_server *server = data;
	struct cg_ipc_handle *ipc = &server->ipc;
	if(mask != WL_EVENT_READABLE) {
		wlr_log(WLR_ERROR, "Expected to receive a WL_EVENT_READABLE");
		return 0;
	}

	int client_fd = accept(ipc->socket, NULL, NULL);
	if (client_fd == -1) {
		wlr_log(WLR_ERROR, "Unable to accept IPC client connection");
		return 0;
	}

	int flags;
	if ((flags = fcntl(client_fd, F_GETFD)) == -1
			|| fcntl(client_fd, F_SETFD, flags|FD_CLOEXEC) == -1) {
		wlr_log(WLR_ERROR, "Unable to set CLOEXEC on IPC client socket");
		close(client_fd);
		return 0;
	}
	if ((flags = fcntl(client_fd, F_GETFL)) == -1
			|| fcntl(client_fd, F_SETFL, flags|O_NONBLOCK) == -1) {
		wlr_log(WLR_ERROR, "Unable to set NONBLOCK on IPC client socket");
		close(client_fd);
		return 0;
	}

	struct cg_ipc_client *client = malloc(sizeof(struct cg_ipc_client));
	if (!client) {
		wlr_log(WLR_ERROR, "Unable to allocate ipc client");
		close(client_fd);
		return 0;
	}
	// +1 for \n and +1 for \0
	client->read_buffer = malloc(sizeof(char)*(MAX_LINE_SIZE+2));
	client->read_buf_len = 0;
	client->read_discard = 0;
	client->server = server;
	client->fd = client_fd;
	client->event_source = wl_event_loop_add_fd(server->event_loop,
			client_fd, WL_EVENT_READABLE, ipc_client_handle_readable, client);
	client->writable_event_source = NULL;

	client->write_buffer_size = 128;
	client->write_buffer_len = 0;
	client->write_buffer = malloc(client->write_buffer_size);
	if (!client->write_buffer) {
		wlr_log(WLR_ERROR, "Unable to allocate ipc client write buffer");
		close(client_fd);
		return 0;
	}

	wl_list_insert(&ipc->client_list, &client->link);
	return 0;
}

int ipc_client_handle_readable(int client_fd, uint32_t mask, void *data) {
	struct cg_ipc_client *client = data;

	if (mask & WL_EVENT_ERROR) {
		wlr_log(WLR_ERROR, "IPC Client socket error, removing client");
		ipc_client_disconnect(client);
		return 0;
	}

	if (mask & WL_EVENT_HANGUP) {
		ipc_client_disconnect(client);
		return 0;
	}

	int read_available;
	if (ioctl(client_fd, FIONREAD, &read_available) < 0) {
		wlr_log(WLR_ERROR, "Unable to read IPC socket buffer size");
		ipc_client_disconnect(client);
		return 0;
	}

	int read_size = read_available < MAX_LINE_SIZE+1-client->read_buf_len ? read_available:MAX_LINE_SIZE+1-client->read_buf_len;
	// Append to buffer
	ssize_t received = recv(client_fd, client->read_buffer+client->read_buf_len, read_size, 0);
	if (received == -1) {
		wlr_log(WLR_ERROR, "Unable to receive data from IPC client");
		ipc_client_disconnect(client);
		return 0;
	}
	client->read_buf_len+=received;

	ipc_client_handle_command(client);

	return 0;
}

void ipc_client_disconnect(struct cg_ipc_client *client) {
	if (client == NULL) {
		wlr_log(WLR_ERROR, "Client \"NULL\" was passed to ipc_client_disconnect");
		return;
	}

	shutdown(client->fd, SHUT_RDWR);

	wl_event_source_remove(client->event_source);
	if (client->writable_event_source) {
		wl_event_source_remove(client->writable_event_source);
	}
	wl_list_remove(&client->link);
	free(client->write_buffer);
	free(client->read_buffer);
	close(client->fd);
	free(client);
}

void ipc_client_handle_command(struct cg_ipc_client *client) {
	if (client == NULL) {
		wlr_log(WLR_ERROR, "Client \"NULL\" was passed to ipc_client_handle_command");
		return;
	}
	client->read_buffer[client->read_buf_len]='\0';
	char *nl_pos;
	uint32_t offset=0;
	while((nl_pos = strchr(client->read_buffer+offset,'\n')) != NULL){
		if(client->read_discard) {
			client->read_discard = 0;
		} else {
			*nl_pos='\0';
			char *line=client->read_buffer+offset;
			if(*line != '\0' && *line != '#') {
				message_clear(client->server->curr_output);
				char *errstr;
				if(parse_rc_line(client->server, line, &errstr) != 0) {
					if(errstr != NULL) {
						message_printf(client->server->curr_output, "%s", errstr);
						wlr_log(WLR_ERROR, "%s",errstr);
						free(errstr);
					}
					wlr_log(WLR_ERROR, "Error parsing input from IPC socket");
				}
			}
		}
		offset=(nl_pos-client->read_buffer)+1;
	}
	if(offset == 0) {
		wlr_log(WLR_ERROR, "Line received was longer that %d, discarding it",MAX_LINE_SIZE);
		client->read_buf_len = 0;
		client->read_discard = 1;
		return;
	}
	if(offset<client->read_buf_len) {
		memmove(client->read_buffer, client->read_buffer+offset, client->read_buf_len-offset);
	}
	client->read_buf_len-=offset;
}
