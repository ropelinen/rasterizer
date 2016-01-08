#include "software_rasterizer/precompiled.h"
#include "rasterizer.h"

#include "software_rasterizer/vector.h"

/* 4 sub bits gives us [-1024, 1023] max render target.
 * It should be possible to get something similar out of 8 sub bits. */
#define SUB_BITS 4
#define TO_FIXED(val, multip) (int32_t)((val) * (multip) + 0.5f)
#define MUL_FIXED(val1, val2, multip) (((val1) * (val2)) / (multip))

#define GB_LEFT (TO_FIXED(-1024, (1 << SUB_BITS)))
#define GB_BOTTOM (TO_FIXED(-1024, (1 << SUB_BITS)))
#define GB_RIGHT (TO_FIXED(1023, (1 << SUB_BITS)))
#define GB_TOP (TO_FIXED(1023, (1 << SUB_BITS)))

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

#define OC_INSIDE 0 // 0000
#define OC_LEFT 1   // 0001
#define OC_RIGHT 2  // 0010
#define OC_BOTTOM 4 // 0100
#define OC_TOP 8    // 1000

uint32_t compute_out_code(struct vec2_int *p, const int32_t minx, const int32_t miny, const int32_t maxx, const int32_t maxy)
{
	assert(p && "compute_out_code: p is NULL");

	uint32_t code = OC_INSIDE; 

	if (p->x < minx)           // to the left of clip window
		code |= OC_LEFT;
	else if (p->x > maxx)      // to the right of clip window
		code |= OC_RIGHT;
	if (p->y < miny)           // below the clip window
		code |= OC_BOTTOM;
	else if (p->y > maxy)      // above the clip window
		code |= OC_TOP;

	return code;
}

/* Get guard-band intersection point */
struct vec2_int get_gb_intersection_point(const unsigned int oc, const struct vec2_int *p1, const struct vec2_int *p2)
{
	assert(p1 && "get_gb_intersection_point: p1 is NULL");
	assert(p2 && "get_gb_intersection_point: p2 is NULL");

	struct vec2_int result = { .x = 0, .y = 0 };

	/* When doing fp / you would normally (a << frac_bits) / b but the * does that for us already. */

	switch (oc)
	{
	case OC_LEFT:
		result.x = GB_LEFT;
		result.y = p1->y + ((GB_LEFT - p1->x) * (p2->y - p1->y)) / (p2->x - p1->x);
		break;
	case OC_RIGHT:
		result.x = GB_RIGHT;
		result.y = p1->y + ((GB_RIGHT - p1->x) * (p2->y - p1->y)) / (p2->x - p1->x);
		break;
	case OC_BOTTOM:
		result.x = p1->x + ((GB_BOTTOM - p1->y) * (p2->x - p1->x)) / (p2->y - p1->y);
		result.y = GB_BOTTOM;
		break;
	case OC_TOP:
		result.x = p1->x + ((GB_TOP - p1->y) * (p2->x - p1->x)) / (p2->y - p1->y);
		result.y = GB_TOP;
		break;
	default:
		assert(false && "get_gb_intersection_point: invalid oc");
		break;
	}
	
	return result;
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

	/* Reserve enough space for possible polys created by clipping */
	struct vec2_int work_poly[7];
	uint32_t work_vc[7];
	unsigned int work_vert_count = 3;
	unsigned int work_index_count = 3;
	unsigned int work_poly_indices[15];

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
		int32_t half_width_sub = TO_FIXED(half_width, sub_multip);
		int32_t half_height_sub = TO_FIXED(half_height, sub_multip);

		work_poly[0].x = TO_FIXED(vert_buf[ind_buf[i]].x / vert_buf[ind_buf[i]].w * half_width, sub_multip);
		work_poly[0].y = TO_FIXED(vert_buf[ind_buf[i]].y / vert_buf[ind_buf[i]].w * half_height, sub_multip);
		work_poly[1].x = TO_FIXED(vert_buf[ind_buf[i + 1]].x / vert_buf[ind_buf[i + 1]].w * half_width, sub_multip);
		work_poly[1].y = TO_FIXED(vert_buf[ind_buf[i + 1]].y / vert_buf[ind_buf[i + 1]].w * half_height, sub_multip);
		work_poly[2].x = TO_FIXED(vert_buf[ind_buf[i + 2]].x / vert_buf[ind_buf[i + 2]].w * half_width, sub_multip);
		work_poly[2].y = TO_FIXED(vert_buf[ind_buf[i + 2]].y / vert_buf[ind_buf[i + 2]].w * half_height, sub_multip);
		work_poly_indices[0] = 0; work_poly_indices[1] = 1; work_poly_indices[2] = 2;
		work_vert_count = 3;
		work_index_count = 3;
		
