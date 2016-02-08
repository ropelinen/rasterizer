#ifndef RPLNN_OSAL_H
#define RPLNN_OSAL_H

/* A simple OS Abtraction Layer */

struct api_info;
struct renderer_info;

void error_popup(const char *msg, const bool kill_program);

/* This should be defined in main.c and only contain non platform specific code */
void main(struct api_info *api_info, struct renderer_info *renderer_info);

bool event_loop(void);

void finish_drawing(struct api_info *api_info);

void *get_backbuffer(struct renderer_info *info);
struct vec2_int get_backbuffer_size(struct renderer_info *info);
uint32_t get_blit_duration_ms(struct renderer_info *info);

void renderer_clear_backbuffer(struct renderer_info *info, const uint32_t color);

uint64_t get_time(void);
uint64_t get_time_microseconds(const uint64_t time);

unsigned int get_logical_core_count(void);

bool uint64_to_string(const uint64_t value, char *buffer, const size_t buffer_size);
bool float_to_string(const float value, char *buffer, const size_t buffer_size);

/* Threading */
struct thread;
/* Negative value means any core will do */
struct thread *thread_create(const int core_id);
void thread_destroy(struct thread **thread);
bool thread_set_task(struct thread *thread, void(*func)(void *), void *data);
bool thread_has_task(struct thread *thread);
void thread_wait_for_task(struct thread *thread);

/* Input */
bool is_key_down(struct api_info *api_info, enum keycodes keycode);

enum keycodes
{
	KEY_A = 0,
	KEY_B,
	KEY_C,
	KEY_D,
	KEY_E,
	KEY_F,
	KEY_G,
	KEY_H,
	KEY_I,
	KEY_J,
	KEY_K,
	KEY_L,
	KEY_M,
	KEY_N,
	KEY_O,
	KEY_P,
	KEY_Q,
	KEY_R,
	KEY_S,
	KEY_T,
	KEY_U,
	KEY_V,
	KEY_W,
	KEY_X,
	KEY_Y,
	KEY_Z,
	KEY_COUNT
};


#endif /* RPLNN_OSAL_H */
