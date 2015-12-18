#include "software_rasterizer/precompiled.h"

#include "software_rasterizer/demo/font.h"
#include "software_rasterizer/demo/osal.h"
#include "software_rasterizer/demo/stats.h"
#include "software_rasterizer/rasterizer.h"

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
	char fps[10];
	fps[0] = '\0';

	/* Test tri, CCW */
	struct vec2_int vert_buf[7] = { { .x = 0, .y = 0 }, { .x = 30, .y = 60 }, { .x = -30, .y = 100 }, { .x = -40, .y = 30 }
								  , { .x = -100, .y = 0 }, { .x = -70, .y = 60 }, { .x = -130, .y = 100 } };
	unsigned int ind_buf[9] = { 0, 1, 2, 0, 2, 3, 4, 5, 6 };

	struct vec2_int backbuffer_size = get_backbuffer_size(renderer_info);
	while (event_loop())
	{
		renderer_clear_backbuffer(renderer_info, 0xFF0000);
		rasterizer_rasterize_triangle(get_backbuffer(renderer_info), &backbuffer_size, &vert_buf[0], &ind_buf[0], 9);
		/* Stat rendering should be easy to disable/modify,
		* maybe a bit field for what should be shown uint32_t would be easily enough. */
		if (font)
			render_stats(stats, font, get_backbuffer(renderer_info), &backbuffer_size);

		finish_drawing(api_info);

		uint64_t frame_end = get_time();
		stats_update_stat(stats, STAT_FRAME, (uint32_t)get_time_microseconds(frame_end - frame_start));
		stats_update_stat(stats, STAT_BLIT, get_blit_duration_ms(renderer_info));
		stats_frame_complete(stats);
		
		frame_start = frame_end;
	}

	stats_destroy(&stats);
	if (font)
		font_destroy(&font);
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
