// Copyright 2020 - 2023, project-repo and the cagebreak contributors
// SPDX-License-Identifier: MIT

#include <wayland-server-core.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>

#include "idle_inhibit_v1.h"
#include "server.h"

struct cg_idle_inhibitor_v1 {
	struct cg_server *server;

	struct wl_list link; // server::inhibitors
	struct wl_listener destroy;
};

static void
idle_inhibit_v1_check_active(struct cg_server *server) {
	/* As of right now, this does not check whether the inhibitor
	 * is visible or not.*/
	bool inhibited = !wl_list_empty(&server->inhibitors);
	wlr_idle_notifier_v1_set_inhibited(server->idle, inhibited);
}

static void
handle_destroy(struct wl_listener *listener, void *data) {
	struct cg_idle_inhibitor_v1 *inhibitor =
	    wl_container_of(listener, inhibitor, destroy);
	struct cg_server *server = inhibitor->server;

	wl_list_remove(&inhibitor->link);
	wl_list_remove(&inhibitor->destroy.link);
	free(inhibitor);

	idle_inhibit_v1_check_active(server);
}

void
handle_idle_inhibitor_v1_new(struct wl_listener *listener, void *data) {
	struct cg_server *server =
	    wl_container_of(listener, server, new_idle_inhibitor_v1);
	struct wlr_idle_inhibitor_v1 *wlr_inhibitor = data;

	struct cg_idle_inhibitor_v1 *inhibitor =
	    calloc(1, sizeof(struct cg_idle_inhibitor_v1));
	if(!inhibitor) {
		return;
	}

	inhibitor->server = server;
	wl_list_insert(&server->inhibitors, &inhibitor->link);

	inhibitor->destroy.notify = handle_destroy;
	wl_signal_add(&wlr_inhibitor->events.destroy, &inhibitor->destroy);

	idle_inhibit_v1_check_active(server);
}
