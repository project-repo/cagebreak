#ifndef CG_UTIL_H
#define CG_UTIL_H

struct wlr_box;

/** Apply scale to a width or height. */
int
scale_length(int length, int offset, double scale);

void
scale_box(struct wlr_box *box, double scale);

#endif
