#include "software_rasterizer/precompiled.h"

#include "software_rasterizer/demo/font.h"
#include "software_rasterizer/demo/osal.h"
#include "software_rasterizer/demo/stats.h"
#include "software_rasterizer/demo/texture.h"
#include "software_rasterizer/rasterizer.h"

#define USE_THREADING 1
#define VERTS_IN_BOX 14
#define LARGE_VERT_BUF_BOXES 8

#ifdef USE_THREADING
struct thread_data
{
	uint32_t *render_target;
	uint32_t *depth_buffer;
	struct vec2_int target_size;
	struct vec2_int *raster_area_mins;
	struct vec2_int *raster_area_maxs;
	uint32_t raster_area_count;
	struct vec4_float **vert_bufs;
	struct vec2_float **uv_bufs;
	unsigned int **ind_bufs;
	unsigned int *ind_counts;
	uint32_t **textures;
	struct vec2_int **texture_sizes;
	uint32_t buffer_count;
};

void thread_data_init(struct thread_data *data, const uint32_t buffer_count, const uint32_t raster_area_count);
void thread_data_deinit(struct thread_data *data);
void thread_data_calculate_areas(struct thread_data *data, const unsigned int core_count, const struct vec2_int *backbuffer_size);
void rasterize_thread(void *data);
#endif

void transform_vertices(const struct vec3_float *in_verts, struct vec4_float *out_verts, unsigned int vert_count,
                     struct matrix_3x4 *translation, struct matrix_3x4 *rotation, struct matrix_4x4 *camera_projection);
void handle_input(struct api_info *api_info, float dt, struct vec3_float *camera_trans);

void create_box_buffers(struct vec3_float *out_vert_buf, struct vec2_float *out_uv_buf, unsigned int *out_ind_buf);
void generate_large_test_buffers(const struct vec3_float *vert_buf_box, const struct vec2_float *uv_box, const unsigned int *ind_buf_box,
	struct vec3_float *out_vert_buf, struct vec2_float *out_uv, unsigned int *out_ind_buf, const struct vec3_float *box_offsets, const unsigned int box_count_out);

void render_stats(struct stats *stats, struct font *font, void *render_target, struct vec2_int *target_size);
void render_stat_line_ms(struct stats *stats, struct font *font, void *render_target, struct vec2_int *target_size, 
                         const char *stat_name, const unsigned char stat_id, const int row_y, const int stat_name_x, const int first_val_x, const int x_increment);
void render_stat_line_mus(struct stats *stats, struct font *font, void *render_target, struct vec2_int *target_size,
                          const char *stat_name, const unsigned char stat_id, const int row_y, const int stat_name_x, const int first_val_x, const int x_increment);

/* A generic platform independent main function.
 * See osal.c for platform specific main. */
