#ifndef RPLNN_RASTERIZER_H
#define RPLNN_RASTERIZER_H

/* Should take a vertex and index buffers instead of three points, this is just stupid. */
/* Currently wants the points in pixel coordinates, (0, 0) at screen center increasing towards top-right. Tri wanted as CCW */
void rasterizer_rasterize_triangle(uint32_t *render_target, const struct vec2_int *target_size, const struct vec2_int *p1, const struct vec2_int *p2, const struct vec2_int *p3);

#endif /* RPLNN_RASTERIZER_H */