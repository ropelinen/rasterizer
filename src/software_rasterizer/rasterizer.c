#include "software_rasterizer/precompiled.h"
#include "rasterizer.h"

#include <math.h>

#include "software_rasterizer/vector.h"

/* Enables parallelization using SSE2.
 * Notably faster than the non-simd version so should be used if possible. 
 * Using simd is highly recommended if you are building a x86 version. */
#define USE_SIMD 1
#ifdef USE_SIMD
/* Uses TILE_SIZE*TILE_SIZE blocks for render target and depth buffer to reduce cache misses.
 * As raster areas must be aligned to tiles and be tile sized 
 * rastering takes a notable hit from per triangle setup and clipping,
 * unless tris are properly binned to the raster areas. */
//#define USE_TILES 1
#endif
#define TILE_SIZE 64

#ifdef USE_SIMD
#include <emmintrin.h>
#endif

/* 4 sub bits gives us [-2048, 2047] max render target.
 * It should be possible to get something similar out of 8 sub bits. */
#define SUB_BITS 4
#define TO_FIXED(val, multip) (int32_t)((val) * (multip) + 0.5f)
#define MUL_FIXED(val1, val2, multip) (((val1) * (val2)) / (multip))

/* Number of bits reserved for depth
 * The rest are reserved for future use (stencil) */
#define DEPTH_BITS 24

#define GB_MIN -2048
#define GB_MAX 2047
#define GB_LEFT (TO_FIXED(GB_MIN, (1 << SUB_BITS)))
#define GB_BOTTOM (TO_FIXED(GB_MIN, (1 << SUB_BITS)))
#define GB_RIGHT (TO_FIXED(GB_MAX, (1 << SUB_BITS)))
#define GB_TOP (TO_FIXED(GB_MAX, (1 << SUB_BITS)))

#ifdef USE_SIMD
/* From http://stackoverflow.com/questions/10500766/sse-multiplication-of-4-32-bit-integers */
__m128i mul_epi32(const __m128i a, const __m128i b)
{
	__m128i tmp1 = _mm_mul_epu32(a, b); /* mul 2,0*/
	__m128i tmp2 = _mm_mul_epu32(_mm_srli_si128(a, 4), _mm_srli_si128(b, 4)); /* mul 3,1 */
	return _mm_unpacklo_epi32(_mm_shuffle_epi32(tmp1, _MM_SHUFFLE(0, 0, 2, 0)), _mm_shuffle_epi32(tmp2, _MM_SHUFFLE(0, 0, 2, 0))); /* shuffle results to [63..0] and pack */
}
#endif

/* Taken straight from https://fgiesen.wordpress.com/2013/02/08/triangle-rasterization-in-practice/ 
 * Returns the signed A*2 of the triangle formed by the three points. 
 * The sign is positive with CCW tri in a coordinate system with up-right positive axes. */