void main(struct api_info *api_info, struct renderer_info *renderer_info)
{
	assert(api_info && "main: api_info is NULL");
	assert(renderer_info && "main: renderer_info is NULL");

	struct texture *texture = texture_create("crate.png");
	if (!texture)
		error_popup("Failed to load the texture", true);

	uint32_t *texture_data = NULL;
	struct vec2_int *texture_size = NULL;

	texture_get_info(texture, &texture_data, &texture_size);
	if (!texture_data || !texture_size)
		error_popup("Couldn't get texture info", true);

	struct font *font = font_create("Tuffy.ttf");
	if (font)
		font_set_line_height(font, 19);
	else
		error_popup("Failed to initialize the font", false);

	struct stats *stats = stats_create(STAT_COUNT, 1000, true);
	unsigned int stabilizing_delay = 500;

	/* Test box, CCW */
	struct vec3_float vert_buf[VERTS_IN_BOX];
	struct vec2_float uv[VERTS_IN_BOX];
	unsigned int ind_buf[36];
	const unsigned int ind_buf_size = sizeof(ind_buf) / sizeof(ind_buf[0]);
	create_box_buffers(&(vert_buf[0]), &(uv[0]), &(ind_buf[0]));

	struct vec3_float vert_buf_large[VERTS_IN_BOX * LARGE_VERT_BUF_BOXES];
	struct vec2_float uv_large[VERTS_IN_BOX * LARGE_VERT_BUF_BOXES];
	unsigned int ind_buf_large[36 * LARGE_VERT_BUF_BOXES];
	const unsigned int ind_buf_large_size = sizeof(ind_buf_large) / sizeof(ind_buf_large[0]);
	struct vec3_float box_offsets[LARGE_VERT_BUF_BOXES] = { { .x = -6.0f, .y = 6.0f, .z = 6.0f }, { .x = 2.0f, .y = 4.0f, .z = 6.0f }, { .x = -2.0f, .y = -2.0f, .z = 2.0f }, { .x = 0.0f, .y = 0.0f, .z = 0.0f },
	                                                        { .x = -6.0f, .y = 2.0f, .z = -2.0f }, { .x = -4.0f, .y = -4.0f, .z = -8.0f }, { .x = 4.0f, .y = 6.0f, .z = -6.0f }, { .x = 2.0f, .y = 10.0f, .z = -8.0f } };
	generate_large_test_buffers(&vert_buf[0], &uv[0], &ind_buf[0], &vert_buf_large[0], &uv_large[0], &ind_buf_large[0], &box_offsets[0], LARGE_VERT_BUF_BOXES);

	struct vec4_float final_vert_buf[VERTS_IN_BOX];
	struct vec4_float final_vert_buf2[VERTS_IN_BOX];
	struct vec4_float final_vert_buf_large[VERTS_IN_BOX * LARGE_VERT_BUF_BOXES];
	struct vec4_float final_vert_buf_large2[VERTS_IN_BOX * LARGE_VERT_BUF_BOXES];
	struct vec4_float final_vert_buf_large3[VERTS_IN_BOX * LARGE_VERT_BUF_BOXES];
	
	/* Transfrom related */
	struct vec3_float translation = { .x = -5.5f, .y = -8.0f, .z = 10.0f };
	struct vec3_float camera_trans = { .x = 2.0f, .y = -4.0f, .z = 0.0f };
	struct matrix_3x4 trans_mat = mat34_get_translation(&translation);
	translation.x += 2.0f; translation.z += 3.0f;
	struct matrix_3x4 trans_mat2 = mat34_get_translation(&translation);
	translation.x += 2.0f; translation.y += 4.0f; translation.z += 15.0f;
	struct matrix_3x4 trans_mat_large = mat34_get_translation(&translation);
	translation.x += 26.0f; translation.y -= 8.0f; translation.z += 10.0f;
	struct matrix_3x4 trans_mat_large2 = mat34_get_translation(&translation);
	translation.x += 10.0f; translation.y += 18.0f; translation.z += 50.0f;
	struct matrix_3x4 trans_mat_large3 = mat34_get_translation(&translation);

	/* Get correctly sized render target */
	struct vec2_int rendertarget_size = get_backbuffer_size(renderer_info);
	uint32_t *render_target = NULL;
	struct vec2_int padded_size = { .x = 0, .y = 0 };
	if (rasterizer_uses_simd())
	{
		if (rasterizer_uses_tiles())
		{
			rasterizer_get_padded_size(&rendertarget_size, &padded_size);
			render_target = malloc(padded_size.x * padded_size.y * sizeof(uint32_t));
		}
		else
			render_target = malloc(rendertarget_size.x * rendertarget_size.y * sizeof(uint32_t));
	}
	else
		render_target = get_backbuffer(renderer_info);

	if (!render_target)
		error_popup("Couldn't get back buffer", true);

	/* Get correctly sized depth buffer */
	uint32_t *depth_buf = NULL;
	if (rasterizer_uses_simd() && rasterizer_uses_tiles())
		depth_buf = malloc(padded_size.x * padded_size.y * sizeof(uint32_t));
	else
		depth_buf = malloc(rendertarget_size.x * rendertarget_size.y * sizeof(uint32_t));

	struct matrix_4x4 perspective_mat = mat44_get_perspective_lh_fov(DEG_TO_RAD(59.0f), (float)rendertarget_size.x / (float)rendertarget_size.y, 1.0f, 1000.0f);

	uint32_t frame_time_mus = 0;
	uint64_t frame_start = get_time();

	const int32_t tile_size = rasterizer_get_tile_size();
#ifdef USE_THREADING
	const unsigned int core_count = get_logical_core_count();
	struct thread **threads = malloc(sizeof(struct thread *) * core_count);
	struct thread_data *thread_data = malloc(sizeof(struct thread_data) * core_count);

	unsigned int tile_count = (padded_size.x / tile_size) * (padded_size.y / tile_size);
	/* Not a good way to do this.
	 * Should try to pass only the relevant vertex info to the thread.
	 * Just passing every tri to all raster areas is really bad when using small raster areas (tiling enabled).
	 * In case a tri is covering multiple raster areas should be ok to pass it to all of them.
	 * Would be relatively easy if I had the verts in some data structure suitable for occlusion culling. */
	for (unsigned int i = 0; i < core_count; ++i)
	{
		threads[i] = thread_create(i);
		thread_data_init(&thread_data[i], 5, tile_count / core_count + 1);
		thread_data[i].render_target = render_target;
		thread_data[i].depth_buffer = depth_buf;
		thread_data[i].target_size = rendertarget_size;

		thread_data[i].vert_bufs[0] = &final_vert_buf[0];
		thread_data[i].uv_bufs[0] = &uv[0];
		thread_data[i].ind_bufs[0] = &ind_buf[0];
		thread_data[i].ind_counts[0] = ind_buf_size;
		thread_data[i].textures[0] = &texture_data[0];
		thread_data[i].texture_sizes[0] = texture_size;

		thread_data[i].vert_bufs[1] = &final_vert_buf2[0];
		thread_data[i].uv_bufs[1] = &uv[0];
		thread_data[i].ind_bufs[1] = &ind_buf[0];
		thread_data[i].ind_counts[1] = ind_buf_size;
		thread_data[i].textures[1] = &texture_data[0];
		thread_data[i].texture_sizes[1] = texture_size;

		thread_data[i].vert_bufs[2] = &final_vert_buf_large[0];
		thread_data[i].uv_bufs[2] = &uv_large[0];
		thread_data[i].ind_bufs[2] = &ind_buf_large[0];
		thread_data[i].ind_counts[2] = ind_buf_large_size;
		thread_data[i].textures[2] = &texture_data[0];
		thread_data[i].texture_sizes[2] = texture_size;

		thread_data[i].vert_bufs[3] = &final_vert_buf_large2[0];
		thread_data[i].uv_bufs[3] = &uv_large[0];
		thread_data[i].ind_bufs[3] = &ind_buf_large[0];
		thread_data[i].ind_counts[3] = ind_buf_large_size;
		thread_data[i].textures[3] = &texture_data[0];
		thread_data[i].texture_sizes[3] = texture_size;

		thread_data[i].vert_bufs[4] = &final_vert_buf_large3[0];
		thread_data[i].uv_bufs[4] = &uv_large[0];
		thread_data[i].ind_bufs[4] = &ind_buf_large[0];
		thread_data[i].ind_counts[4] = ind_buf_large_size;
		thread_data[i].textures[4] = &texture_data[0];
		thread_data[i].texture_sizes[4] = texture_size;
	}

	thread_data_calculate_areas(thread_data, core_count, &rendertarget_size);
#endif

	while (event_loop())
	{
		if (stabilizing_delay > 0)
			--stabilizing_delay;
		
		if (rasterizer_uses_simd())
		{
			if (rasterizer_uses_tiles())
			{
				for (int i = 0; i < padded_size.x * padded_size.y; ++i)
					((int*)render_target)[i] = 0xFF0000;
			}
			else
			{
				for (int i = 0; i < rendertarget_size.x * rendertarget_size.y; ++i)
					((int*)render_target)[i] = 0xFF0000;
			}
		}
		else
			renderer_clear_backbuffer(renderer_info, 0xFF0000);

		float dt = (float)frame_time_mus / 1000000.0f;

		handle_input(api_info, dt, &camera_trans);

		/* Projection and view*/
		struct matrix_3x4 camera_mat = mat34_get_translation(&camera_trans);
		camera_mat = mat34_get_inverse(&camera_mat);
		struct matrix_4x4 camera_projection = mat44_mul_mat34(&perspective_mat, &camera_mat);

		/* World space */
		struct matrix_3x4 rot_mat = mat34_get_rotation_y(DEG_TO_RAD(40.0f));
		transform_vertices(&(vert_buf[0]), &(final_vert_buf[0]), sizeof(vert_buf) / sizeof(vert_buf[0]), &trans_mat, &rot_mat, &camera_projection);

		rot_mat = mat34_get_rotation_y(DEG_TO_RAD(38.0f));
		transform_vertices(&(vert_buf[0]), &(final_vert_buf2[0]), sizeof(vert_buf) / sizeof(vert_buf[0]), &trans_mat2, &rot_mat, &camera_projection);

		rot_mat = mat34_get_rotation_y(DEG_TO_RAD(30.0f));
		transform_vertices(&(vert_buf_large[0]), &(final_vert_buf_large[0]), sizeof(vert_buf_large) / sizeof(vert_buf_large[0]), &trans_mat_large, &rot_mat, &camera_projection);

		rot_mat = mat34_get_rotation_y(DEG_TO_RAD(-45.0f));
		transform_vertices(&(vert_buf_large[0]), &(final_vert_buf_large2[0]), sizeof(vert_buf_large) / sizeof(vert_buf_large[0]), &trans_mat_large2, &rot_mat, &camera_projection);

		transform_vertices(&(vert_buf_large[0]), &(final_vert_buf_large3[0]), sizeof(vert_buf_large) / sizeof(vert_buf_large[0]), &trans_mat_large3, &rot_mat, &camera_projection);

		if (rasterizer_uses_tiles())
			rasterizer_clear_depth_buffer(depth_buf, &padded_size);
		else
			rasterizer_clear_depth_buffer(depth_buf, &rendertarget_size);

		uint64_t raster_duration = get_time();
#ifdef USE_THREADING
		for (unsigned int i = 0; i < core_count; ++i)
			thread_set_task(threads[i], &rasterize_thread, &thread_data[i]);

		for (unsigned int i = 0; i < core_count; ++i)
			thread_wait_for_task(threads[i]);
#else
		const struct vec2_int area_min = { .x = 0, .y = 0 };
		struct vec2_int area_max;
		area_max.x = rendertarget_size.x - 1;
		area_max.y = rendertarget_size.y - 1;
		rasterizer_rasterize(render_target, depth_buf, &rendertarget_size, &area_min, &area_max, &final_vert_buf[0], &uv[0], &ind_buf[0], ind_buf_size, texture_data, texture_size);
		rasterizer_rasterize(render_target, depth_buf, &rendertarget_size, &area_min, &area_max, &final_vert_buf2[0], &uv[0], &ind_buf[0], ind_buf_size, texture_data, texture_size);
		rasterizer_rasterize(render_target, depth_buf, &rendertarget_size, &area_min, &area_max, &final_vert_buf_large[0], &uv_large[0], &ind_buf_large[0], ind_buf_large_size, texture_data, texture_size);
		rasterizer_rasterize(render_target, depth_buf, &rendertarget_size, &area_min, &area_max, &final_vert_buf_large2[0], &uv_large[0], &ind_buf_large[0], ind_buf_large_size, texture_data, texture_size);
		rasterizer_rasterize(render_target, depth_buf, &rendertarget_size, &area_min, &area_max, &final_vert_buf_large3[0], &uv_large[0], &ind_buf_large[0], ind_buf_large_size, texture_data, texture_size);
#endif
		raster_duration = get_time() - raster_duration;

		if (rasterizer_uses_simd())
		{
			/* Deswizzle the backbuffer here */
			uint32_t *bb = get_backbuffer(renderer_info);
			if (rasterizer_uses_tiles())
			{
				for (int tile_y = 0; tile_y < padded_size.y; tile_y += tile_size)
				{
					for (int tile_x = 0; tile_x < padded_size.x; tile_x += tile_size)
					{
						const int tile_index = (padded_size.x / tile_size) * (tile_y / tile_size) + (tile_x / tile_size);
						const int tile_start = tile_size * tile_size * tile_index;

						for (int y = 0; y < tile_size && tile_y + y < rendertarget_size.y; y += 2)
						{
							int source_row = tile_start + tile_size * y;
							int target_row = (tile_y + y) * rendertarget_size.x;
							for (int x = 0; x < tile_size && tile_x + x < rendertarget_size.x; x += 2)
							{
								int start = target_row + tile_x + x;
								int start_swizz = source_row + x * 2;

								assert(start + rendertarget_size.x + 1 < rendertarget_size.x * rendertarget_size.y && "invalid target index");
								assert(start_swizz + 3 < padded_size.x * padded_size.y && "invalid source index");

								bb[start] = render_target[start_swizz];
								bb[start + 1] = render_target[start_swizz + 1];
								bb[start + rendertarget_size.x] = render_target[start_swizz + 2];
								bb[start + rendertarget_size.x + 1] = render_target[start_swizz + 3];
							}
						}
					}
				}
			}
			else
			{
				for (int y = 0; y < rendertarget_size.y; y += 2)
				{
					int row = y * rendertarget_size.x;
					for (int x = 0; x < rendertarget_size.x; x += 2)
					{
						int start = row + x;
						int start_swizz = row + x * 2;
						bb[start] = render_target[start_swizz];
						bb[start+1] = render_target[start_swizz + 1];
						bb[start+ rendertarget_size.x] = render_target[start_swizz + 2];
						bb[start+ rendertarget_size.x+1] = render_target[start_swizz + 3];
					}
				}
			}
		}

		/* Stat rendering should be easy to disable/modify.
		 * Maybe a bit field for what should be shown, uint32_t would be easily enough. */
		if (stats && font && stats_profiling_run_complete(stats))
			render_stats(stats, font, get_backbuffer(renderer_info), &rendertarget_size);

		finish_drawing(api_info);

		uint64_t frame_end = get_time();
		frame_time_mus = (uint32_t)get_time_microseconds(frame_end - frame_start);
		if (stats && stabilizing_delay == 0)
		{
			stats_update_stat(stats, STAT_FRAME, frame_time_mus);
			stats_update_stat(stats, STAT_BLIT, get_blit_duration_ms(renderer_info));
			stats_update_stat(stats, STAT_RASTER, (uint32_t)get_time_microseconds(raster_duration));
			stats_frame_complete(stats);
		}

		frame_start = frame_end;
	}

	/* Free resources */
#ifdef USE_THREADING
	for (unsigned int i = 0; i < core_count; ++i)
	{
		thread_destroy(&threads[i]);
		thread_data_deinit(&thread_data[i]);
	}
	free(threads);
	free(thread_data);
#endif

	if (rasterizer_uses_simd())
		free(render_target);
	
	free(depth_buf);

	if (stats)
		stats_destroy(&stats);
	
	if (font)
		font_destroy(&font);

	texture_destroy(&texture);
}