		work_vc[0] = vert_colors[ind_buf[i]];
		work_vc[1] = vert_colors[ind_buf[i + 1]];
		work_vc[2] = vert_colors[ind_buf[i + 2]];

		/* Test view port x, y clipping */
		uint32_t oc0 = compute_out_code(&work_poly[0], -half_width_sub, -half_height_sub, half_width_sub - sub_multip, half_height_sub - sub_multip);
		uint32_t oc1 = compute_out_code(&work_poly[1], -half_width_sub, -half_height_sub, half_width_sub - sub_multip, half_height_sub - sub_multip);
		uint32_t oc2 = compute_out_code(&work_poly[2], -half_width_sub, -half_height_sub, half_width_sub - sub_multip, half_height_sub - sub_multip);
		if ((oc0 | oc1 | oc2) == 0) 
		{ 
			/* Whole poly inside the view, trivially accept */
		}
		else if (oc0 & oc1 & oc2) 
		{
			/* All points in the same outside region, trivially reject */
			continue;
		}
		else
		{
			/* Test guard band clipping */
			const int32_t minx = TO_FIXED(-1024, sub_multip);
			const int32_t miny = TO_FIXED(-1024, sub_multip);
			const int32_t maxx = TO_FIXED(1023, sub_multip);
			const int32_t maxy = TO_FIXED(1023, sub_multip);

			oc0 = compute_out_code(&work_poly[0], minx, miny, maxx, maxy);
			oc1 = compute_out_code(&work_poly[1], minx, miny, maxx, maxy);
			oc2 = compute_out_code(&work_poly[2], minx, miny, maxx, maxy);

			if ((oc0 | oc1 | oc2) == 0)
			{
				/* Whole poly inside the guard band, trivially accept */
			}
			else if (oc0 & oc1 & oc2)
			{
				/* Partially inside the view port and not inside the guard band?? */
				assert(false && "rasterizer_rasterize: Poly can't be partailly in view and wholly totally outside the guard band simultaniously.");
				continue;
			}
			else
			{
				/* Sutherland–Hodgman algorithm */

				/* We use indices to create a closed convex polygon.
				 * Closed as in same first and last vertex. */
				work_index_count = 4;
				work_poly_indices[3] = 0;

				unsigned int clipped_vert_count = 0;
				struct vec2_int clipped_poly[7];
				uint32_t clipped_vc[7];
				unsigned int clipped_index_count = 0;
				unsigned int clipped_poly_indices[15];

				/* Loop all edges */
				unsigned int oc_codes[4] = { OC_LEFT, OC_BOTTOM, OC_RIGHT, OC_TOP };
				for (unsigned int oc_index = 0; oc_index < 4; ++oc_index)
				{
					unsigned int oc = oc_codes[oc_index];
					clipped_vert_count = 0;
					clipped_index_count = 0;
					for (unsigned int vert_index = 0; vert_index < work_index_count - 1; ++vert_index)
					{
						if ((compute_out_code(&work_poly[work_poly_indices[vert_index]], minx, miny, maxx, maxy) & oc) == 0)
						{
							if ((compute_out_code(&work_poly[work_poly_indices[vert_index + 1]], minx, miny, maxx, maxy) & oc) == 0)
							{
								/* both inside, add second */
								clipped_poly[clipped_vert_count] = work_poly[work_poly_indices[vert_index + 1]];
								clipped_vc[clipped_vert_count] = work_vc[work_poly_indices[vert_index + 1]];
								clipped_poly_indices[clipped_index_count] = clipped_vert_count;
								++clipped_vert_count;
								++clipped_index_count;
							}
							else
							{
								/* second out, add intersection */
								clipped_poly[clipped_vert_count] = get_gb_intersection_point(oc, &work_poly[work_poly_indices[vert_index]], &work_poly[work_poly_indices[vert_index + 1]]);
								clipped_poly_indices[clipped_index_count] = clipped_vert_count;

								/* lerp color */
								uint32_t temp_x = work_poly[work_poly_indices[vert_index + 1]].x - work_poly[work_poly_indices[vert_index]].x;
								uint32_t temp_y = work_poly[work_poly_indices[vert_index + 1]].y - work_poly[work_poly_indices[vert_index]].y;
								uint32_t len_org = (temp_x * temp_x + temp_y * temp_y) >> SUB_BITS;
								temp_x = clipped_poly[clipped_vert_count].x - work_poly[work_poly_indices[vert_index]].x;
								temp_y = clipped_poly[clipped_vert_count].y - work_poly[work_poly_indices[vert_index]].y;
								uint32_t len_int = (temp_x * temp_x + temp_y * temp_y) >> SUB_BITS;
								float weight = (float)len_int / (float)len_org;

								uint32_t n_r = (uint32_t)(((float)(work_vc[work_poly_indices[vert_index]] & 0xFF0000) * (1.0f - weight)) + ((float)(work_vc[work_poly_indices[vert_index + 1]] & 0xFF0000) * weight)) & 0xFF0000;
								uint32_t n_g = (uint32_t)(((float)(work_vc[work_poly_indices[vert_index]] & 0x00FF00) * (1.0f - weight)) + ((float)(work_vc[work_poly_indices[vert_index + 1]] & 0x00FF00) * weight)) & 0x00FF00;
								uint32_t n_b = (uint32_t)(((float)(work_vc[work_poly_indices[vert_index]] & 0x0000FF) * (1.0f - weight)) + ((float)(work_vc[work_poly_indices[vert_index + 1]] & 0x0000FF) * weight)) & 0x0000FF;
								clipped_vc[clipped_vert_count] = n_r | n_g | n_b;

								++clipped_vert_count;
								++clipped_index_count;
							}
						}
						else
						{
							if ((compute_out_code(&work_poly[work_poly_indices[vert_index + 1]], minx, miny, maxx, maxy) & oc) == 0)
							{
								/* second in, add intersection and second */
								clipped_poly[clipped_vert_count] = get_gb_intersection_point(oc, &work_poly[work_poly_indices[vert_index + 1]], &work_poly[work_poly_indices[vert_index]]);
								clipped_poly_indices[clipped_index_count] = clipped_vert_count;

								/* lerp color */
								uint32_t temp_x = work_poly[work_poly_indices[vert_index + 1]].x - work_poly[work_poly_indices[vert_index]].x;
								uint32_t temp_y = work_poly[work_poly_indices[vert_index + 1]].y - work_poly[work_poly_indices[vert_index]].y;
								uint32_t len_org = temp_x * temp_x + temp_y * temp_y;
								temp_x = clipped_poly[clipped_vert_count].x - work_poly[work_poly_indices[vert_index]].x;
								temp_y = clipped_poly[clipped_vert_count].y - work_poly[work_poly_indices[vert_index]].y;
								uint32_t len_int = temp_x * temp_x + temp_y * temp_y;;
								float weight = (float)len_int / (float)len_org;

								uint32_t n_r = (uint32_t)(((float)(work_vc[work_poly_indices[vert_index]] & 0xFF0000) * (1.0f - weight)) + ((float)(work_vc[work_poly_indices[vert_index + 1]] & 0xFF0000) * weight)) & 0xFF0000;
								uint32_t n_g = (uint32_t)(((float)(work_vc[work_poly_indices[vert_index]] & 0x00FF00) * (1.0f - weight)) + ((float)(work_vc[work_poly_indices[vert_index + 1]] & 0x00FF00) * weight)) & 0x00FF00;
								uint32_t n_b = (uint32_t)(((float)(work_vc[work_poly_indices[vert_index]] & 0x0000FF) * (1.0f - weight)) + ((float)(work_vc[work_poly_indices[vert_index + 1]] & 0x0000FF) * weight)) & 0x0000FF;
								clipped_vc[clipped_vert_count] = n_r | n_g | n_b;

								++clipped_vert_count;
								++clipped_index_count;

								clipped_poly[clipped_vert_count] = work_poly[work_poly_indices[vert_index + 1]];
								clipped_vc[clipped_vert_count] = work_vc[work_poly_indices[vert_index + 1]];
								clipped_poly_indices[clipped_index_count] = clipped_vert_count;
								++clipped_vert_count;
								++clipped_index_count;
							}
						}
					}
					/* Make sure we have a closed poly */
					clipped_poly_indices[clipped_index_count] = 0;
					++clipped_index_count;
					
					assert(clipped_vert_count < 7 && "rasterizer_rasterize: too many vertices in the clipped poly");

					work_vert_count = clipped_vert_count;
					for (unsigned int j = 0; j < work_vert_count; ++j)
					{
						work_poly[j] = clipped_poly[j];
						work_vc[j] = clipped_vc[j];
					}

					work_index_count = clipped_index_count;
					for (unsigned int j = 0; j < work_index_count; ++j)
						work_poly_indices[j] = clipped_poly_indices[j];
				}

				--work_index_count;

				/* Need to generate proper index list */
				unsigned int temp_ind_buf[15];
				unsigned int temp_ind_count = 0;
				for (unsigned int vert_index = 1; vert_index < work_index_count - 1; ++vert_index)
				{
					temp_ind_buf[temp_ind_count] = work_poly_indices[0];
					temp_ind_buf[temp_ind_count + 1] = work_poly_indices[vert_index];
					temp_ind_buf[temp_ind_count + 2] = work_poly_indices[vert_index + 1];
					temp_ind_count += 3;
				}
				for (unsigned int j = 0; j < temp_ind_count; ++j)
					work_poly_indices[j] = temp_ind_buf[j];

				work_index_count = temp_ind_count;
				assert(work_index_count < 15 && "rasterizer_rasterize: too many indices in the clipped poly");
				assert(work_index_count % 3 == 0 && "rasterizer_rasterize: clipped index count is invalid");
			}
		}		

