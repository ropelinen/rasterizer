#include "software_rasterizer/precompiled.h"

#include "software_rasterizer/demo/font.h"
#include "software_rasterizer/demo/osal.h"
#include "software_rasterizer/demo/stats.h"
#include "software_rasterizer/rasterizer.h"

void handle_input(struct api_info *api_info, float dt, struct vec3_float *camera_trans);

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

	struct font *font = font_create("Rockwell.ttf");
	if (!font)
		error_popup("Failed to initialize the font", false);

	if (font)
		font_set_line_height(font, 19);

	struct stats *stats = stats_create(STAT_COUNT, 120);

	uint64_t frame_start = get_time();

	/* Test tri, CCW */
	struct vec3_float vert_buf[8] = { { .x = -2.0f, .y = 2.0f, .z = -2.0f }, { .x = -2.0f, .y = -2.0f, .z = -2.0f },
	                                  { .x = 2.0f, .y = 2.0f, .z = -2.0f }, { .x = 2.0f, .y = -2.0f, .z = -2.0f },
									  { .x = -2.0f, .y = 2.0f, .z = 2.0f }, { .x = -2.0f, .y = -2.0f, .z = 2.0f },
									  { .x = 2.0f, .y = 2.0f, .z = 2.0f }, { .x = 2.0f, .y = -2.0f, .z = 2.0f } };
	struct vec4_float final_vert_buf[8];
	struct vec4_float final_vert_buf2[8];
	uint32_t vert_colors[8] = { 0x00FF00, 0x0F0F0F, 0x800080, 0x0000FF
	                          , 0x00FF00, 0x0F0F0F, 0x800080, 0x0000FF };
	unsigned int ind_buf[36] = { 0, 1, 3, 3, 2, 0 /* front */
	                           , 0, 2, 4, 4, 2, 6 /* top */ 
	                           , 1, 5, 3, 3, 5, 7 /* bottom */
	                           , 0, 4, 5, 5, 1, 0 /* left */
	                           , 2, 3, 7, 7, 6, 2 /* right */
	                           , 6, 7, 5, 5, 4, 6 }; /* back */
	
	/* Transfrom related */
	float rot = 0.0f;
	struct vec3_float translation = { .x = 2.0f, .y = -4.0f, .z = 10.0f };
	struct vec3_float camera_trans = { .x = 2.0f,.y = -4.0f,.z = 0.0f };
	struct matrix_3x4 trans_mat = mat34_get_translation(&translation);
	translation.x += 2.0f;
	translation.z += 3.0f;
	struct matrix_3x4 trans_mat2 = mat34_get_translation(&translation);

	struct vec2_int backbuffer_size = get_backbuffer_size(renderer_info);
	struct matrix_4x4 perspective_mat = mat44_get_perspective_lh_fov(DEG_TO_RAD(59.0f), (float)backbuffer_size.x / (float)backbuffer_size.y, 1.0f, 1000.0f);

	uint32_t frame_time_mus = 0;

	while (event_loop())
	{
		renderer_clear_backbuffer(renderer_info, 0xFF0000);

		float dt = (float)frame_time_mus / 1000000.0f;
		
		handle_input(api_info, dt, &camera_trans);

		/* Update test rotation */
		rot += dt * (float)PI / 16.0f;
		if (rot > PI * 2.0f) rot -= PI * 2.0f;

		/* Projection and view*/
		struct matrix_3x4 camera_mat = mat34_get_translation(&camera_trans);
		camera_mat = mat34_get_inverse(&camera_mat);
		struct matrix_4x4 camera_projection = mat44_mul_mat34(&perspective_mat, &camera_mat);

		/* World space */
		//struct matrix_3x4 rot_mat = mat34_get_rotation_z(rot);
		struct matrix_3x4 rot_mat = mat34_get_rotation_y(rot);
		struct matrix_3x4 world_transform = mat34_mul_mat34(&trans_mat, &rot_mat);
		struct matrix_3x4 world_transform2 = mat34_mul_mat34(&trans_mat2, &rot_mat);

		struct matrix_4x4 final_transform = mat44_mul_mat34(&camera_projection, &world_transform);
		struct matrix_4x4 final_transform2 = mat44_mul_mat34(&camera_projection, &world_transform2);

		for (unsigned int i = 0; i < sizeof(vert_buf) / sizeof(vert_buf[0]); ++i)
		{
			final_vert_buf[i] = mat44_mul_vec3(&final_transform, &vert_buf[i]);
			final_vert_buf2[i] = mat44_mul_vec3(&final_transform2, &vert_buf[i]);
		}

		rasterizer_rasterize(get_backbuffer(renderer_info), &backbuffer_size, &final_vert_buf[0], &vert_colors[0], &ind_buf[0], sizeof(ind_buf) / sizeof(ind_buf[0]));
		rasterizer_rasterize(get_backbuffer(renderer_info), &backbuffer_size, &final_vert_buf2[0], &vert_colors[0], &ind_buf[0], sizeof(ind_buf) / sizeof(ind_buf[0]));

		/* Stat rendering should be easy to disable/modify,
		* maybe a bit field for what should be shown uint32_t would be easily enough. */
		if (font)
			render_stats(stats, font, get_backbuffer(renderer_info), &backbuffer_size);

		finish_drawing(api_info);

		uint64_t frame_end = get_time();
		frame_time_mus = (uint32_t)get_time_microseconds(frame_end - frame_start);
		stats_update_stat(stats, STAT_FRAME, frame_time_mus);
		stats_update_stat(stats, STAT_BLIT, get_blit_duration_ms(renderer_info));
		stats_frame_complete(stats);

		frame_start = frame_end;
	}

	stats_destroy(&stats);
	if (font)
		font_destroy(&font);
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
	/* Blit*/
	render_stat_line_mus(stats, font, render_target, target_size, "blit (mus):", STAT_BLIT, INFO_ROW_Y + ROW_Y_INCREMENT * 2, STAT_COLUMN_X, FIRST_VAL_COLUMN_X, COLUMN_X_INCREMENT);

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
