#ifndef RPLNN_RASTERIZER_H
#define RPLNN_RASTERIZER_H

/* Currently wants the points in pixel coordinates, (0, 0) at screen center increasing towards top-right. Tri wanted as CCW */
void rasterizer_rasterize(uint32_t *render_target, const struct vec2_int *target_size, const struct vec3_float *vert_buf, const uint32_t *vert_colors, const unsigned int *ind_buf, const unsigned int index_count);

#endif /* RPLNN_RASTERIZER_H */
