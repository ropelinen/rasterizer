#include "software_rasterizer/precompiled.h"
#include "rasterizer.h"

#include "software_rasterizer/vector.h"

/* 4 sub bits gives us [-1024, 1023] max render target.
 * It should be possible to get something similar out of 8 sub bits. */
#define SUB_BITS 4
#define TO_FIXED(val, multip) (int32_t)((val) * (multip) + 0.5f)
#define MUL_FIXED(val1, val2, multip) (((val1) * (val2)) / (multip))

/* Taken straight from https://fgiesen.wordpress.com/2013/02/08/triangle-rasterization-in-practice/ 
 * Returns the signed A*2 of the triangle formed by the three points. 
 * The sign is positive with CCW tri in a coordinate system with up-right positive axes. */
int32_t winding_2d(const struct vec2_int *p1, const struct vec2_int *p2, const struct vec2_int *p3)
{
	assert(p1 && "winding_2d: p1 is NULL");
	assert(p2 && "winding_2d: p2 is NULL");
	assert(p3 && "winding_2d: p3 is NULL");

	int32_t sub_multip = 1 << SUB_BITS;
	return MUL_FIXED(p1->y - p2->y, p3->x, sub_multip) + MUL_FIXED(p2->x - p1->x, p3->y, sub_multip) + (MUL_FIXED(p1->x, p2->y, sub_multip) - MUL_FIXED(p1->y, p2->x, sub_multip));
}

bool is_top_or_left(const struct vec2_int *p1, const struct vec2_int *p2)
{
	assert(p1 && "is_top_or_left: p1 is NULL");
	assert(p2 && "is_top_or_left: p2 is NULL");
	
	/* Left || top */
	return ((p2->y < p1->y) || (p2->x < p1->x && p1->y == p2->y));
}

