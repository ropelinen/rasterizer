#include "software_rasterizer/precompiled.h"

#include "software_rasterizer/demo/font.h"
#include "software_rasterizer/demo/osal.h"
#include "software_rasterizer/demo/stats.h"
#include "software_rasterizer/demo/texture.h"
#include "software_rasterizer/rasterizer.h"

#define LARGE_VERT_BUF_BOXES 8

void handle_input(struct api_info *api_info, float dt, struct vec3_float *camera_trans);

void generate_large_test_buffers(const struct vec3_float *vert_buf_box, const struct vec2_float *uv_box, const unsigned int *ind_buf_box,
	struct vec3_float *out_vert_buf, struct vec2_float *out_uv, unsigned int *out_ind_buf, const struct vec3_float *box_offsets, const unsigned int box_count_out);

void render_stats(struct stats *stats, struct font *font, void *render_target, struct vec2_int *target_size);
void render_stat_line_ms(struct stats *stats, struct font *font, void *render_target, struct vec2_int *target_size, 
                         const char *stat_name, const unsigned char stat_id, const int row_y, const int stat_name_x, const int first_val_x, const int x_increment);
void render_stat_line_mus(struct stats *stats, struct font *font, void *render_target, struct vec2_int *target_size,
                          const char *stat_name, const unsigned char stat_id, const int row_y, const int stat_name_x, const int first_val_x, const int x_increment);

/* A genereic platform independent main function.
 * See osal.c for platform specific main. */
