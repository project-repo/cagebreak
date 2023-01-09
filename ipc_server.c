// Copyright 2020 - 2023, project-repo and the cagebreak contributors
// SPDX -License-Identifier: MIT

#define _DEFAULT_SOURCE

#include "ipc_server.h"
#include "message.h"
#include "parse.h"
#include "server.h"
#include "util.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <wlr/util/log.h>

static const char ipc_magic[] = {'c', 'g', '-', 'i', 'p', 'c'};

#define IPC_HEADER_SIZE sizeof(ipc_magic)

static void
handle_display_destroy(struct wl_listener *listener, void *data) {
	struct cg_ipc_handle *ipc = wl_container_of(listener, ipc, display_destroy);
	if(ipc->event_source != NULL) {
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

int
ipc_init(struct cg_server *server) {
	if(server->enable_socket == false) {
		return 0;
	}
	struct cg_ipc_handle *ipc = &server->ipc;
	ipc->socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if(ipc->socket == -1) {
		wlr_log(WLR_ERROR, "Unable to create IPC socket");
		return -1;
	}
	if(fcntl(ipc->socket, F_SETFD, FD_CLOEXEC) == -1) {
		wlr_log(WLR_ERROR, "Unable to set CLOEXEC on IPC socket");
		return -1;
	}
	if(fcntl(ipc->socket, F_SETFL, O_NONBLOCK) == -1) {
		wlr_log(WLR_ERROR, "Unable to set NONBLOCK on IPC socket");
		return -1;
	}

	ipc->sockaddr = malloc(sizeof(struct sockaddr_un));

	if(ipc->sockaddr == NULL) {
		wlr_log(WLR_ERROR, "Unable to allocate socket address");
		return -1;
	}

	ipc->sockaddr->sun_family = AF_UNIX;
	int max_path_size = sizeof(ipc->sockaddr->sun_path);
	const char *sockdir = getenv("XDG_RUNTIME_DIR");
	if(sockdir == NULL) {
		sockdir = "/tmp";
	}

	if(max_path_size <= snprintf(ipc->sockaddr->sun_path, max_path_size,
	                             "%s/cagebreak-ipc.%i.%i.sock", sockdir,
	                             getuid(), getpid())) {
		wlr_log(WLR_ERROR, "Unable to write socket path to "
		                   "ipc->sockaddr->sun_path. Path too long");
		free(ipc->sockaddr);
		return -1;
	}

	unlink(ipc->sockaddr->sun_path);

	if(bind(ipc->socket, (struct sockaddr *)ipc->sockaddr,
	        sizeof(*ipc->sockaddr)) == -1) {
		wlr_log(WLR_ERROR, "Unable to bind IPC socket");
		free(ipc->sockaddr);
		return -1;
	}

	if(listen(ipc->socket, 3) == -1) {
		wlr_log(WLR_ERROR, "Unable to listen on IPC socket");
		free(ipc->sockaddr);
		return -1;
	}

	chmod(ipc->sockaddr->sun_path, 0700);
	setenv("CAGEBREAK_SOCKET", ipc->sockaddr->sun_path, 1);

	wl_list_init(&ipc->client_list);

	ipc->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(server->wl_display, &ipc->display_destroy);

	ipc->event_source =
	    wl_event_loop_add_fd(server->event_loop, ipc->socket, WL_EVENT_READABLE,
	                         ipc_handle_connection, server);
	return 0;
}

int
ipc_client_handle_writable(int client_fd, uint32_t mask, void *data) {
	struct cg_ipc_client *client = data;

	if(mask & WL_EVENT_ERROR) {
		ipc_client_disconnect(client);
		return 0;
	}

	if(mask & WL_EVENT_HANGUP) {
		ipc_client_disconnect(client);
		return 0;
	}

	if(client->write_buffer_len <= 0) {
		return 0;
	}

	if(fcntl(client->fd, F_GETFD) == -1) {
		return 0;
	}
	ssize_t written =
	    write(client->fd, client->write_buffer, client->write_buffer_len);

	if(written == -1 && errno == EAGAIN) {
		return 0;
	} else if(written == -1) {
		wlr_log(WLR_ERROR, "Unable to send data from queue to IPC client");
		ipc_client_disconnect(client);
		return 0;
	}

	memmove(client->write_buffer, client->write_buffer + written,
	        client->write_buffer_len - written);
	client->write_buffer_len -= written;

	if(client->write_buffer_len == 0 && client->writable_event_source) {
		wl_event_source_remove(client->writable_event_source);
		client->writable_event_source = NULL;
	}

	return 0;
}

int
ipc_handle_connection(int fd, uint32_t mask, void *data) {
	(void)fd;
	struct cg_server *server = data;
	struct cg_ipc_handle *ipc = &server->ipc;
	if(mask != WL_EVENT_READABLE) {
		wlr_log(WLR_ERROR, "Expected to receive a WL_EVENT_READABLE");
		return 0;
	}

	int client_fd = accept(ipc->socket, NULL, NULL);
	if(client_fd == -1) {
		wlr_log(WLR_ERROR, "Unable to accept IPC client connection");
		return 0;
	}

	int flags;
	if((flags = fcntl(client_fd, F_GETFD)) == -1 ||
	   fcntl(client_fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
		wlr_log(WLR_ERROR, "Unable to set CLOEXEC on IPC client socket");
		close(client_fd);
		return 0;
	}
	if((flags = fcntl(client_fd, F_GETFL)) == -1 ||
	   fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		wlr_log(WLR_ERROR, "Unable to set NONBLOCK on IPC client socket");
		close(client_fd);
		return 0;
	}

	struct cg_ipc_client *client = malloc(sizeof(struct cg_ipc_client));
	if(!client) {
		wlr_log(WLR_ERROR, "Unable to allocate ipc client");
		close(client_fd);
		return 0;
	}
	client->read_buf_cap = 64;
	client->read_buffer = calloc(client->read_buf_cap, sizeof(char));
	client->read_buf_len = 0;
	client->read_discard = 0;
	client->server = server;
	client->fd = client_fd;
	client->event_source =
	    wl_event_loop_add_fd(server->event_loop, client_fd, WL_EVENT_READABLE,
	                         ipc_client_handle_readable, client);
	client->writable_event_source =
	    wl_event_loop_add_fd(server->event_loop, client_fd, WL_EVENT_WRITABLE,
	                         ipc_client_handle_writable, client);

	client->write_buffer_size = 128;
	client->write_buffer_len = 0;
	client->write_buffer = malloc(client->write_buffer_size);
	if(!client->write_buffer) {
		wlr_log(WLR_ERROR, "Unable to allocate ipc client write buffer");
		close(client_fd);
		return 0;
	}

	wl_list_insert(&ipc->client_list, &client->link);
	return 0;
}

int
ipc_client_handle_readable(int client_fd, uint32_t mask, void *data) {
	struct cg_ipc_client *client = data;

	if(mask & WL_EVENT_ERROR) {
		wlr_log(WLR_ERROR, "IPC Client socket error, removing client");
		ipc_client_disconnect(client);
		return 0;
	}

	if(mask & WL_EVENT_HANGUP) {
		ipc_client_disconnect(client);
		return 0;
	}

	int read_available;
	if(ioctl(client_fd, FIONREAD, &read_available) < 0) {
		wlr_log(WLR_ERROR, "Unable to read IPC socket buffer size");
		ipc_client_disconnect(client);
		return 0;
	}

	while(read_available + client->read_buf_len >
	      (int32_t)client->read_buf_cap - 1) {
		client->read_buf_cap *= 2;
		client->read_buffer = reallocarray(client->read_buffer,
		                                   client->read_buf_cap, sizeof(char));
		if(client->read_buffer == NULL) {
			wlr_log(WLR_ERROR, "Unable to allocate buffer large enough to hold "
			                   "client read data");
			ipc_client_disconnect(client);
			return 0;
		}
	}
	// Append to buffer
	ssize_t received =
	    recv(client_fd, client->read_buffer + client->read_buf_len,
	         read_available, 0);
	if(received == -1) {
		wlr_log(WLR_ERROR, "Unable to receive data from IPC client");
		ipc_client_disconnect(client);
		return 0;
	}
	client->read_buf_len += received;

	ipc_client_handle_command(client);

	return 0;
}

void
ipc_client_disconnect(struct cg_ipc_client *client) {
	if(client == NULL) {
		wlr_log(WLR_ERROR,
		        "Client \"NULL\" was passed to ipc_client_disconnect");
		return;
	}

	shutdown(client->fd, SHUT_RDWR);

	wl_event_source_remove(client->event_source);
	if(client->writable_event_source) {
		wl_event_source_remove(client->writable_event_source);
	}
	wl_list_remove(&client->link);
	if(client->write_buffer != NULL) {
		free(client->write_buffer);
	}
	if(client->read_buffer != NULL) {
		free(client->read_buffer);
	}
	close(client->fd);
	free(client);
}

void
ipc_client_handle_command(struct cg_ipc_client *client) {
	if(client == NULL) {
		wlr_log(WLR_ERROR,
		        "Client \"NULL\" was passed to ipc_client_handle_command");
		return;
	}
	client->read_buffer[client->read_buf_len] = '\0';
	char *nl_pos;
	uint32_t offset = 0;
	while((nl_pos = strchr(client->read_buffer + offset, '\n')) != NULL) {
		if(client->read_discard) {
			client->read_discard = 0;
		} else {
			*nl_pos = '\0';
			char *line = client->read_buffer + offset;
			if(*line != '\0' && *line != '#') {
				message_clear(client->server->curr_output);
				char *errstr;
				if(parse_rc_line(client->server, line, &errstr) != 0) {
					if(errstr != NULL) {
						message_printf(client->server->curr_output, "%s",
						               errstr);
						wlr_log(WLR_ERROR, "%s", errstr);
						free(errstr);
					}
					wlr_log(WLR_ERROR, "Error parsing input from IPC socket");
				}
			}
		}
		offset = (nl_pos - client->read_buffer) + 1;
	}
	if(offset < client->read_buf_len) {
		memmove(client->read_buffer, client->read_buffer + offset,
		        client->read_buf_len - offset);
	}
	client->read_buf_len -= offset;
}

void
ipc_send_event_client(struct cg_ipc_client *client, const char *payload,
                      uint32_t payload_length) {
	char data[IPC_HEADER_SIZE];

	memcpy(data, ipc_magic, sizeof(ipc_magic));

	// +1 for terminating null character
	while(client->write_buffer_len + IPC_HEADER_SIZE + payload_length + 1 >=
	      client->write_buffer_size) {
		client->write_buffer_size *= 2;
	}

	if(client->write_buffer_size > 4e6) { // 4 MB
		wlr_log(WLR_ERROR,
		        "Client write buffer too big (%zu), disconnecting client",
		        client->write_buffer_size);
		ipc_client_disconnect(client);
		return;
	}

	char *new_buffer = realloc(client->write_buffer, client->write_buffer_size);
	if(!new_buffer) {
		wlr_log(WLR_ERROR, "Unable to reallocate ipc client write buffer");
		ipc_client_disconnect(client);
		return;
	}
	client->write_buffer = new_buffer;

	memcpy(client->write_buffer + client->write_buffer_len, data,
	       IPC_HEADER_SIZE);
	client->write_buffer_len += IPC_HEADER_SIZE;
	memcpy(client->write_buffer + client->write_buffer_len, payload,
	       payload_length);
	client->write_buffer_len += payload_length;
	memcpy(client->write_buffer + client->write_buffer_len, "\0", 1);
	client->write_buffer_len += 1;
}

void
ipc_send_event(struct cg_server *server, const char *fmt, ...) {
	if(server->enable_socket == false) {
		return;
	}
	if(wl_list_empty(&server->ipc.client_list)) {
		return;
	}
	va_list args;
	va_start(args, fmt);
	char *msg = malloc_vsprintf_va_list(fmt, args);
	if(msg == NULL) {
		wlr_log(WLR_ERROR, "Unable to allocate memory for ipc event");
		va_end(args);
		return;
	}
	va_end(args);
	struct cg_ipc_client *it, *tmp;
	uint32_t len = strlen(msg);
	wl_list_for_each_safe(it, tmp, &server->ipc.client_list, link) {
		if(it->writable_event_source == NULL) {
			it->writable_event_source = wl_event_loop_add_fd(
			    server->event_loop, it->fd, WL_EVENT_WRITABLE,
			    ipc_client_handle_writable, it);
		}
		ipc_send_event_client(it, msg, len);
	}
	free(msg);
}