void transform_vertices(const struct vec3_float *in_verts, struct vec4_float *out_verts, unsigned int vert_count,
                     struct matrix_3x4 *translation, struct matrix_3x4 *rotation, struct matrix_4x4 *camera_projection)
{
	assert(in_verts && "transform_vertices: in_verts is NULL");
	assert(out_verts && "transform_vertices: out_verts is NULL");
	assert(translation && "transform_vertices: translation is NULL");
	assert(rotation && "transform_vertices: rotation is NULL");
	assert(camera_projection && "transform_vertices: camera_projection is NULL");

	struct matrix_3x4 world_transform = mat34_mul_mat34(translation, rotation);
	struct matrix_4x4 final_transform = mat44_mul_mat34(camera_projection, &world_transform);
	for (unsigned int i = 0; i < vert_count; ++i)
		out_verts[i] = mat44_mul_vec3(&final_transform, &in_verts[i]);
}

void handle_input(struct api_info *api_info, float dt, struct vec3_float *camera_trans)
{
	assert(api_info && "handle_input: api_info is NULL");
	assert(camera_trans && "handle_input: camera_trans is NULL");

	const float camera_speed = 10.0f;

	if (is_key_down(api_info, KEY_D))
		camera_trans->x += camera_speed * dt;
	else if (is_key_down(api_info, KEY_A))
		camera_trans->x -= camera_speed * dt;

	if (is_key_down(api_info, KEY_W))
		camera_trans->z += camera_speed * dt;
	else if (is_key_down(api_info, KEY_S))
		camera_trans->z -= camera_speed * dt;
}

