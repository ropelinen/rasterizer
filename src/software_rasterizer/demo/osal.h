#ifndef RPLNN_OSAL_H
#define RPLNN_OSAL_H

/* A simple OS Abtraction Layer */

/* TODO: Rename api_info */
struct api_info;
struct renderer_info;

void error_popup(const char *msg, const bool kill_program);

/* This should be defined in main.c and only containt non platform specific code */
void main(struct api_info *, struct renderer_info *);

bool event_loop(void);

void finish_drawing(struct api_info *api_info);

void *get_backbuffer(struct renderer_info *info);
struct vec2_int get_backbuffer_size(struct renderer_info *info);
uint32_t get_blit_duration_ms(struct renderer_info *info);

void renderer_clear_backbuffer(struct renderer_info *info, const uint32_t color);

uint64_t get_time(void);
uint64_t get_time_microseconds(const uint64_t time);

bool uint64_to_string(const uint64_t value, char *buffer, const size_t buffer_size);
bool float_to_string(const float value, char *buffer, const size_t buffer_size);


#endif /* RPLNN_OSAL_H */