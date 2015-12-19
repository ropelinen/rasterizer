#include "software_rasterizer/precompiled.h"

#include "rasterizer.h"

#include "software_rasterizer/vector.h"

/* Taken straight from https://fgiesen.wordpress.com/2013/02/08/triangle-rasterization-in-practice/ */
int orient2d(const struct vec2_int *p1, const struct vec2_int *p2, const struct vec2_int *p3)
{
	assert(p1 && "orient2d: p1 is NULL");
	assert(p2 && "orient2d: p2 is NULL");
	assert(p3 && "orient2d: p3 is NULL");

	return (p2->x - p1->x)*(p3->y - p1->y) - (p2->y - p1->y)*(p3->x - p1->x);
}

void rasterizer_rasterize_triangle(uint32_t *render_target, const struct vec2_int *target_size, const struct vec3_float *vert_buf, const unsigned int *ind_buf, const unsigned int index_count)
{
	assert(render_target && "rasterizer_rasterize_triangle: render_target is NULL");
	assert(target_size && "rasterizer_rasterize_triangle: target_size is NULL");
	assert(vert_buf && "rasterizer_rasterize_triangle: vert_buf is NULL");
	assert(ind_buf && "rasterizer_rasterize_triangle: ind_buf is NULL");
	assert(index_count % 3 == 0 && "rasterizer_rasterize_triangle: index count is not valid");

	/* Bounding box */
	struct vec2_int min;
	struct vec2_int max;

	for (unsigned int i = 0; i < index_count; i += 3)
	{
		/* Very crude, needs sub-pixel precision */
		struct vec2_int p1;
		p1.x = (int)vert_buf[ind_buf[i]].x;
		p1.y = (int)vert_buf[ind_buf[i]].y;
		struct vec2_int p2;
		p2.x = (int)vert_buf[ind_buf[i + 1]].x;
		p2.y = (int)vert_buf[ind_buf[i + 1]].y;
		struct vec2_int p3;
		p3.x = (int)vert_buf[ind_buf[i + 2]].x;
		p3.y = (int)vert_buf[ind_buf[i + 2]].y;

		min.x = min3(p1.x, p2.x, p3.x);
		min.y = min3(p1.y, p2.y, p3.y);
		max.x = max3(p1.x, p2.x, p3.x);
		max.y = max3(p1.y, p2.y, p3.y);

		/* Clip to screen */
		int half_width = target_size->x / 2;
		int half_height = target_size->y / 2;
		min.x = max(min.x, -half_height);
		min.y = max(min.y, -half_width);
		max.x = min(max.x, target_size->x / 2 - 1);
		max.y = min(max.y, target_size->y / 2 - 1);

		/* Rasterize */
		struct vec2_int point;
		for (point.y = min.y; point.y <= max.y; ++point.y)
		{
			for (point.x = min.x; point.x <= max.x; ++point.x)
			{
				/* Calculate barysentric coords */
				int w0 = orient2d(&p1, &p2, &point);
				int w1 = orient2d(&p2, &p3, &point);
				int w2 = orient2d(&p3, &p1, &point);

				if (w0 >= 0 && w1 >= 0 && w2 >= 0)
				{
					/* Render pixel, totally hax btw, should do renderer specifix pixel setting */
					/* 0,0 at center of the screen, positive to up and right */
					render_target[target_size->x * (-point.y + half_height) + point.x + half_width] = 0x0000FF;
				}
			}
		}
	}
}