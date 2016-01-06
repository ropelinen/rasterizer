#include "software_rasterizer/precompiled.h"

#if RPLNN_PLATFORM == RPLNN_PLATFORM_WINDOWS

#include "osal.h"

#pragma warning(push)
#pragma warning(disable : 4255) /* no function prototype given: converting '()' to '(void)' */
#include <Windows.h>
#pragma warning(pop)
#include <stdio.h>

/* Reserved|Red|Green|Blue, 32bit */
#define RGB_BLACK 0
#define RGB_BLUE 255
#define RGB_RED 255 << 16

struct renderer_info
{
#if RPLNN_RENDERER == RPLNN_RENDERER_GDI
	BITMAPINFO bitmapinfo;
	unsigned int width;
	unsigned int height;
	uint32_t blit_duration_mus;
	void *buffer;
#endif
};

#define MAX_KEYCODE 0xFF
#define KEY_STATE_TYPE_BITS 32
struct api_info
{
	HWND hwnd;
	uint32_t key_states[(MAX_KEYCODE + KEY_STATE_TYPE_BITS - 1) / KEY_STATE_TYPE_BITS];
	struct renderer_info *renderer_info;
};

LPCWSTR class_name = TEXT("TestClass");
LPCWSTR window_name = TEXT("Rasterizer");
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

void error_popup(const char *msg, const bool kill_program)
{
	assert(msg && "error_popup: msg is NULL");

	MessageBoxA(NULL, msg, kill_program ? "Fatal Error" : "Error", MB_OK | MB_ICONERROR);
	if (kill_program) 
		ExitProcess(~(UINT)0);
}

void create_window(struct api_info *api_info, HINSTANCE hInstance, struct renderer_info *renderer_info)
{
	assert(api_info && "create_window: api_info is NULL");
	assert(renderer_info && "create_window: renderer_info is NULL");

	WNDCLASSEX wc;

	memset(&wc, 0, sizeof(wc));
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = class_name;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);

	if (RegisterClassEx(&wc) == 0)
		error_popup("Failed to register window class", true);

	RECT client_size;
	client_size.left = 0; 
	client_size.top = 0;
	client_size.right = (LONG)renderer_info->width;
	client_size.bottom = (LONG)renderer_info->height;

	DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
	DWORD exStyle = 0;
	if (!AdjustWindowRectEx(&client_size, style, false, exStyle))
		error_popup("Failed to get correct window size", true);

		

	api_info->hwnd = CreateWindowEx(
		exStyle,
		class_name,
		window_name,
		style,
		120, 120,
		client_size.right - client_size.left, client_size.bottom - client_size.top,
		NULL,
		NULL,
		hInstance,
		renderer_info); /* Need to pass stuff here, at least bitmap info for GDI */

	if (api_info->hwnd == NULL)
		error_popup("Failed to create a window", true);

	/* If I need to access api_info in WindowProc's WM_CREATE handling move this there. */
	SetWindowLongPtr(api_info->hwnd, GWLP_USERDATA, (LONG_PTR)api_info);
}

void renderer_initialize(struct renderer_info *info, unsigned int width, unsigned int height)
{
	assert(info && "renderer_initialize: info is NULL");

#if RPLNN_RENDERER == RPLNN_RENDERER_GDI
	BITMAPINFO *bitmapinfo = &(info->bitmapinfo);
	memset(bitmapinfo, 0, sizeof(BITMAPINFO));
	bitmapinfo->bmiHeader.biSize = sizeof(BITMAPINFO);
	bitmapinfo->bmiHeader.biWidth = width;
	bitmapinfo->bmiHeader.biHeight = -(int)height; /* Negative here to make this top-down bitmap, but do I really want this?? */
	bitmapinfo->bmiHeader.biPlanes = 1;
	bitmapinfo->bmiHeader.biBitCount = 32;
	bitmapinfo->bmiHeader.biCompression = BI_RGB;

	info->width = width;
	info->height = height;

	const size_t buffer_size = width * height * 4;
	info->buffer = malloc(buffer_size);
	memset(info->buffer, 0, buffer_size);
	/* Should just call renderer_clear_backbuffer */
	for (unsigned i = 0; i < buffer_size / 4; ++i)
		((int*)info->buffer)[i] = RGB_RED;
#endif
}

void renderer_destroy(struct renderer_info *info)
{
	assert(info && "renderer_destroy: info is NULL");

	free(get_backbuffer(info));
}


bool event_loop(void)
{
	MSG msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
			return FALSE;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return TRUE;
}

void finish_drawing(struct api_info *api_info)
{
	assert(api_info && "finish_drawing: api_info is NULL");

#if RPLNN_RENDERER == RPLNN_RENDERER_GDI
	RedrawWindow(api_info->hwnd, NULL, NULL, RDW_INTERNALPAINT | RDW_INVALIDATE);
#endif
}

void *get_backbuffer(struct renderer_info *info)
{
	assert(info && "get_backbuffer: info is NULL");

#if RPLNN_RENDERER == RPLNN_RENDERER_GDI
	return info->buffer;
#else
	return NULL;
#endif
}

uint32_t get_blit_duration_ms(struct renderer_info *info)
{
	assert(info && "get_blit_time_ms: info is NULL");

#if RPLNN_RENDERER == RPLNN_RENDERER_GDI
	return info->blit_duration_mus;
#else
	return 0;
#endif
}

