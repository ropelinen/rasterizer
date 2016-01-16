#ifndef RPLNN_RASTERIZER_H
#define RPLNN_RASTERIZER_H

/* Currently wants the points in pixel coordinates, (0, 0) at screen center increasing towards top-right. Tri wanted as CCW */
void rasterizer_rasterize(uint32_t *render_target, float *depth_buf, const struct vec2_int *target_size, const struct vec4_float *vert_buf, const struct vec2_float *uv_buf, const unsigned int *ind_buf, const unsigned int index_count, uint32_t *texture, struct vec2_int *texture_size);
void rasterizer_clear_depth_buffer(float *depth_buf, const struct vec2_int *buf_size);

#endif /* RPLNN_RASTERIZER_H */