int32_t winding_2d(const struct vec2_int *p1, const struct vec2_int *p2, const struct vec2_int *p3)
{
	assert(p1 && "winding_2d: p1 is NULL");
	assert(p2 && "winding_2d: p2 is NULL");
	assert(p3 && "winding_2d: p3 is NULL");

	const int32_t sub_multip = 1 << SUB_BITS;
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

	/* When doing fp / you would normally (a << frac_bits) / b but the multiply does that for us already. */

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

void lerp_vert_attributes(const struct vec2_int* vec_arr, const float *z_arr, const float *w_arr, const struct vec2_float *uv_arr, unsigned int p0i, unsigned int p1i, 
	const struct vec2_int *clip, float *out_clipz, float *out_clipw, struct vec2_float *out_clipuv)
{
	assert(vec_arr && "lerp_vert_attributes: vec_arr is NULL");
	assert(z_arr && "lerp_vert_attributes: z_arr is NULL");
	assert(w_arr && "lerp_vert_attributes: w_arr is NULL");
	assert(uv_arr && "lerp_vert_attributes: uv_arr is NULL");
	assert(clip && "lerp_vert_attributes: clip is NULL");
	assert(out_clipz && "lerp_vert_attributes: out_clipz is NULL");
	assert(out_clipw && "lerp_vert_attributes: out_clipw is NULL");
	assert(out_clipuv && "lerp_vert_attributes: out_clipuv is NULL");

	const int32_t sub_multip = 1 << SUB_BITS;

	/* Calculate weight */
	uint32_t temp_x = vec_arr[p1i].x - vec_arr[p0i].x;
	uint32_t temp_y = vec_arr[p1i].y - vec_arr[p0i].y;
	uint32_t len_org = MUL_FIXED(temp_x, temp_x, sub_multip) + MUL_FIXED(temp_y, temp_y, sub_multip);
	temp_x = clip->x - vec_arr[p0i].x;
	temp_y = clip->y - vec_arr[p0i].y;
	uint32_t len_int = MUL_FIXED(temp_x, temp_x, sub_multip) + MUL_FIXED(temp_y, temp_y, sub_multip);
	float weight = (float)len_int / (float)len_org;
	weight = sqrtf(weight);
	assert(weight >= 0.0f && weight <= 1.0f && "lerp_vert_attributes: invalid weight");

	/* Interpolate z */
	float z0 = z_arr[p0i];
	float z1 = z_arr[p1i];
	*out_clipz = (z0 + (z1 - z0) * weight);

	/* Interpolate w */
	float interp_w = w_arr[p0i] + (w_arr[p1i] - w_arr[p0i]) * weight;
	*out_clipw = interp_w;

	/* Interpolate uv */
	float uv0 = uv_arr[p0i].x * w_arr[p0i];
	float uv1 = uv_arr[p1i].x * w_arr[p1i];
	out_clipuv->x = (uv0 + (uv1 - uv0) * weight) / interp_w;
	assert(out_clipuv->x >= 0.0f && out_clipuv->x <= 1.0f && "lerp_vert_attributes: Invalid interpolated u");

	uv0 = uv_arr[p0i].y * w_arr[p0i];
	uv1 = uv_arr[p1i].y * w_arr[p1i];
	out_clipuv->y = (uv0 + (uv1 - uv0) * weight) / interp_w;
	assert(out_clipuv->y >= 0.0f && out_clipuv->y <= 1.0f && "lerp_vert_attributes: Invalid interpolated v");
}

/* Handles viewport and guard-band clipping.
 * Returns true when the triangle(s) should be rasterized, 
 * false if it/they can be discarded. */
bool clip(struct vec2_int *work_poly, float *work_z, float *work_w, struct vec2_float *work_uv, 
	unsigned int *work_vert_count, unsigned int *work_index_count, unsigned int *work_poly_indices,
	const struct vec2_int *target_min, const struct vec2_int *target_max)
{
	assert(work_poly && "clip: work_poly is NULL");
	assert(work_z && "clip: work_z is NULL");
	assert(work_w && "clip: work_w is NULL");
	assert(work_uv && "clip: work_uv is NULL");
	assert(work_vert_count && "clip: work_vert_count is NULL");
	assert(work_index_count && "clip: work_index_count is NULL");
	assert(work_poly_indices && "clip: work_poly_indices is NULL");
	assert(target_min && "clip: target_min is NULL");
	assert(target_max && "clip: target_max is NULL");
	assert(target_min->x < target_max->x && target_min->y < target_max->y && "clip: target_min must be smaller than target_max");

	/* Test view port x, y clipping */
	uint32_t oc0 = compute_out_code(&work_poly[0], target_min->x, target_min->y, target_max->x, target_max->y);
	uint32_t oc1 = compute_out_code(&work_poly[1], target_min->x, target_min->y, target_max->x, target_max->y);
	uint32_t oc2 = compute_out_code(&work_poly[2], target_min->x, target_min->y, target_max->x, target_max->y);
	if ((oc0 | oc1 | oc2) == 0)
	{
		/* Whole poly inside the view, trivially accept */
		return true;
	}
	else if (oc0 & oc1 & oc2)
	{
		/* All points in the same outside region, trivially reject */
		return false;
	}
	else
	{
		/* Test guard band clipping */
		const int32_t sub_multip = 1 << SUB_BITS;
		const int32_t minx = TO_FIXED(GB_MIN, sub_multip);
		const int32_t miny = TO_FIXED(GB_MIN, sub_multip);
		const int32_t maxx = TO_FIXED(GB_MAX, sub_multip);
		const int32_t maxy = TO_FIXED(GB_MAX, sub_multip);

		oc0 = compute_out_code(&work_poly[0], minx, miny, maxx, maxy);
		oc1 = compute_out_code(&work_poly[1], minx, miny, maxx, maxy);
		oc2 = compute_out_code(&work_poly[2], minx, miny, maxx, maxy);

		if ((oc0 | oc1 | oc2) == 0)
		{
			/* Whole poly inside the guard band, trivially accept */
			return true;
		}
		else if (oc0 & oc1 & oc2)
		{
			/* Partially inside the view port and not inside the guard band?? */
			assert(false && "clip: Poly can't be partailly in view and wholly outside the guard band simultaneously.");
			return false;
		}
		else
		{
			/* Sutherland–Hodgman algorithm */

			/* We use indices to create a closed convex polygon.
			* Closed as in same first and last vertex. */
			*work_index_count = 4;
			work_poly_indices[3] = 0;

			unsigned int clipped_vert_count = 0;
			struct vec2_int clipped_poly[7];
			float clipped_z[7];
			float clipped_w[7];
			struct vec2_float clipped_uv[7];
			unsigned int clipped_index_count = 0;
			unsigned int clipped_poly_indices[15];

			/* Loop all edges */
			unsigned int oc_codes[4] = { OC_LEFT, OC_BOTTOM, OC_RIGHT, OC_TOP };
			for (unsigned int oc_index = 0; oc_index < 4; ++oc_index)
			{
				unsigned int oc = oc_codes[oc_index];
				clipped_vert_count = 0;
				clipped_index_count = 0;
				for (unsigned int vert_index = 0; vert_index < *work_index_count - 1; ++vert_index)
				{
					if ((compute_out_code(&work_poly[work_poly_indices[vert_index]], minx, miny, maxx, maxy) & oc) == 0)
					{
						if ((compute_out_code(&work_poly[work_poly_indices[vert_index + 1]], minx, miny, maxx, maxy) & oc) == 0)
						{
							/* both inside, add second */
							clipped_poly[clipped_vert_count] = work_poly[work_poly_indices[vert_index + 1]];
							clipped_z[clipped_vert_count] = work_z[work_poly_indices[vert_index + 1]];
							clipped_w[clipped_vert_count] = work_w[work_poly_indices[vert_index + 1]];
							clipped_uv[clipped_vert_count] = work_uv[work_poly_indices[vert_index + 1]];
							clipped_poly_indices[clipped_index_count] = clipped_vert_count;
							++clipped_vert_count;
							++clipped_index_count;
						}
						else
						{
							/* second out, add intersection */
							clipped_poly[clipped_vert_count] = get_gb_intersection_point(oc, &work_poly[work_poly_indices[vert_index]], &work_poly[work_poly_indices[vert_index + 1]]);
							clipped_poly_indices[clipped_index_count] = clipped_vert_count;

							lerp_vert_attributes(&work_poly[0], &work_z[0], &work_w[0], &work_uv[0], work_poly_indices[vert_index], work_poly_indices[vert_index + 1]
								, &clipped_poly[clipped_vert_count], &clipped_z[clipped_vert_count], &clipped_w[clipped_vert_count], &clipped_uv[clipped_vert_count]);

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

							lerp_vert_attributes(&work_poly[0], &work_z[0], &work_w[0], &work_uv[0], work_poly_indices[vert_index + 1], work_poly_indices[vert_index]
								, &clipped_poly[clipped_vert_count], &clipped_z[clipped_vert_count], &clipped_w[clipped_vert_count], &clipped_uv[clipped_vert_count]);

							++clipped_vert_count;
							++clipped_index_count;

							clipped_poly[clipped_vert_count] = work_poly[work_poly_indices[vert_index + 1]];
							clipped_z[clipped_vert_count] = work_z[work_poly_indices[vert_index + 1]];
							clipped_w[clipped_vert_count] = work_w[work_poly_indices[vert_index + 1]];
							clipped_uv[clipped_vert_count] = work_uv[work_poly_indices[vert_index + 1]];
							clipped_poly_indices[clipped_index_count] = clipped_vert_count;
							++clipped_vert_count;
							++clipped_index_count;
						}
					}
				}
				/* Make sure we have a closed poly */
				clipped_poly_indices[clipped_index_count] = 0;
				++clipped_index_count;

				assert(clipped_vert_count < 7 && "clip: too many vertices in the clipped poly");

				*work_vert_count = clipped_vert_count;
				for (unsigned int j = 0; j < *work_vert_count; ++j)
				{
					work_poly[j] = clipped_poly[j];
					work_z[j] = clipped_z[j];
					work_w[j] = clipped_w[j];
					work_uv[j] = clipped_uv[j];
				}

				*work_index_count = clipped_index_count;
				for (unsigned int j = 0; j < *work_index_count; ++j)
					work_poly_indices[j] = clipped_poly_indices[j];
			}

			--(*work_index_count);

			/* Need to generate proper index list */
			unsigned int temp_ind_buf[15];
			unsigned int temp_ind_count = 0;
			for (unsigned int vert_index = 1; vert_index < *work_index_count - 1; ++vert_index)
			{
				temp_ind_buf[temp_ind_count] = work_poly_indices[0];
				temp_ind_buf[temp_ind_count + 1] = work_poly_indices[vert_index];
				temp_ind_buf[temp_ind_count + 2] = work_poly_indices[vert_index + 1];
				temp_ind_count += 3;
			}
			for (unsigned int j = 0; j < temp_ind_count; ++j)
				work_poly_indices[j] = temp_ind_buf[j];

			*work_index_count = temp_ind_count;
			assert(*work_index_count < 15 && "clip: too many indices in the clipped poly");
			assert(*work_index_count % 3 == 0 && "clip: clipped index count is invalid");

			return true;
		}
	}
}

void rasterizer_rasterize(uint32_t *render_target, uint32_t *depth_buf, const struct vec2_int *target_size, const struct vec2_int *rasterize_area_min, const struct vec2_int *rasterize_area_max,
	const struct vec4_float *vert_buf, const struct vec2_float *uv_buf, const unsigned int *ind_buf, const unsigned int index_count, 
	const uint32_t *texture, const struct vec2_int *texture_size)
{
	assert(render_target && "rasterizer_rasterize: render_target is NULL");
	assert(depth_buf && "rasterizer_rasterize: depth_buf is NULL");
	assert(target_size && "rasterizer_rasterize: target_size is NULL");
	assert(rasterize_area_min && "rasterizer_rasterize: rasterize_area_min is NULL");
	assert(rasterize_area_max && "rasterizer_rasterize: rasterize_area_max is NULL");
	assert(vert_buf && "rasterizer_rasterize: vert_buf is NULL");
	assert(uv_buf && "rasterizer_rasterize: uv_buf is NULL");
	assert(ind_buf && "rasterizer_rasterize: ind_buf is NULL");
	assert(texture && "rasterizer_rasterize: texture is NULL");
	assert(texture_size && "rasterizer_rasterize: texture_size is NULL");
	assert(index_count % 3 == 0 && "rasterizer_rasterize: index count is not valid");
	assert(SUB_BITS == 4 && "rasterizer_rasterize: SUB_BITS has changed, check the assert below.");
	assert(target_size->x <= (2 * -(GB_MIN)) && target_size->y <= (2 * -(GB_MIN)) && "rasterizer_rasterize: render target is too large");
	assert(rasterize_area_min->x >= 0 && rasterize_area_min->y >= 0 && "rasterizer_rasterize: invalid rasterize_area_min");
#ifndef USE_TILES
	assert(rasterize_area_max->x < target_size->x && rasterize_area_max->y < target_size->y && "rasterizer_rasterize: invalid rasterize_are_max");
#endif
	assert(rasterize_area_min->x < rasterize_area_max->x && rasterize_area_min->y < rasterize_area_max->y && "rasterizer_rasterize: rasterize_area_min must be smaller than rasterize_area_max");
#ifdef USE_SIMD
	assert(rasterize_area_min->x % 2 == 0 && "rasterizer_rasterize: rasterize_area_min must be even");
	assert(rasterize_area_min->y % 2 == 0 && "rasterizer_rasterize: rasterize_area_min must be even");
	assert(rasterize_area_max->x % 2 == 1 && "rasterizer_rasterize: rasterize_area_max must be odd");
	assert(rasterize_area_max->y % 2 == 1 && "rasterizer_rasterize: rasterize_area_max must be odd");
#ifdef USE_TILES
	assert(rasterize_area_min->x % TILE_SIZE == 0 && "rasterizer_rasterize: When using tiles rasterize areas must be aligned to tiles.");
	assert(rasterize_area_min->y % TILE_SIZE == 0 && "rasterizer_rasterize: When using tiles rasterize areas must be aligned to tiles.");
	assert(rasterize_area_max->x - rasterize_area_min->x == TILE_SIZE - 1 && "rasterizer_rasterize: When using tiles rasterize areas must be tile sized.");
	assert(rasterize_area_max->y - rasterize_area_min->y == TILE_SIZE - 1 && "rasterizer_rasterize: When using tiles rasterize areas must be tile sized.");
#endif
#endif

	/* Sub-pixel constants */
	const int32_t sub_multip = 1 << SUB_BITS;
	const int32_t half_pixel = sub_multip >> 1;
	const int32_t sub_mask = sub_multip - 1;

	/* Reserve enough space for possible polys created by clipping */
	struct vec2_int work_poly[7];
	float work_z[7];
	float work_w[7]; /* Note that this is actually the reciprocal of w */
	struct vec2_float work_uv[7];
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

		const int32_t half_width = target_size->x / 2;
		const int32_t half_height = target_size->y / 2;

		struct vec2_int rast_min;
		rast_min.x = TO_FIXED(rasterize_area_min->x - half_width, sub_multip);
		rast_min.y = TO_FIXED(rasterize_area_min->y - half_height, sub_multip);
		struct vec2_int rast_max;
		rast_max.x = TO_FIXED(rasterize_area_max->x - half_width, sub_multip);
		rast_max.y = TO_FIXED(rasterize_area_max->y - half_height, sub_multip);

		work_poly[0].x = TO_FIXED(vert_buf[ind_buf[i]].x / vert_buf[ind_buf[i]].w * half_width, sub_multip);
		work_poly[0].y = TO_FIXED(vert_buf[ind_buf[i]].y / vert_buf[ind_buf[i]].w * half_height, sub_multip);
		work_z[0] = vert_buf[ind_buf[i]].z / vert_buf[ind_buf[i]].w;
		work_w[0] = 1.0f / vert_buf[ind_buf[i]].w;
		work_poly[1].x = TO_FIXED(vert_buf[ind_buf[i + 1]].x / vert_buf[ind_buf[i + 1]].w * half_width, sub_multip);
		work_poly[1].y = TO_FIXED(vert_buf[ind_buf[i + 1]].y / vert_buf[ind_buf[i + 1]].w * half_height, sub_multip);
		work_z[1] = vert_buf[ind_buf[i + 1]].z / vert_buf[ind_buf[i + 1]].w;
		work_w[1] = 1.0f / vert_buf[ind_buf[i + 1]].w;
		work_poly[2].x = TO_FIXED(vert_buf[ind_buf[i + 2]].x / vert_buf[ind_buf[i + 2]].w * half_width, sub_multip);
		work_poly[2].y = TO_FIXED(vert_buf[ind_buf[i + 2]].y / vert_buf[ind_buf[i + 2]].w * half_height, sub_multip);
		work_z[2] = vert_buf[ind_buf[i + 2]].z / vert_buf[ind_buf[i + 2]].w;
		work_w[2] = 1.0f / vert_buf[ind_buf[i + 2]].w;
		work_poly_indices[0] = 0; work_poly_indices[1] = 1; work_poly_indices[2] = 2;
		work_vert_count = 3;
		work_index_count = 3;
		
		work_uv[0] = uv_buf[ind_buf[i]];
		work_uv[1] = uv_buf[ind_buf[i + 1]];
		work_uv[2] = uv_buf[ind_buf[i + 2]];

		if (!clip(&(work_poly[0]), &(work_z[0]), &(work_w[0]), &(work_uv[0]),
			&work_vert_count, &work_index_count, &(work_poly_indices[0]), &rast_min, &rast_max))
			continue;

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
#ifdef USE_SIMD
			min.x = ((max(min.x, rast_min.x) & ~sub_mask) & ~sub_multip) + half_pixel;
			min.y = ((max(min.y, rast_min.y) & ~sub_mask) & ~sub_multip) + half_pixel;
			max.x = ((min(max.x, rast_max.x) & ~sub_mask) | sub_multip) + half_pixel;
			max.y = ((min(max.y, rast_max.y) & ~sub_mask) | sub_multip) + half_pixel;
#else
			min.x = (max(min.x, rast_min.x) & ~sub_mask) + half_pixel;
			min.y = (max(min.y, rast_min.y) & ~sub_mask) + half_pixel;
			max.x = (min(max.x, rast_max.x) & ~sub_mask) + half_pixel;
			max.y = (min(max.y, rast_max.y) & ~sub_mask) + half_pixel;
#endif

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

#ifdef USE_SIMD
			float temp = work_uv[i0].x * work_w[i0];
			const __m128 uv0x = _mm_set_ps(temp, temp, temp, temp);
			temp = work_uv[i0].y * work_w[i0];
			const __m128 uv0y = _mm_set_ps(temp, temp, temp, temp);
			temp = work_uv[i1].x * work_w[i1] - work_uv[i0].x * work_w[i0];
			const __m128 uv10x = _mm_set_ps(temp, temp, temp, temp); 
			temp = work_uv[i1].y * work_w[i1] - work_uv[i0].y * work_w[i0];
			const __m128 uv10y = _mm_set_ps(temp, temp, temp, temp);
			temp = work_uv[i2].x * work_w[i2] - work_uv[i0].x * work_w[i0];
			const __m128 uv20x = _mm_set_ps(temp, temp, temp, temp); 
			temp = work_uv[i2].y * work_w[i2] - work_uv[i0].y * work_w[i0];
			const __m128 uv20y = _mm_set_ps(temp, temp, temp, temp);

			temp = 1.0f / (float)winding_2d(&work_poly[i0], &work_poly[i1], &work_poly[i2]);
			const __m128 one_over_double_area = _mm_set_ps(temp, temp, temp, temp);
			temp = work_z[i1] - work_z[i0];
			const __m128 z10 = _mm_set_ps(temp, temp, temp, temp);
			temp = work_z[i2] - work_z[i0];
			const __m128 z20 = _mm_set_ps(temp, temp, temp, temp);

			temp = (float)(texture_size->x - 1);
			const __m128 tex_coor_x_max = _mm_set_ps(temp, temp, temp, temp);
			temp = (float)(texture_size->y - 1);
			const __m128 tex_coor_y_max = _mm_set_ps(temp, temp, temp, temp);

#ifdef USE_TILES
			unsigned int pixel_index_row;
			{
				struct vec2_int padded_size;
				rasterizer_get_padded_size(target_size, &padded_size);
				pixel_index_row = TILE_SIZE * TILE_SIZE * 
					((padded_size.x / TILE_SIZE) * (rasterize_area_min->y / TILE_SIZE) + (rasterize_area_min->x / TILE_SIZE)); /* tile index */
				pixel_index_row += TILE_SIZE 
					* ((((min.y - half_pixel) / sub_multip) + half_height) - rasterize_area_min->y) /* y */
					+ (((((min.x - half_pixel) / sub_multip) + half_width) - rasterize_area_min->x) * 2); /* x */

			}
#else
			unsigned int pixel_index_row = target_size->x
				* (((min.y - half_pixel) / sub_multip) + half_height) /* y */
				+ ((((min.x - half_pixel) / sub_multip) + half_width) * 2); /* x */
#endif

			const __m128i step_size = _mm_set_epi32(2 * sub_multip, 2 * sub_multip, 2 * sub_multip, 2 * sub_multip);
			const __m128i xor_mask = _mm_set_epi32(~(uint32_t)0, ~(uint32_t)0, ~(uint32_t)0, ~(uint32_t)0);
			const __m128 work_w0 = _mm_set_ps(work_w[i0], work_w[i0], work_w[i0], work_w[i0]);
			const __m128 work_w1 = _mm_set_ps(work_w[i1], work_w[i1], work_w[i1], work_w[i1]);
			const __m128 work_w2 = _mm_set_ps(work_w[i2], work_w[i2], work_w[i2], work_w[i2]);

			/* Rasterize */
			__m128i point_x;
			__m128i point_y = _mm_set_epi32(min.y + sub_multip, min.y + sub_multip, min.y, min.y);
			for (; ((int32_t *)&point_y)[0] <= max.y; point_y = _mm_add_epi32(point_y, step_size))
			{
				__m128i w0 = _mm_set_epi32(w0_row + step_y_12 + step_x_12, w0_row + step_y_12, w0_row + step_x_12, w0_row);
				__m128i w1 = _mm_set_epi32(w1_row + step_y_20 + step_x_20, w1_row + step_y_20, w1_row + step_x_20, w1_row);
				__m128i w2 = _mm_set_epi32(w2_row + step_y_01 + step_x_01, w2_row + step_y_01, w2_row + step_x_01, w2_row);

				uint32_t pixel_index_start = pixel_index_row;

				point_x = _mm_set_epi32(min.x + sub_multip, min.x, min.x + sub_multip, min.x);
				for (; ((int32_t *)&point_x)[0] <= max.x; point_x = _mm_add_epi32(point_x, step_size))
				{
					__m128i mask = _mm_or_si128(w0, w1);
					mask = _mm_or_si128(mask, w2);

					/* Compare for less than zero
					* (a0 < b0) ? 0xffffffff : 0x0
					* if anything is >= 0 then there will be 0x0 bytes */
					__m128i temp_mask = _mm_cmplt_epi32(mask, _mm_setzero_si128());
					/* Invert with xor */
					mask = _mm_xor_si128(xor_mask, temp_mask);
					/* Or all bits and check if any were set */
					if (_mm_movemask_epi8(mask) != 0)
					{
						__m128 one = _mm_set_ps(1.0f, 1.0f, 1.0f, 1.0f);
						__m128 w0_f = _mm_min_ps(_mm_mul_ps(_mm_cvtepi32_ps(w0), one_over_double_area), one);
						__m128 w1_f = _mm_min_ps(_mm_mul_ps(_mm_cvtepi32_ps(w1), one_over_double_area), one);
						__m128 w2_f = _mm_max_ps(_mm_sub_ps(_mm_sub_ps(one, w0_f), w1_f), _mm_setzero_ps());

						__m128i z = _mm_cvttps_epi32(
							_mm_mul_ps(_mm_set_ps((1 << DEPTH_BITS), (1 << DEPTH_BITS), (1 << DEPTH_BITS), (1 << DEPTH_BITS)),
								_mm_add_ps(_mm_set_ps(work_z[i0], work_z[i0], work_z[i0], work_z[i0]),
									_mm_add_ps(_mm_mul_ps(w1_f, z10), _mm_mul_ps(w2_f, z20)))));

						/* force the buffer to be aligned and change the load to _mm_load_si128 */
						__m128i depth = _mm_loadu_si128((const __m128i *)&depth_buf[pixel_index_start]);

						__m128i depth_mask = _mm_set_epi32(0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff);
						temp_mask = _mm_cmplt_epi32(z, _mm_and_si128(depth, depth_mask));
						mask = _mm_and_si128(mask, temp_mask);

						if (_mm_movemask_epi8(mask) != 0x0)
						{
							__m128 interp_w = _mm_add_ps(_mm_add_ps(
								_mm_mul_ps(work_w0, w0_f),
								_mm_mul_ps(work_w1, w1_f)),
								_mm_mul_ps(work_w2, w2_f));

							__m128 u = _mm_div_ps(
								_mm_add_ps(uv0x,
									_mm_add_ps(_mm_mul_ps(w1_f, uv10x),
										_mm_mul_ps(w2_f, uv20x))), interp_w);

							__m128 v = _mm_div_ps(
								_mm_add_ps(uv0y,
									_mm_add_ps(_mm_mul_ps(w1_f, uv10y),
										_mm_mul_ps(w2_f, uv20y))), interp_w);

							__m128i texture_index = mul_epi32(_mm_cvttps_epi32(_mm_mul_ps(tex_coor_y_max, v)), _mm_set_epi32(texture_size->x, texture_size->x, texture_size->x, texture_size->x));
							texture_index = _mm_add_epi32(texture_index, _mm_cvttps_epi32(_mm_mul_ps(tex_coor_x_max, u)));

							for (unsigned int pixel = 0; pixel < 4; ++pixel)
							{
								if (((int32_t *)&mask)[pixel] == 0)
									continue;
#ifdef USE_TILES
#if RPLNN_BUILD_TYPE == RPLNN_DEBUG
								struct vec2_int padded_size;
								rasterizer_get_padded_size(target_size, &padded_size);
								assert(pixel_index_start + pixel < (unsigned)(padded_size.x * padded_size.y) && "rasterizer_rasterize: invalid pixel_index");
#endif
#else
								assert(pixel_index_start + pixel < (unsigned)(target_size->x * target_size->y) && "rasterizer_rasterize: invalid pixel_index");
#endif
								assert(((uint32_t *)&texture_index)[pixel] < (unsigned)(texture_size->x * texture_size->y) && "rasterizer_rasterize: invalid texture_index");

								/* There must be a better way to do this */
								depth_buf[pixel_index_start + pixel] = ((int32_t *)&z)[pixel];
								/* Mipmapping should help with this,
								 * currently especially small triangles can cause cache misses
								 * by accessing the texture in the opposite ends of the array.*/
								render_target[pixel_index_start + pixel] = texture[((int32_t *)&texture_index)[pixel]];
							}
						}
					}
					uint32_t double_step = step_x_12 * 2;
					w0 = _mm_add_epi32(w0, _mm_set_epi32(double_step, double_step, double_step, double_step));
					double_step = step_x_20 * 2;
					w1 = _mm_add_epi32(w1, _mm_set_epi32(double_step, double_step, double_step, double_step));
					double_step = step_x_01 * 2;
					w2 = _mm_add_epi32(w2, _mm_set_epi32(double_step, double_step, double_step, double_step));

					pixel_index_start += 4;
				}

				w0_row += step_y_12 * 2;
				w1_row += step_y_20 * 2;
				w2_row += step_y_01 * 2;
#ifdef USE_TILES
				pixel_index_row += TILE_SIZE * 2;
#else
				pixel_index_row += target_size->x * 2;
#endif
			}
#else
			struct vec2_float uv0;
			uv0.x = work_uv[i0].x * work_w[i0]; uv0.y = work_uv[i0].y * work_w[i0];
			struct vec2_float uv10;
			uv10.x = work_uv[i1].x * work_w[i1] - work_uv[i0].x * work_w[i0];
			uv10.y = work_uv[i1].y * work_w[i1] - work_uv[i0].y * work_w[i0];
			struct vec2_float uv20;
			uv20.x = work_uv[i2].x * work_w[i2] - work_uv[i0].x * work_w[i0];
			uv20.y = work_uv[i2].y * work_w[i2] - work_uv[i0].y * work_w[i0];

			float z10 = work_z[i1] - work_z[i0];
			float z20 = work_z[i2] - work_z[i0];
			
			float one_over_double_area = 1.0f / (float)winding_2d(&work_poly[i0], &work_poly[i1], &work_poly[i2]);

			const float tex_coor_x_max = (float)(texture_size->x - 1);
			const float tex_coor_y_max = (float)(texture_size->y - 1);

			unsigned int pixel_index_row = target_size->x
				* (((min.y - half_pixel) / sub_multip) + half_height) /* y */
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
						float w0_f = min((float)w0 * one_over_double_area, 1.0f);
						float w1_f = min((float)w1 * one_over_double_area, 1.0f);
						float w2_f = max(1.0f - w0_f - w1_f, 0.0f);

						uint32_t z = (uint32_t)((work_z[i0] + (w1_f * z10) + (w2_f * z20)) * (1 << DEPTH_BITS));
						assert(z < ((1 << DEPTH_BITS) + 1) && "rasterizer_rasterize: z value is too large");
						if (z < depth_buf[pixel_index])
						{
							depth_buf[pixel_index] = z;

							float interp_w = work_w[i0] * w0_f + work_w[i1] * w1_f + work_w[i2] * w2_f;
							float u = uv0.x + (w1_f * uv10.x) + (w2_f * uv20.x);
							u /= interp_w;
							float v = uv0.y + (w1_f * uv10.y) + (w2_f * uv20.y);
							v /= interp_w;

							const unsigned int texture_index = (unsigned)(tex_coor_y_max * v) * (unsigned)texture_size->x
								+ (unsigned)(tex_coor_x_max * u);
							assert(pixel_index < (unsigned)(target_size->x * target_size->y) && "rasterizer_rasterize: invalid pixel_index");
							assert(texture_index < (unsigned)(texture_size->x * texture_size->y) && "rasterizer_rasterize: invalid texture_index");
							render_target[pixel_index] = texture[texture_index];
						}
					}

					w0 += step_x_12;
					w1 += step_x_20;
					w2 += step_x_01;

					++pixel_index;
				}

				w0_row += step_y_12;
				w1_row += step_y_20;
				w2_row += step_y_01;

				pixel_index_row += target_size->x;
			}