void create_box_buffers(struct vec3_float *out_vert_buf, struct vec2_float *out_uv_buf, unsigned int *out_ind_buf)
{
	assert(out_vert_buf && "create_box_buffers: out_vert_buf is NULL");
	assert(out_uv_buf && "create_box_buffers: out_uv_buf is NULL");
	assert(out_ind_buf && "create_box_buffers: out_ind_buf is NULL");

	out_vert_buf[0].x = -2.0f;  out_vert_buf[0].y = 2.0f;   out_vert_buf[0].z = 2.0f; 
	out_vert_buf[1].x = -2.0f;  out_vert_buf[1].y = -2.0f;  out_vert_buf[1].z = 2.0f;
	out_vert_buf[2].x = -2.0f;  out_vert_buf[2].y = 2.0f;   out_vert_buf[2].z = 2.0f;
	out_vert_buf[3].x = -2.0f;  out_vert_buf[3].y = 2.0f;   out_vert_buf[3].z = -2.0f;
	out_vert_buf[4].x = -2.0f;  out_vert_buf[4].y = -2.0f;  out_vert_buf[4].z = -2.0f;
	out_vert_buf[5].x = -2.0f;  out_vert_buf[5].y = -2.0f;  out_vert_buf[5].z = 2.0f;
	out_vert_buf[6].x = 2.0f;   out_vert_buf[6].y = 2.0f;   out_vert_buf[6].z = 2.0f;
	out_vert_buf[7].x = 2.0f;   out_vert_buf[7].y = 2.0f;   out_vert_buf[7].z = -2.0f;
	out_vert_buf[8].x = 2.0f;   out_vert_buf[8].y = -2.0f;  out_vert_buf[8].z = -2.0f;
	out_vert_buf[9].x = 2.0f;   out_vert_buf[9].y = -2.0f;  out_vert_buf[9].z = 2.0f;
	out_vert_buf[10].x = 2.0f;  out_vert_buf[10].y = 2.0f;  out_vert_buf[10].z = 2.0f;
	out_vert_buf[11].x = 2.0f;  out_vert_buf[11].y = -2.0f; out_vert_buf[11].z = 2.0f;
	out_vert_buf[12].x = -2.0f; out_vert_buf[12].y = 2.0f;  out_vert_buf[12].z = 2.0f;
	out_vert_buf[13].x = -2.0f; out_vert_buf[13].y = -2.0f; out_vert_buf[13].z = 2.0f;

	out_uv_buf[0].x = 0.0f; out_uv_buf[0].y = 0.33f;
	out_uv_buf[1].x = 0.0f; out_uv_buf[1].y = 0.66f;
	out_uv_buf[2].x = 0.25f; out_uv_buf[2].y = 0.0f;
	out_uv_buf[3].x = 0.25f; out_uv_buf[3].y = 0.33f;
	out_uv_buf[4].x = 0.25f; out_uv_buf[4].y = 0.66f;
	out_uv_buf[5].x = 0.25f; out_uv_buf[5].y = 1.0f;
	out_uv_buf[6].x = 0.5f; out_uv_buf[6].y = 0.0f;
	out_uv_buf[7].x = 0.5f; out_uv_buf[7].y = 0.33f;
	out_uv_buf[8].x = 0.5f; out_uv_buf[8].y = 0.66f;
	out_uv_buf[9].x = 0.5f; out_uv_buf[9].y = 1.0f;
	out_uv_buf[10].x = 0.75f; out_uv_buf[10].y = 0.33f;
	out_uv_buf[11].x = 0.75f; out_uv_buf[11].y = 0.66f;
	out_uv_buf[12].x = 1.0f; out_uv_buf[12].y = 0.33f;
	out_uv_buf[13].x = 1.0f; out_uv_buf[13].y = 0.66f;

	/* front */
	out_ind_buf[0] = 3; out_ind_buf[1] =  4; out_ind_buf[2] = 7;
	out_ind_buf[3] = 7; out_ind_buf[4] =  4; out_ind_buf[5] = 8; 
	/* top */
	out_ind_buf[6] = 2; out_ind_buf[7] = 3; out_ind_buf[8] = 6;
	out_ind_buf[9] = 6; out_ind_buf[10] = 3; out_ind_buf[11] = 7;
	/* bottom */
	out_ind_buf[12] = 4; out_ind_buf[13] = 5; out_ind_buf[14] = 8;
	out_ind_buf[15] = 8; out_ind_buf[16] = 5; out_ind_buf[17] = 9;
	/* left */
	out_ind_buf[18] = 0; out_ind_buf[19] = 1; out_ind_buf[20] = 3;
	out_ind_buf[21] = 3; out_ind_buf[22] = 1; out_ind_buf[23] = 4;
	/* right */
	out_ind_buf[24] = 7; out_ind_buf[25] = 8; out_ind_buf[26] = 10;
	out_ind_buf[27] = 10; out_ind_buf[28] = 8; out_ind_buf[29] = 11;
	/* back */
	out_ind_buf[30] = 10; out_ind_buf[31] = 11; out_ind_buf[32] = 12;
	out_ind_buf[33] = 12; out_ind_buf[34] = 11; out_ind_buf[35] = 13;
}

