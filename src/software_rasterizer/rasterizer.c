#include "software_rasterizer/precompiled.h"
#include "rasterizer.h"

#include "software_rasterizer/vector.h"

/* 4 sub bits gives us [-1024, 1023] max render target.
 * It should be possible to get something similar out of 8 sub bits. */
#define SUB_BITS 4
#define TO_FIXED(val, multip) (int32_t)((val) * (multip) + 0.5f)
#define MUL_FIXED(val1, val2, multip) (((val1) * (val2)) / (multip))

/* Taken straight from https://fgiesen.wordpress.com/2013/02/08/triangle-rasterization-in-practice/ */
int32_t orient2d(const struct vec2_int *p1, const struct vec2_int *p2, const struct vec2_int *p3)
{
	assert(p1 && "orient2d: p1 is NULL");
	assert(p2 && "orient2d: p2 is NULL");
	assert(p3 && "orient2d: p3 is NULL");

	int32_t sub_multip = 1 << SUB_BITS;
	return MUL_FIXED(p1->y - p2->y, p3->x, sub_multip) + MUL_FIXED(p2->x - p1->x, p3->y, sub_multip) + (MUL_FIXED(p1->x, p2->y, sub_multip) - MUL_FIXED(p1->y, p2->x, sub_multip));
}

void rasterizer_rasterize_triangle(uint32_t *render_target, const struct vec2_int *target_size, const struct vec3_float *vert_buf, const unsigned int *ind_buf, const unsigned int index_count)
{
	assert(render_target && "rasterizer_rasterize_triangle: render_target is NULL");
	assert(target_size && "rasterizer_rasterize_triangle: target_size is NULL");
	assert(vert_buf && "rasterizer_rasterize_triangle: vert_buf is NULL");
	assert(ind_buf && "rasterizer_rasterize_triangle: ind_buf is NULL");
	assert(index_count % 3 == 0 && "rasterizer_rasterize_triangle: index count is not valid");
	assert(SUB_BITS == 4 && "rasterizer_rasterize_triangle: SUB_BITS has changed, check the assert below.");
	assert(target_size->x <= 2048 && target_size->y <= 2048 && "rasterizer_rasterize_triangle: render target is too large");

	/* Sub-pixel constants */
	const int32_t sub_multip = 1 << SUB_BITS;
	const int32_t half_pixel = sub_multip >> 1;
	const int32_t sub_mask = sub_multip - 1;

	/* Bounding box */
	struct vec2_int min;
	struct vec2_int max;

	for (unsigned int i = 0; i < index_count; i += 3)
	{
		struct vec2_int p1;
		p1.x = TO_FIXED(vert_buf[ind_buf[i]].x, sub_multip);
		p1.y = TO_FIXED(vert_buf[ind_buf[i]].y, sub_multip);
		struct vec2_int p2;
		p2.x = TO_FIXED(vert_buf[ind_buf[i + 1]].x, sub_multip);
		p2.y = TO_FIXED(vert_buf[ind_buf[i + 1]].y, sub_multip);
		struct vec2_int p3;
		p3.x = TO_FIXED(vert_buf[ind_buf[i + 2]].x, sub_multip);
		p3.y = TO_FIXED(vert_buf[ind_buf[i + 2]].y, sub_multip);

		min.x = min3(p1.x, p2.x, p3.x);
		min.y = min3(p1.y, p2.y, p3.y);
		max.x = max3(p1.x, p2.x, p3.x);
		max.y = max3(p1.y, p2.y, p3.y);

		/* Clip to screen and round to pixel centers */
		int32_t half_width = target_size->x / 2; 
		int32_t half_height = target_size->y / 2; 
		int32_t half_width_sub = TO_FIXED(half_width, sub_multip);
		int32_t half_height_sub = TO_FIXED(half_height, sub_multip);

		min.x = (max(min.x, -half_width_sub) & ~sub_mask) + half_pixel;
		min.y = (max(min.y, -half_height_sub) & ~sub_mask) + half_pixel;
		max.x = ((min(max.x, half_width_sub - sub_multip) + sub_mask) & ~sub_mask) + half_pixel;
		max.y = ((min(max.y, half_height_sub - sub_multip) + sub_mask) & ~sub_mask) + half_pixel;

		/* Orient at min point */
		int32_t w1_row = orient2d(&p1, &p2, &min);
		int32_t w2_row = orient2d(&p2, &p3, &min);
		int32_t w3_row = orient2d(&p3, &p1, &min);

		/* Calculate steps */
		int32_t step_x_12 = p1.y - p2.y;
		int32_t step_x_23 = p2.y - p3.y;
		int32_t step_x_31 = p3.y - p1.y;

		int32_t step_y_12 = p2.x - p1.x;
		int32_t step_y_23 = p3.x - p2.x;
		int32_t step_y_31 = p1.x - p3.x;

		/* How we calculate and step this is based on the format of the backbuffer.
		 * Instead of trying to support what ever the blitter uses should just require some specific format (goes also for color format).
		 * Could for example use tiling or swizzling for optimization https://fgiesen.wordpress.com/2011/01/17/texture-tiling-and-swizzling/ 
		 * Currently render targer format: 0, 0 at top left, increases towards bottom right, 32bit Reserved|Red|Green|Blue */
		unsigned int pixel_index_row = target_size->x * (-(min.y / sub_multip) + half_height) + (min.x / sub_multip) + half_width;

		/* Rasterize */
		struct vec2_int point;
		for (point.y = min.y; point.y <= max.y; point.y += sub_multip)
		{
			int32_t w1 = w1_row;
			int32_t w2 = w2_row;
			int32_t w3 = w3_row;

			unsigned int pixel_index = pixel_index_row;

			for (point.x = min.x; point.x <= max.x; point.x += sub_multip)
			{
				if ((w1 | w2 | w3) >= 0)
				{
					render_target[pixel_index] = 0x0000FF;
				}

				w1 += step_x_12;
				w2 += step_x_23;
				w3 += step_x_31;

				++pixel_index;
			}

			w1_row += step_y_12;
			w2_row += step_y_23;
			w3_row += step_y_31;

			pixel_index_row -= target_size->x;
		}
	}
}