#endif		
		}
	}
}

void rasterizer_clear_depth_buffer(uint32_t *depth_buf, const struct vec2_int *buf_size)
{
	assert(depth_buf && "rasterizer_clear_depth_buffer: depth_buf is NULL");
	assert(buf_size && "rasterizer_clear_depth_buffer: buf_size is NULL");

	for (int i = 0; i < buf_size->x * buf_size->y; ++i)
		depth_buf[i] |= 0x00FFFFFF;
}

bool rasterizer_uses_simd(void)
{
#ifdef USE_SIMD
	return true;
#else
	return false;
#endif
}

bool rasterizer_uses_tiles(void)
{
#ifdef USE_TILES
	return true;
#else
	return false;
#endif
}

uint32_t rasterizer_get_tile_size(void)
{
	return TILE_SIZE;
}

void rasterizer_get_padded_size(const struct vec2_int *target_size, struct vec2_int *out_padded_size)
{
	assert(target_size && "rasterizer_get_padded_size: target_size is NULL");
	assert(out_padded_size && "rasterizer_get_padded_size: out_padded_size is NULL");

	out_padded_size->x = target_size->x;
	out_padded_size->y = target_size->y;
#ifdef USE_TILES
	if ((target_size->x % TILE_SIZE) != 0)
		out_padded_size->x += TILE_SIZE - (target_size->x % TILE_SIZE);
	if ((target_size->y % TILE_SIZE) != 0)
		out_padded_size->y += TILE_SIZE - (target_size->y % TILE_SIZE);
#endif
}