void rasterizer_rasterize(uint32_t *render_target, const struct vec2_int *target_size, const struct vec4_float *vert_buf, const uint32_t *vert_colors, const unsigned int *ind_buf, const unsigned int index_count)
{
	assert(render_target && "rasterizer_rasterize: render_target is NULL");
	assert(target_size && "rasterizer_rasterize: target_size is NULL");
	assert(vert_buf && "rasterizer_rasterize: vert_buf is NULL");
	assert(vert_colors && "rasterizer_rasterize: vert_colors is NULL");
	assert(ind_buf && "rasterizer_rasterize: ind_buf is NULL");
	assert(index_count % 3 == 0 && "rasterizer_rasterize: index count is not valid");
	assert(SUB_BITS == 4 && "rasterizer_rasterize: SUB_BITS has changed, check the assert below.");
	assert(target_size->x <= 2048 && target_size->y <= 2048 && "rasterizer_rasterize: render target is too large");

	/* Sub-pixel constants */
	const int32_t sub_multip = 1 << SUB_BITS;
	const int32_t half_pixel = sub_multip >> 1;
	const int32_t sub_mask = sub_multip - 1;

	for (unsigned int i = 0; i < index_count; i += 3)
	{
		/* Skip the tri if any of its vertices is outside the near/far planes.
		 * A bit hacky but good enough for the time being. */
		if (vert_buf[ind_buf[i]].z < 0.0f || vert_buf[ind_buf[i]].z > vert_buf[ind_buf[i]].w ||
			vert_buf[ind_buf[i + 1]].z < 0.0f || vert_buf[ind_buf[i + 1]].z > vert_buf[ind_buf[i + 1]].w ||
			vert_buf[ind_buf[i + 2]].z < 0.0f || vert_buf[ind_buf[i + 2]].z > vert_buf[ind_buf[i + 2]].w)
			continue;

		int32_t half_width = target_size->x / 2;
		int32_t half_height = target_size->y / 2;

		struct vec2_int p1;
		p1.x = TO_FIXED(vert_buf[ind_buf[i]].x / vert_buf[ind_buf[i]].w * half_width, sub_multip);
		p1.y = TO_FIXED(vert_buf[ind_buf[i]].y / vert_buf[ind_buf[i]].w * half_height, sub_multip);
		struct vec2_int p2;
		p2.x = TO_FIXED(vert_buf[ind_buf[i + 1]].x / vert_buf[ind_buf[i + 1]].w * half_width, sub_multip);
		p2.y = TO_FIXED(vert_buf[ind_buf[i + 1]].y / vert_buf[ind_buf[i + 1]].w * half_height, sub_multip);
		struct vec2_int p3;
		p3.x = TO_FIXED(vert_buf[ind_buf[i + 2]].x / vert_buf[ind_buf[i + 2]].w * half_width, sub_multip);
		p3.y = TO_FIXED(vert_buf[ind_buf[i + 2]].y / vert_buf[ind_buf[i + 2]].w * half_height, sub_multip);

		uint32_t vc1 = vert_colors[ind_buf[i]];
		uint32_t vc2 = vert_colors[ind_buf[i + 1]];
		uint32_t vc3 = vert_colors[ind_buf[i + 2]];

		/* Bounding box */
		struct vec2_int min;
		struct vec2_int max;
		min.x = min3(p1.x, p2.x, p3.x);
		min.y = min3(p1.y, p2.y, p3.y);
		max.x = max3(p1.x, p2.x, p3.x);
		max.y = max3(p1.y, p2.y, p3.y);

		/* Clip to screen and round to pixel centers */
		int32_t half_width_sub = TO_FIXED(half_width, sub_multip);
		int32_t half_height_sub = TO_FIXED(half_height, sub_multip);

		min.x = (max(min.x, -half_width_sub) & ~sub_mask) + half_pixel;
		min.y = (max(min.y, -half_height_sub) & ~sub_mask) + half_pixel;
		max.x = ((min(max.x, half_width_sub - sub_multip) + sub_mask) & ~sub_mask) + half_pixel;
		max.y = ((min(max.y, half_height_sub - sub_multip) + sub_mask) & ~sub_mask) + half_pixel;

		/* Orient at min point 
		 * The top or left bias causes the weights to get offset by 1 sub-pixel.
		 * Could correct for that to get everything depending on the weights just right
		 * but I think I can live with an error of 1 sub pixel (1/16 pixel currently). */
		int32_t w1_row = winding_2d(&p2, &p3, &min) + (is_top_or_left(&p2, &p3) ? 0 : -1);
		int32_t w2_row = winding_2d(&p3, &p1, &min) + (is_top_or_left(&p3, &p1) ? 0 : -1);
		int32_t w3_row = winding_2d(&p1, &p2, &min) + (is_top_or_left(&p1, &p2) ? 0 : -1);

		/* Calculate steps */
		int32_t step_x_12 = p1.y - p2.y;
		int32_t step_x_23 = p2.y - p3.y;
		int32_t step_x_31 = p3.y - p1.y;

		int32_t step_y_12 = p2.x - p1.x;
		int32_t step_y_23 = p3.x - p2.x;
		int32_t step_y_31 = p1.x - p3.x;

		float double_tri_area = (float)winding_2d(&p1, &p2, &p3);

		/* How we calculate and step this is based on the format of the backbuffer.
		 * Instead of trying to support what ever the blitter uses should just require some specific format (goes also for color format).
		 * Could for example use tiling or swizzling for optimization https://fgiesen.wordpress.com/2011/01/17/texture-tiling-and-swizzling/ 
		 * Currently render targer format: 0, 0 at top left, increases towards bottom right, 32bit Reserved|Red|Green|Blue */
		unsigned int pixel_index_row = target_size->x 
			* ((target_size->y - 1) - (((min.y - half_pixel) / sub_multip) + half_height)) /* y */
			+ (((min.x - half_pixel) / sub_multip) + half_width); /* x */

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
					float w1_f = min((float)w1 / double_tri_area, 1.0f);
					float w2_f = min((float)w2 / double_tri_area, 1.0f);
					float w3_f = max(1.0f - w1_f - w2_f, 0.0f);

					uint32_t n_r = (uint32_t)(((float)(vc1 & 0xFF0000) * w1_f) + ((float)(vc2 & 0xFF0000) * w2_f) + ((float)(vc3 & 0xFF0000) * w3_f)) & 0xFF0000;
					uint32_t n_g = (uint32_t)(((float)(vc1 & 0x00FF00) * w1_f) + ((float)(vc2 & 0x00FF00) * w2_f) + ((float)(vc3 & 0x00FF00) * w3_f)) & 0x00FF00;
					uint32_t n_b = (uint32_t)(((float)(vc1 & 0x0000FF) * w1_f) + ((float)(vc2 & 0x0000FF) * w2_f) + ((float)(vc3 & 0x0000FF) * w3_f)) & 0x0000FF;

					assert(pixel_index < (unsigned)(target_size->x * target_size->y) && "rasterizer_rasterize: invalid pixel_index");
					render_target[pixel_index] = n_r | n_g | n_b;
				}

				w1 += step_x_23;
				w2 += step_x_31;
				w3 += step_x_12;

				++pixel_index;
			}

			w1_row += step_y_23;
			w2_row += step_y_31;
			w3_row += step_y_12;

			pixel_index_row -= target_size->x;
		}
	}
}
