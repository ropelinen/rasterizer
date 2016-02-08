#include "software_rasterizer/precompiled.h"

#include "font.h"

#define STB_TRUETYPE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable : 4100) /* unreferenced formal parameter */
#include <stb/stb_truetype.h>
#pragma warning(pop)

#include <stdio.h>

struct font
{
	stbtt_fontinfo font_info;
	float scale;
	int ascent;
	int descent;
	int line_gap;
	void *buffer;
};

struct font *font_create(const char *file_name)
{
	assert(file_name && "font_create: file_name is NULL");

	struct font *font = malloc(sizeof(struct font));
	FILE *font_file = fopen(file_name, "rb");
	if (!font_file)
		return NULL;

	fseek(font_file, 0, SEEK_END);
	int size = ftell(font_file);
	fseek(font_file, 0, SEEK_SET);

	font->buffer = malloc(size);

	fread(font->buffer, size, 1, font_file);
	fclose(font_file);

	if (!stbtt_InitFont(&font->font_info, font->buffer, 0))
		return NULL;

	return font;
}

void font_destroy(struct font **font)
{
	assert(font && "font_destroy: font is NULL");
	assert(*font && "font_destroy: *font is NULL");

	free((*font)->buffer);
	free(*font);
	*font = NULL;
}

void font_set_line_height(struct font *font, float line_height)
{
	assert(font && "font_set_line_height: font is NULL");

	float scale;
	int ascent, descent;

	scale = stbtt_ScaleForPixelHeight(&(font->font_info), line_height);
	stbtt_GetFontVMetrics(&(font->font_info), &ascent, &descent, &(font->line_gap));
	font->ascent = (int)(ascent * scale);
	font->descent = (int)(descent * scale);
	font->scale = scale;
}

void font_render_text(void *render_target, const struct vec2_int *target_size, struct font *font, const char *text, const struct vec2_int *pos, const uint32_t text_color)
{
	assert(render_target && "font_render_text: render_target is NULL");
	assert(target_size && "font_render_text: target_size is NULL");
	assert(font && "font_render_text: font is NULL");
	assert(text && "font_render_text: text is NULL");
	assert(pos && "font_render_text: pos is NULL");

	stbtt_fontinfo *font_info = &(font->font_info);
	float scale = font->scale;

	int x = 0;
	unsigned int text_len = (unsigned int)strlen(text);
	/* This sorely needs optimization */
	for (unsigned int i = 0; i < text_len; ++i)
	{
		int char_x0, char_x1;
		int char_y0, char_y1;
		stbtt_GetCodepointBitmapBox(font_info, text[i], scale, scale, &char_x0, &char_y0, &char_x1, &char_y1);

		int y = font->ascent + char_y0;
		int char_width, char_height;
		int x_offset, y_offset;
		/* NOTE: This allocates memory, should do something about that */
		unsigned char *bitmap = stbtt_GetCodepointBitmap(font_info, scale, scale, (int)text[i], &char_width, &char_height, &x_offset, &y_offset);

		for (int char_y = 0; char_y < char_height; ++char_y)
		{
			int line = target_size->y - (pos->y + char_y + y);
			if (line >= target_size->y)
				break;

			for (int char_x = 0; char_x < char_width; ++char_x)
			{
				int pos_x = pos->x + char_x + x;
				if (pos_x >= target_size->x)
					break;

				uint8_t alpha = bitmap[char_y * char_width + char_x];	
				uint32_t *pixel = &((uint32_t*)render_target)[line * target_size->x + pos_x];
				/* This needs to be calculated using a macro or something as this depends on the target format */
				/* Colors of the original pixel */
				uint32_t s_r = (*pixel >> 16) & 0xFF;
				uint32_t s_g = (*pixel >> 8) & 0xFF;
				uint32_t s_b = (*pixel) & 0xFF;
				/* Colors for the text */
				uint32_t t_r = (text_color >> 16) & 0xFF;
				uint32_t t_g = (text_color >> 8) & 0xFF;
				uint32_t t_b = (text_color) & 0xFF;
				/* 255 = opaque, 0 = transparent */
				float alpha_float = (float)(alpha / 255.0f);
				uint32_t n_r = ((uint32_t)(s_r * (1.0f - alpha_float)) + (uint32_t)(t_r * alpha_float)) << 16;
				uint32_t n_g = ((uint32_t)(s_g * (1.0f - alpha_float)) + (uint32_t)(t_g * alpha_float)) << 8;
				uint32_t n_b = ((uint32_t)(s_b * (1.0f - alpha_float)) + (uint32_t)(t_b * alpha_float));
				/* Calculate target pixel */
				uint32_t target_pixel = 0;
				target_pixel |= n_r & 0xFF0000;
				target_pixel |= n_g & 0xFF00;
				target_pixel |= n_b & 0xFF;
				*pixel = target_pixel;
			}
		}

		/* Advance character width */
		int advance_x;
		stbtt_GetCodepointHMetrics(font_info, text[i], &advance_x, 0);
		x = x + (int)(advance_x * scale);

		/* Add kerning */
		int kerning = stbtt_GetCodepointKernAdvance(font_info, text[i], text[i + 1]);
		x = x + (int)(kerning * scale);

		if (x >= target_size->x)
			break;
	}
}
