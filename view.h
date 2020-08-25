#ifndef CG_VIEW_H
#define CG_VIEW_H

#include "config.h"

#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_surface.h>

struct cg_server;
struct wlr_box;
struct wlr_surface;

enum cg_view_type {
	CG_XDG_SHELL_VIEW,
#if CG_HAS_XWAYLAND
	CG_XWAYLAND_VIEW,
#endif
};

struct cg_view {
	struct cg_workspace *workspace;
	struct wl_list link;     // server::views
	struct wl_list children; // cg_view_child::link
	struct wlr_surface *wlr_surface;

	/* The view has a position in output coordinates. */
	int ox, oy;

	enum cg_view_type type;
	const struct cg_view_impl *impl;

	struct wl_listener new_subsurface;
};

struct cg_view_impl {
	char *(*get_title)(const struct cg_view *view);
	void (*get_geometry)(const struct cg_view *view, int *width_out,
	                     int *height_out);
	bool (*is_primary)(const struct cg_view *view);
	void (*activate)(struct cg_view *view, bool activate);
	void (*close)(struct cg_view *view);
	void (*maximize)(struct cg_view *view, int width, int height);
	void (*destroy)(struct cg_view *view);
	void (*for_each_surface)(struct cg_view *view,
	                         wlr_surface_iterator_func_t iterator, void *data);
	void (*for_each_popup)(struct cg_view *view,
	                       wlr_surface_iterator_func_t iterator, void *data);
	struct wlr_surface *(*wlr_surface_at)(const struct cg_view *view, double sx,
	                                      double sy, double *sub_x,
	                                      double *sub_y);
};

struct cg_view_child {
	struct cg_view *view;
	struct cg_view_child *parent;
	struct wlr_surface *wlr_surface;
	struct wl_list link;

	struct wl_listener commit;
	struct wl_listener new_subsurface;

	void (*destroy)(struct cg_view_child *child);
	void (*get_coords)(struct cg_view_child *child, int *x, int *y);
};

struct cg_subsurface {
	struct cg_view_child view_child;
	struct wlr_subsurface *wlr_subsurface;

	struct wl_listener destroy;
};

char *
view_get_title(const struct cg_view *view);
struct cg_tile *
view_get_tile(const struct cg_view *view);
bool
view_is_primary(const struct cg_view *view);
bool
view_is_visible(const struct cg_view *view);
void
view_damage_part(struct cg_view *view);
void
view_damage_whole(struct cg_view *view);
void
view_damage_child(struct cg_view_child *view, int x, int y, bool whole);
void
view_activate(struct cg_view *view, bool activate);
void
view_position(struct cg_view *view);
void
view_for_each_surface(struct cg_view *view,
                      wlr_surface_iterator_func_t iterator, void *data);
void
view_for_each_popup(struct cg_view *view, wlr_surface_iterator_func_t iterator,
                    void *data);
void
view_unmap(struct cg_view *view);
void
view_maximize(struct cg_view *view, const struct wlr_box *tile_box);
void
view_map(struct cg_view *view, struct wlr_surface *surface);
void
view_destroy(struct cg_view *view);
void
view_init(struct cg_view *view, struct cg_workspace *ws, enum cg_view_type type,
          const struct cg_view_impl *impl);

struct wlr_surface *
view_wlr_surface_at(const struct cg_view *view, double sx, double sy,
                    double *sub_x, double *sub_y);

void
view_child_finish(struct cg_view_child *child);
void
view_child_init(struct cg_view_child *child, struct cg_view_child *parent,
                struct cg_view *view, struct wlr_surface *wlr_surface);

#endif
