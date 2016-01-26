#ifndef RPLNN_TEXTURE_H
#define RPLNN_TEXTURE_H

struct texture;

/* Should create a version of this which doesn't malloc (basically just give memory block as a parameter). */
struct texture *texture_create(const char *file_name);

void texture_destroy(struct texture **texture);

void texture_get_info(struct texture *texture, uint32_t **buf, struct vec2_int **size);

#endif /* RPLNN_TEXTURE_H */
