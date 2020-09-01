#ifndef CG_IDLE_INHIBIT_H
#define CG_IDLE_INHIBIT_H

struct wl_listener;

void
handle_idle_inhibitor_v1_new(struct wl_listener *listener, void *data);

#endif