/* Generates something horrible, but I just need a bit larger vert buffer for testing */
void generate_large_test_buffers(const struct vec3_float *vert_buf_box, const struct vec2_float *uv_box, const unsigned int *ind_buf_box,
                                 struct vec3_float *out_vert_buf, struct vec2_float *out_uv, unsigned int *out_ind_buf, const struct vec3_float *box_offsets, const unsigned int box_count_out)
{
	assert(vert_buf_box && "generate_large_test_buffers: vert_buf_box is NULL");
	assert(uv_box && "generate_large_test_buffers: uv_box is NULL");
	assert(ind_buf_box && "generate_large_test_buffers: ind_buf_box is NULL");
	assert(out_vert_buf && "generate_large_test_buffers: out_vert_buf is NULL");
	assert(out_uv && "generate_large_test_buffers: out_uv is NULL");
	assert(out_ind_buf && "generate_large_test_buffers: out_ind_buf is NULL");
	assert(box_offsets && "generate_large_test_buffers: box_offsets is NULL");

	struct matrix_3x4 trans_mat;
	
	for (unsigned int box = 0; box < box_count_out; ++box)
	{
		trans_mat = mat34_get_translation(&(box_offsets[box]));
		for (unsigned int i = 0; i < VERTS_IN_BOX; ++i)
		{
			unsigned int vert_ind = box * VERTS_IN_BOX + i;
			out_vert_buf[vert_ind] = mat34_mul_vec3(&trans_mat, &(vert_buf_box[i]));
			out_uv[vert_ind] = uv_box[i];
		}
		for (unsigned int i = 0; i < 36; ++i)
		{
			out_ind_buf[box * 36 + i] = ind_buf_box[i] + box * VERTS_IN_BOX;
		}
	}
}

