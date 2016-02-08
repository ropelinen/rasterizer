#ifndef RPLNN_RASTERIZER_H
#define RPLNN_RASTERIZER_H

/* Left handed coordinate system. Tris wanted as CCW. 
 * Depth buffer stores the depth in the first 24bits and the rest 8 are reserved for future use (stencil). 
 * Rasterize area is in inclusive pixel values for min >= 0 && max < target_size && min < max.
 * When using SIMD rasterize area min must be even and rasterize area max must be odd because of 2x2 tiling.
 * Render target and depth buffer must have their 0,0 at botto left corner. */
void rasterizer_rasterize(uint32_t *render_target, uint32_t *depth_buf, const struct vec2_int *target_size, const struct vec2_int *rasterize_area_min, const struct vec2_int *rasterize_area_max,
	const struct vec4_float *vert_buf, const struct vec2_float *uv_buf, const unsigned int *ind_buf, const unsigned int index_count, 
	const uint32_t *texture, const struct vec2_int *texture_size);
void rasterizer_clear_depth_buffer(uint32_t *depth_buf, const struct vec2_int *buf_size);
/* When SIMD is used the render target and depth buffer will be tiled.
 * They are tiled to 2x2 pixel blocks bottom two pixels first followed by the top two pixels. */
bool rasterizer_uses_simd(void);

#endif /* RPLNN_RASTERIZER_H */
