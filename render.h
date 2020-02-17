#ifndef CG_RENDER_H
#define CG_RENDER_H

struct cg_output;

void
output_render(struct cg_output *output, pixman_region32_t *damage);

#endif