void render_stats(struct stats *stats, struct font *font, void *render_target, struct vec2_int *target_size)
{
#define STAT_COLUMN_X 5
#define FIRST_VAL_COLUMN_X 100
#define COLUMN_X_INCREMENT 50
#define INFO_ROW_Y 5
#define ROW_Y_INCREMENT 20

	assert(stats && "render_stats: stats is NULL");
	assert(font && "render_stats: font is NULL");
	assert(render_target && "render_stats: render_target is NULL");
	assert(target_size && "render_stats: target_size is NULL");

	if (!stats)
		return;

	const struct vec2_int pos_avarage = { .x = FIRST_VAL_COLUMN_X, .y = INFO_ROW_Y };
	const struct vec2_int pos_median = { .x = FIRST_VAL_COLUMN_X + COLUMN_X_INCREMENT, .y = INFO_ROW_Y };
	const struct vec2_int pos_percentile_90 = { .x = FIRST_VAL_COLUMN_X + COLUMN_X_INCREMENT * 2, .y = INFO_ROW_Y };
	const struct vec2_int pos_percentile_95 = { .x = FIRST_VAL_COLUMN_X + COLUMN_X_INCREMENT * 3, .y = INFO_ROW_Y };
	const struct vec2_int pos_percentile_99 = { .x = FIRST_VAL_COLUMN_X + COLUMN_X_INCREMENT * 4, .y = INFO_ROW_Y };

	/* Info line */
	font_render_text(render_target, target_size, font, "avg", &pos_avarage, 0);
	font_render_text(render_target, target_size, font, "mdn", &pos_median, 0);
	font_render_text(render_target, target_size, font, "90%", &pos_percentile_90, 0);
	font_render_text(render_target, target_size, font, "95%", &pos_percentile_95, 0);
	font_render_text(render_target, target_size, font, "99%", &pos_percentile_99, 0);

	/* Frame time */
	render_stat_line_ms(stats, font, render_target, target_size, "frame (ms):", STAT_FRAME, INFO_ROW_Y + ROW_Y_INCREMENT, STAT_COLUMN_X, FIRST_VAL_COLUMN_X, COLUMN_X_INCREMENT);
	/* Rasterize */
	render_stat_line_ms(stats, font, render_target, target_size, "rast (ms):", STAT_RASTER, INFO_ROW_Y + ROW_Y_INCREMENT * 2, STAT_COLUMN_X, FIRST_VAL_COLUMN_X, COLUMN_X_INCREMENT);
	/* Blit*/
	render_stat_line_mus(stats, font, render_target, target_size, "blit (mus):", STAT_BLIT, INFO_ROW_Y + ROW_Y_INCREMENT * 3, STAT_COLUMN_X, FIRST_VAL_COLUMN_X, COLUMN_X_INCREMENT);

#undef STAT_COLUMN_X
#undef FIRST_VAL_COLUMN_X
#undef COLUMN_X_INCREMENT
#undef INFO_ROW_Y
#undef ROW_Y_INCREMENT
}