struct vec2_int get_backbuffer_size(struct renderer_info *info)
{
	assert(info && "get_backbuffer_size: info is NULL");

#if RPLNN_RENDERER == RPLNN_RENDERER_GDI
	struct vec2_int size;
	size.x = info->width; 
	size.y = info->height;
#else
	struct vec2_int size = { .x = -1. .y = -1 };
#endif
	return size;
}

void renderer_clear_backbuffer(struct renderer_info *info, const uint32_t color)
{
#if RPLNN_RENDERER == RPLNN_RENDERER_GDI
	/* Optimize this */
	for (unsigned i = 0; i < info->width * info->height; ++i)
		((int*)info->buffer)[i] = color;
#endif
}

uint64_t get_time(void)
{
	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);
	return time.QuadPart;
}

uint64_t get_time_microseconds(const uint64_t time)
{
	LARGE_INTEGER Frequency;
	QueryPerformanceFrequency(&Frequency);

	uint64_t microseconds =	time * 1000000;
	microseconds /= Frequency.QuadPart;
	return microseconds;
}

bool uint64_to_string(const uint64_t value, char *buffer, const size_t buffer_size)
{
	return (_ui64toa_s(value, buffer, buffer_size, 10) == 0);
}

bool float_to_string(const float value, char *buffer, const size_t buffer_size)
{
	return snprintf(buffer, buffer_size, "%.3f", value) < (int)buffer_size;
}

static const uint8_t keycode_table[KEY_COUNT] = 
{ 
	0x41, /* A */
	0x42, /* B */
	0x43, /* C */
	0x44, /* D */
	0x45, /* E */
	0x46, /* F */
	0x47, /* G */
	0x48, /* H */
	0x49, /* I */
	0x4A, /* J */
	0x4B, /* K */
	0x4C, /* L */
	0x4D, /* M */
	0x4E, /* N */
	0x4F, /* O */
	0x50, /* P */
	0x51, /* Q */
	0x52, /* R */
	0x53, /* S */
	0x54, /* T */
	0x55, /* U */
	0x56, /* V */
	0x57, /* W */
	0x58, /* X */
	0x59, /* Y */
	0x5A  /* Z */
};

uint8_t get_virtual_key_code(enum keycodes keycode)
{
	assert(keycode < sizeof(keycode_table) / sizeof(keycode_table[0]) && "get_virtual_key_code: invalid keycode");
	return keycode_table[keycode];
}

bool is_key_down(struct api_info *api_info, enum keycodes keycode)
{
	uint8_t vkc = get_virtual_key_code(keycode);
	return (api_info->key_states[vkc / KEY_STATE_TYPE_BITS] & (1 << (vkc % KEY_STATE_TYPE_BITS))) != 0;
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	/* This works because GWLP_USERDATA defaults to 0 */
	struct api_info *api_info = (struct api_info*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	struct renderer_info *renderer_info = api_info ? api_info->renderer_info : NULL;

	switch (msg)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	case WM_KEYDOWN:
		assert(wParam <= MAX_KEYCODE && "WindowProc: Unexpected keycode");
		if (api_info && wParam <= MAX_KEYCODE)
			api_info->key_states[wParam / KEY_STATE_TYPE_BITS] |= (1 << (wParam % KEY_STATE_TYPE_BITS));
		return 0;
	case WM_KEYUP:
		assert(wParam <= MAX_KEYCODE && "WindowProc: Unexpected keycode");
		if (api_info && wParam <= MAX_KEYCODE)
			api_info->key_states[wParam / KEY_STATE_TYPE_BITS] &= ~(1 << (wParam % KEY_STATE_TYPE_BITS));
		return 0;
	case WM_PAINT:
#if RPLNN_RENDERER == RPLNN_RENDERER_GDI
		assert(renderer_info && "WindowProc: renderer_info is NULL");
		if (renderer_info)
		{
			uint64_t blit_start = get_time();
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			RECT rect;
			GetClientRect(hwnd, &rect);
			unsigned int dest_width = (unsigned)(rect.right - rect.left);
			unsigned int dest_height = (unsigned)(rect.bottom - rect.top);
			StretchDIBits(
				hdc,
				0, 0, dest_width, dest_height,
				0, 0, renderer_info->width, renderer_info->height,
				renderer_info->buffer,
				&(renderer_info->bitmapinfo),
				DIB_RGB_COLORS,
				SRCCOPY);

			EndPaint(hwnd, &ps);
			renderer_info->blit_duration_mus = (uint32_t)get_time_microseconds(get_time() - blit_start);
		}
#endif
		return 0;
	}

	return (DefWindowProc(hwnd, msg, wParam, lParam));
}

#pragma warning(push)
#pragma warning(disable : 4100) /* unreferenced formal parameter */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	struct renderer_info renderer_info;
#if RPLNN_RENDERER == RPLNN_RENDERER_GDI
	renderer_info.buffer = NULL;
#endif

	struct api_info api_info;
	api_info.hwnd = NULL;
	memset(api_info.key_states, 0, sizeof(api_info.key_states));
	api_info.renderer_info = &renderer_info;

	renderer_initialize(&renderer_info, 1280, 720);
	create_window(&api_info, hInstance, &renderer_info);

	main(&api_info, &renderer_info);

	renderer_destroy(&renderer_info);

	return TRUE;
}
#pragma warning(pop)

#endif
