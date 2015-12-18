#ifndef RPLNN_FONT_H
#define RPLNN_FONT_H

struct font;

/* Should create a version of this which doesn't malloc (basically just give memory block as a parameter). */
struct font *font_create(const char *file_name);

void font_destroy(struct font **font);

void font_set_line_height(struct font *font, float line_height);

void font_render_text(void *render_target, const struct vec2_int *target_size, struct font *font, const char *text, const struct vec2_int *pos, const uint32_t text_color);

#endif /* RPLNN_FONT_H */