void render_stat_line_ms(struct stats *stats, struct font *font, void *render_target, struct vec2_int *target_size,
                         const char *stat_name, const unsigned char stat_id, const int row_y, const int stat_name_x, const int first_val_x, const int x_increment)
{
	assert(stats && "render_stat_line_ms: stats is NULL");
	assert(font && "render_stat_line_ms: font is NULL");
	assert(render_target && "render_stat_line_ms: render_target is NULL");
	assert(target_size && "render_stat_line_ms: target_size is NULL");
	assert(stat_name && "render_stat_line_ms: stat_name is NULL");

	struct vec2_int pos;
	pos.x = stat_name_x;
	pos.y = row_y;

	/* Render stat name*/
	font_render_text(render_target, target_size, font, stat_name, &pos, 0);

	char str[10];
	/* Avarage */
	pos.x = first_val_x;
	if (!float_to_string((float)stats_get_avarage(stats, stat_id) / 1000.0f, str, 10)) { /* The value has been truncated, do something?? */ }
	font_render_text(render_target, target_size, font, str, &pos, 0);
	/* Median */
	pos.x += x_increment;
	if (!float_to_string((float)stats_get_stat_percentile(stats, stat_id, 50.0f) / 1000.0f, str, 10)) { /* The value has been truncated, do something?? */ }
	font_render_text(render_target, target_size, font, str, &pos, 0);
	/* 90th percentile */
	pos.x += x_increment;
	if (!float_to_string((float)stats_get_stat_percentile(stats, stat_id, 90.0f) / 1000.0f, str, 10)) { /* The value has been truncated, do something?? */ }
	font_render_text(render_target, target_size, font, str, &pos, 0);
	/* 95th percentile */
	pos.x += x_increment;
	if (!float_to_string((float)stats_get_stat_percentile(stats, stat_id, 95.0f) / 1000.0f, str, 10)) { /* The value has been truncated, do something?? */ }
	font_render_text(render_target, target_size, font, str, &pos, 0);
	/* 99th percentile */
	pos.x += x_increment;
	if (!float_to_string((float)stats_get_stat_percentile(stats, stat_id, 99.0f) / 1000.0f, str, 10)) { /* The value has been truncated, do something?? */ }
	font_render_text(render_target, target_size, font, str, &pos, 0);
}

void render_stat_line_mus(struct stats *stats, struct font *font, void *render_target, struct vec2_int *target_size,
                          const char *stat_name, const unsigned char stat_id, const int row_y, const int stat_name_x, const int first_val_x, const int x_increment)
{
	assert(stats && "render_stat_line_mus: stats is NULL");
	assert(font && "render_stat_line_mus: font is NULL");
	assert(render_target && "render_stat_line_mus: render_target is NULL");
	assert(target_size && "render_stat_line_mus: target_size is NULL");
	assert(stat_name && "render_stat_line_mus: stat_name is NULL");

	struct vec2_int pos;
	pos.x = stat_name_x;
	pos.y = row_y;

	/* Render stat name*/
	font_render_text(render_target, target_size, font, stat_name, &pos, 0);

	char str[10];

	/* Avarage */
	pos.x = first_val_x;
	if (uint64_to_string((uint64_t)stats_get_avarage(stats, stat_id), str, 10))
		font_render_text(render_target, target_size, font, str, &pos, 0);
	/* Median */
	pos.x += x_increment;
	if (uint64_to_string((uint64_t)stats_get_stat_percentile(stats, stat_id, 50.0f), str, 10))
		font_render_text(render_target, target_size, font, str, &pos, 0);
	/* 90th percentile */
	pos.x += x_increment;
	if (uint64_to_string((uint64_t)stats_get_stat_percentile(stats, stat_id, 90.0f), str, 10))
		font_render_text(render_target, target_size, font, str, &pos, 0);
	/* 95th percentile */
	pos.x += x_increment;
	if (uint64_to_string((uint64_t)stats_get_stat_percentile(stats, stat_id, 95.0f), str, 10))
		font_render_text(render_target, target_size, font, str, &pos, 0);
	/* 99th percentile */
	pos.x += x_increment;
	if (uint64_to_string((uint64_t)stats_get_stat_percentile(stats, stat_id, 99.0f), str, 10))
		font_render_text(render_target, target_size, font, str, &pos, 0);
}