		for (unsigned ind_i = 0; ind_i < work_index_count; ind_i += 3)
		{
			const unsigned int i0 = work_poly_indices[ind_i];
			const unsigned int i1 = work_poly_indices[ind_i + 1];
			const unsigned int i2 = work_poly_indices[ind_i + 2];

			/* Bounding box */
			struct vec2_int min;
			struct vec2_int max;
			min.x = min3(work_poly[i0].x, work_poly[i1].x, work_poly[i2].x);
			min.y = min3(work_poly[i0].y, work_poly[i1].y, work_poly[i2].y);
			max.x = max3(work_poly[i0].x, work_poly[i1].x, work_poly[i2].x);
			max.y = max3(work_poly[i0].y, work_poly[i1].y, work_poly[i2].y);

			/* Clip to screen and round to pixel centers */
			min.x = (max(min.x, -half_width_sub) & ~sub_mask) + half_pixel;
			min.y = (max(min.y, -half_height_sub) & ~sub_mask) + half_pixel;
			max.x = ((min(max.x, half_width_sub - sub_multip) + sub_mask) & ~sub_mask) + half_pixel;
			max.y = ((min(max.y, half_height_sub - sub_multip) + sub_mask) & ~sub_mask) + half_pixel;

			/* Orient at min point
			 * The top or left bias causes the weights to get offset by 1 sub-pixel.
			 * Could correct for that to get everything depending on the weights just right
			 * but I think I can live with an error of 1 sub pixel (1/16 pixel currently). */
			int32_t w0_row = winding_2d(&work_poly[i1], &work_poly[i2], &min) + (is_top_or_left(&work_poly[i1], &work_poly[i2]) ? 0 : -1);
			int32_t w1_row = winding_2d(&work_poly[i2], &work_poly[i0], &min) + (is_top_or_left(&work_poly[i2], &work_poly[i0]) ? 0 : -1);
			int32_t w2_row = winding_2d(&work_poly[i0], &work_poly[i1], &min) + (is_top_or_left(&work_poly[i0], &work_poly[i1]) ? 0 : -1);

			/* Calculate steps */
			int32_t step_x_01 = work_poly[i0].y - work_poly[i1].y;
			int32_t step_x_12 = work_poly[i1].y - work_poly[i2].y;
			int32_t step_x_20 = work_poly[i2].y - work_poly[i0].y;

			int32_t step_y_01 = work_poly[i1].x - work_poly[i0].x;
			int32_t step_y_12 = work_poly[i2].x - work_poly[i1].x;
			int32_t step_y_20 = work_poly[i0].x - work_poly[i2].x;

			float double_tri_area = (float)winding_2d(&work_poly[i0], &work_poly[i1], &work_poly[i2]);

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
				int32_t w0 = w0_row;
				int32_t w1 = w1_row;
				int32_t w2 = w2_row;

				unsigned int pixel_index = pixel_index_row;

				for (point.x = min.x; point.x <= max.x; point.x += sub_multip)
				{
					if ((w0 | w1 | w2) >= 0)
					{
						float w0_f = min((float)w0 / double_tri_area, 1.0f);
						float w1_f = min((float)w1 / double_tri_area, 1.0f);
						float w2_f = max(1.0f - w0_f - w1_f, 0.0f);

						uint32_t n_r = (uint32_t)(((float)(work_vc[i0] & 0xFF0000) * w0_f) + ((float)(work_vc[i1] & 0xFF0000) * w1_f) + ((float)(work_vc[i2] & 0xFF0000) * w2_f)) & 0xFF0000;
						uint32_t n_g = (uint32_t)(((float)(work_vc[i0] & 0x00FF00) * w0_f) + ((float)(work_vc[i1] & 0x00FF00) * w1_f) + ((float)(work_vc[i2] & 0x00FF00) * w2_f)) & 0x00FF00;
						uint32_t n_b = (uint32_t)(((float)(work_vc[i0] & 0x0000FF) * w0_f) + ((float)(work_vc[i1] & 0x0000FF) * w1_f) + ((float)(work_vc[i2] & 0x0000FF) * w2_f)) & 0x0000FF;

						assert(pixel_index < (unsigned)(target_size->x * target_size->y) && "rasterizer_rasterize: invalid pixel_index");
						render_target[pixel_index] = n_r | n_g | n_b;
					}

					w0 += step_x_12;
					w1 += step_x_20;
					w2 += step_x_01;

					++pixel_index;
				}

				w0_row += step_y_12;
				w1_row += step_y_20;
				w2_row += step_y_01;

				pixel_index_row -= target_size->x;
			}
		}
	}
}