void main(struct api_info *api_info, struct renderer_info *renderer_info)
{
	assert(api_info && "main: api_info is NULL");
	assert(renderer_info && "main: renderer_info is NULL");

	struct texture *texture = texture_create("brick_base.png");
	if (!texture)
		error_popup("Failed to load the texture", true);

	/* Fix this :D */
	uint32_t *texture_data;
	struct vec2_int *texture_size;

	texture_get_info(texture, &texture_data, &texture_size);

	struct font *font = font_create("Rockwell.ttf");
	if (!font)
		error_popup("Failed to initialize the font", false);

	if (font)
		font_set_line_height(font, 19);

	struct stats *stats = stats_create(STAT_COUNT, 1000, true);
	unsigned int stabilizing_delay = 500;

	/* Test tri, CCW */
	struct vec3_float vert_buf[8] = { { .x = -2.0f, .y = 2.0f, .z = -2.0f }, { .x = -2.0f, .y = -2.0f, .z = -2.0f },
	                                  { .x = 2.0f, .y = 2.0f, .z = -2.0f }, { .x = 2.0f, .y = -2.0f, .z = -2.0f },
									  { .x = -2.0f, .y = 2.0f, .z = 2.0f }, { .x = -2.0f, .y = -2.0f, .z = 2.0f },
									  { .x = 2.0f, .y = 2.0f, .z = 2.0f }, { .x = 2.0f, .y = -2.0f, .z = 2.0f } };
	struct vec4_float final_vert_buf[8];
	struct vec4_float final_vert_buf2[8];
	struct vec2_float uv[8] = { { .x = 0.0f, .y = 0.0f }, { .x = 0.0f, .y = 1.0f }, { .x = 1.0f, .y = 0.0f }, { .x = 1.0f, .y = 1.0f }
							  , { .x = 1.0f, .y = 0.0f }, { .x = 1.0f, .y = 1.0f }, { .x = 0.0f, .y = 0.0f }, { .x = 0.0f, .y = 1.0f } };
	unsigned int ind_buf[36] = { 0, 1, 3, 3, 2, 0 /* front */
	                           , 0, 2, 4, 4, 2, 6 /* top */ 
	                           , 1, 5, 3, 3, 5, 7 /* bottom */
	                           , 0, 4, 5, 5, 1, 0 /* left */
	                           , 2, 3, 7, 7, 6, 2 /* right */
	                           , 6, 7, 5, 5, 4, 6 }; /* back */

	struct vec3_float vert_buf_large[8 * LARGE_VERT_BUF_BOXES];
	struct vec4_float final_vert_buf_large[8 * LARGE_VERT_BUF_BOXES];
	struct vec4_float final_vert_buf_large2[8 * LARGE_VERT_BUF_BOXES];
	struct vec4_float final_vert_buf_large3[8 * LARGE_VERT_BUF_BOXES];
	struct vec2_float uv_large[8 * LARGE_VERT_BUF_BOXES];
	unsigned int ind_buf_large[36 * LARGE_VERT_BUF_BOXES];
	struct vec3_float box_offsets[LARGE_VERT_BUF_BOXES] = { { .x = -6.0f, .y = 6.0f, .z = 6.0f }, { .x = 2.0f, .y = 4.0f, .z = 6.0f }, { .x = -2.0f, .y = -2.0f, .z = 2.0f }, { .x = 0.0f, .y = 0.0f, .z = 0.0f },
	                                                        { .x = -6.0f, .y = 2.0f, .z = -2.0f }, { .x = -4.0f, .y = -4.0f, .z = -8.0f }, { .x = 4.0f, .y = 6.0f, .z = -6.0f }, { .x = 2.0f, .y = 10.0f, .z = -8.0f } };
	generate_large_test_buffers(&vert_buf[0], &uv[0], &ind_buf[0], &vert_buf_large[0], &uv_large[0], &ind_buf_large[0], &box_offsets[0], LARGE_VERT_BUF_BOXES);
	
	/* Transfrom related */
	struct vec3_float translation = { .x = -5.5f, .y = -8.0f, .z = 10.0f };
	struct vec3_float camera_trans = { .x = 2.0f, .y = -4.0f, .z = 0.0f };
	struct matrix_3x4 trans_mat = mat34_get_translation(&translation);
	translation.x += 2.0f;
	translation.z += 3.0f;
	struct matrix_3x4 trans_mat2 = mat34_get_translation(&translation);
	translation.x += 2.0f;
	translation.y += 4.0f;
	translation.z += 15.0f;
	struct matrix_3x4 trans_mat_large = mat34_get_translation(&translation);
	translation.x += 26.0f;
	translation.y -= 8.0f;
	translation.z += 10.0f;
	struct matrix_3x4 trans_mat_large2 = mat34_get_translation(&translation);
	translation.x += 10.0f;
	translation.y += 18.0f;
	translation.z += 50.0f;
	struct matrix_3x4 trans_mat_large3 = mat34_get_translation(&translation);

	struct vec2_int backbuffer_size = get_backbuffer_size(renderer_info);
	struct matrix_4x4 perspective_mat = mat44_get_perspective_lh_fov(DEG_TO_RAD(59.0f), (float)backbuffer_size.x / (float)backbuffer_size.y, 1.0f, 1000.0f);
	uint32_t *depth_buf = malloc(backbuffer_size.x * backbuffer_size.y * sizeof(uint32_t));

	uint32_t frame_time_mus = 0;

	uint64_t frame_start = get_time();

	while (event_loop())
	{
		if (stabilizing_delay > 0)
			--stabilizing_delay;

		renderer_clear_backbuffer(renderer_info, 0xFF0000);

		float dt = (float)frame_time_mus / 1000000.0f;

		handle_input(api_info, dt, &camera_trans);

		/* Projection and view*/
		struct matrix_3x4 camera_mat = mat34_get_translation(&camera_trans);
		camera_mat = mat34_get_inverse(&camera_mat);
		struct matrix_4x4 camera_projection = mat44_mul_mat34(&perspective_mat, &camera_mat);

		/* World space */
		struct matrix_3x4 rot_mat = mat34_get_rotation_y(DEG_TO_RAD(40.0f));
		struct matrix_3x4 world_transform = mat34_mul_mat34(&trans_mat, &rot_mat);
		struct matrix_4x4 final_transform = mat44_mul_mat34(&camera_projection, &world_transform);
		for (unsigned int i = 0; i < sizeof(vert_buf) / sizeof(vert_buf[0]); ++i)
			final_vert_buf[i] = mat44_mul_vec3(&final_transform, &vert_buf[i]);

		/* A function for this stuff would be nice */
		rot_mat = mat34_get_rotation_y(DEG_TO_RAD(38.0f));
		world_transform = mat34_mul_mat34(&trans_mat2, &rot_mat);
		final_transform = mat44_mul_mat34(&camera_projection, &world_transform);
		for (unsigned int i = 0; i < sizeof(vert_buf) / sizeof(vert_buf[0]); ++i)
			final_vert_buf2[i] = mat44_mul_vec3(&final_transform, &vert_buf[i]);

		rot_mat = mat34_get_rotation_y(DEG_TO_RAD(30.0f));
		world_transform = mat34_mul_mat34(&trans_mat_large, &rot_mat);
		final_transform = mat44_mul_mat34(&camera_projection, &world_transform);
		for (unsigned int i = 0; i < sizeof(vert_buf_large) / sizeof(vert_buf_large[0]); ++i)
			final_vert_buf_large[i] = mat44_mul_vec3(&final_transform, &vert_buf_large[i]);

		rot_mat = mat34_get_rotation_y(DEG_TO_RAD(-45.0f));
		world_transform = mat34_mul_mat34(&trans_mat_large2, &rot_mat);
		final_transform = mat44_mul_mat34(&camera_projection, &world_transform);
		for (unsigned int i = 0; i < sizeof(vert_buf_large) / sizeof(vert_buf_large[0]); ++i)
			final_vert_buf_large2[i] = mat44_mul_vec3(&final_transform, &vert_buf_large[i]);

		world_transform = mat34_mul_mat34(&trans_mat_large3, &rot_mat);
		final_transform = mat44_mul_mat34(&camera_projection, &world_transform);
		for (unsigned int i = 0; i < sizeof(vert_buf_large) / sizeof(vert_buf_large[0]); ++i)
			final_vert_buf_large3[i] = mat44_mul_vec3(&final_transform, &vert_buf_large[i]);

		rasterizer_clear_depth_buffer(depth_buf, &backbuffer_size);

		uint64_t raster_duration = get_time();
		rasterizer_rasterize(get_backbuffer(renderer_info), depth_buf, &backbuffer_size, &final_vert_buf[0], &uv[0], &ind_buf[0], sizeof(ind_buf) / sizeof(ind_buf[0]), texture_data, texture_size);
		rasterizer_rasterize(get_backbuffer(renderer_info), depth_buf, &backbuffer_size, &final_vert_buf2[0], &uv[0], &ind_buf[0], sizeof(ind_buf) / sizeof(ind_buf[0]), texture_data, texture_size);
		rasterizer_rasterize(get_backbuffer(renderer_info), depth_buf, &backbuffer_size, &final_vert_buf_large[0], &uv_large[0], &ind_buf_large[0], sizeof(ind_buf_large) / sizeof(ind_buf_large[0]), texture_data, texture_size);
		rasterizer_rasterize(get_backbuffer(renderer_info), depth_buf, &backbuffer_size, &final_vert_buf_large2[0], &uv_large[0], &ind_buf_large[0], sizeof(ind_buf_large) / sizeof(ind_buf_large[0]), texture_data, texture_size);
		rasterizer_rasterize(get_backbuffer(renderer_info), depth_buf, &backbuffer_size, &final_vert_buf_large3[0], &uv_large[0], &ind_buf_large[0], sizeof(ind_buf_large) / sizeof(ind_buf_large[0]), texture_data, texture_size);
		raster_duration = get_time() - raster_duration;

		/* Stat rendering should be easy to disable/modify,
		* maybe a bit field for what should be shown uint32_t would be easily enough. */
		if (font && stats_profiling_run_complete(stats))
			render_stats(stats, font, get_backbuffer(renderer_info), &backbuffer_size);

		finish_drawing(api_info);

		uint64_t frame_end = get_time();
		frame_time_mus = (uint32_t)get_time_microseconds(frame_end - frame_start);
		if (stabilizing_delay == 0)
		{
			stats_update_stat(stats, STAT_FRAME, frame_time_mus);
			stats_update_stat(stats, STAT_BLIT, get_blit_duration_ms(renderer_info));
			stats_update_stat(stats, STAT_RASTER, (uint32_t)get_time_microseconds(raster_duration));
			stats_frame_complete(stats);
		}

		frame_start = frame_end;
	}

	free(depth_buf);

	stats_destroy(&stats);
	if (font)
		font_destroy(&font);

	texture_destroy(&texture);
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

void generate_large_test_buffers(const struct vec3_float *vert_buf_box, const struct vec2_float *uv_box, const unsigned int *ind_buf_box,
                                 struct vec3_float *out_vert_buf, struct vec2_float *out_uv, unsigned int *out_ind_buf, const struct vec3_float *box_offsets, const unsigned int box_count_out)
{
	assert(vert_buf_box && "generate_large_test_buffers: vert_buf_box is NULL");
	assert(uv_box && "generate_large_test_buffers: uv_box is NULL");
	assert(ind_buf_box && "generate_large_test_buffers: ind_buf_box is NULL");
	assert(out_vert_buf && "generate_large_test_buffers: out_vert_buf is NULL");
	assert(out_uv && "generate_large_test_buffers: uv_out is NULL");
	assert(out_ind_buf && "generate_large_test_buffers: ind_buf_out is NULL");
	assert(box_offsets && "generate_large_test_buffers: box_offsets is NULL");

	/* Generates a something horrible, but I just need a a bit larger vert buffer for testing */
	struct matrix_3x4 trans_mat;
	
	for (unsigned int box = 0; box < box_count_out; ++box)
	{
		trans_mat = mat34_get_translation(&(box_offsets[box]));
		for (unsigned int i = 0; i < 8; ++i)
		{
			unsigned int vert_ind = box * 8 + i;
			out_vert_buf[vert_ind] = mat34_mul_vec3(&trans_mat, &(vert_buf_box[i]));
			out_uv[vert_ind] = uv_box[i];
		}
		for (unsigned int i = 0; i < 36; ++i)
		{
			out_ind_buf[box * 36 + i] = ind_buf_box[i] + box * 8;
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
#undef FIRS_VAL_COLUMN_X
#undef COLUMN_X_INCREMENT
#undef INFO_ROW_Y
#undef ROW_Y_INCREMENT
}

void render_stat_line_ms(struct stats *stats, struct font *font, void *render_target, struct vec2_int *target_size,
                         const char *stat_name, const unsigned char stat_id, const int row_y, const int stat_name_x, const int first_val_x, const int x_increment)
{
	assert(stats && "render_stats: stats is NULL");
	assert(font && "render_stats: font is NULL");
	assert(render_target && "render_stats: render_target is NULL");
	assert(target_size && "render_stats: target_size is NULL");

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
	assert(stats && "render_stats: stats is NULL");
	assert(font && "render_stats: font is NULL");
	assert(render_target && "render_stats: render_target is NULL");
	assert(target_size && "render_stats: target_size is NULL");

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