#ifdef USE_THREADING
void thread_data_init(struct thread_data *data, const uint32_t buffer_count, const uint32_t raster_area_count)
{
	assert(data && "thread_data_init: data is NULL");

	data->buffer_count = buffer_count;
	if (rasterizer_uses_tiles())
	{
		data->raster_area_mins = malloc(sizeof(struct vec2_int) * raster_area_count);
		data->raster_area_maxs = malloc(sizeof(struct vec2_int) * raster_area_count);
		data->raster_area_count = raster_area_count;
	}
	else
	{
		data->raster_area_mins = malloc(sizeof(struct vec2_int));
		data->raster_area_maxs = malloc(sizeof(struct vec2_int));
		data->raster_area_count = 1;
	}
	data->vert_bufs = malloc(sizeof(struct vec4_float *) * buffer_count);
	data->uv_bufs = malloc(sizeof(struct vec2_float *) * buffer_count);
	data->ind_bufs = malloc(sizeof(unsigned int *) * buffer_count);
	data->ind_counts = malloc(sizeof(unsigned int) * buffer_count);
	data->textures = malloc(sizeof(uint32_t *) * buffer_count);
	data->texture_sizes = malloc(sizeof(struct vec2_int *) * buffer_count);
}

void thread_data_deinit(struct thread_data *data)
{
	assert(data && "thread_data_deinit: data is NULL");

	free(data->raster_area_mins);
	free(data->raster_area_maxs);
	free(data->vert_bufs);
	free(data->uv_bufs);
	free(data->ind_bufs);
	free(data->ind_counts);
	free(data->textures);
	free(data->texture_sizes);
}

void thread_data_calculate_areas(struct thread_data *data, const unsigned int core_count, const struct vec2_int *backbuffer_size)
{
	assert(data && "thread_data_calculate_areas: data is NULL");
	assert(backbuffer_size && "thread_data_calculate_areas: backbuffer_size is NULL");

	if (rasterizer_uses_tiles())
	{
		/* Should have proper load balancing for threads.
		 * For example a task manager which would give a new task to 
		 * which ever thread got finished first until all tasks were completed.
		 * Giving all threads tasks from all around the screen in an attempt for even some load balancing. */
		int tile_size = rasterizer_get_tile_size();
		struct vec2_int padded_size;
		rasterizer_get_padded_size(backbuffer_size, &padded_size);
		unsigned int current_core = 0;
		int area_index = 0;
		for (int y = 0; y < padded_size.y; y += tile_size)
		{
			for (int x = 0; x < padded_size.x; x += tile_size)
			{
				data[current_core].raster_area_mins[area_index].x = x;
				data[current_core].raster_area_mins[area_index].y = y;
				data[current_core].raster_area_maxs[area_index].x = x + tile_size - 1;
				data[current_core].raster_area_maxs[area_index].y = y + tile_size - 1;
				data[current_core].raster_area_count = area_index + 1;

				++current_core;
				if (current_core >= core_count)
				{
					current_core = 0;
					++area_index;
				}
			}
		}
	}
	else
	{
		unsigned int columns = core_count / 2;
		unsigned int width = backbuffer_size->x / columns;
		unsigned int i;
		for (i = 0; i < columns; ++i)
		{
			data[i].raster_area_mins[0].x = i * width;
			data[i].raster_area_maxs[0].x = i == columns - 1 ? backbuffer_size->x - 1 : i * width + width - 1;
			data[i].raster_area_mins[0].y = 0;
			data[i].raster_area_maxs[0].y = backbuffer_size->y / 2 - 1;
		}

		columns = core_count - columns;
		for (unsigned int j = 0; i < core_count; ++i, ++j)
		{
			data[i].raster_area_mins[0].x = j * width;
			data[i].raster_area_maxs[0].x = i == core_count - 1 ? backbuffer_size->x - 1 : j * width + width - 1;
			data[i].raster_area_mins[0].y = backbuffer_size->y / 2;
			data[i].raster_area_maxs[0].y = backbuffer_size->y - 1;
		}
	}
}

void rasterize_thread(void *data)
{
	assert(data && "rasterize_thread: data is NULL");

	struct thread_data *td = (struct thread_data *)data;
	for (unsigned int area = 0; area < td->raster_area_count; ++area)
	{
		for (unsigned int i = 0; i < td->buffer_count; ++i)
		{
			rasterizer_rasterize(td->render_target, td->depth_buffer, &td->target_size, &td->raster_area_mins[area], &td->raster_area_maxs[area],
				&td->vert_bufs[i][0], &td->uv_bufs[i][0], &td->ind_bufs[i][0], td->ind_counts[i],
				td->textures[i], td->texture_sizes[i]);
		}
	}
}
#endif