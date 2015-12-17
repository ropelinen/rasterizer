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

struct api_info
{
	HWND hwnd;
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

	api_info->hwnd = CreateWindowEx(
		0,
		class_name,
		window_name,
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		120, 120,
		(int)renderer_info->width, (int)renderer_info->height,
		NULL,
		NULL,
		hInstance,
		renderer_info); /* Need to pass stuff here, at least bitmap info for GDI */

	if (api_info->hwnd == NULL)
		error_popup("Failed to create a window", true);
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

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	struct renderer_info *renderer_info;
	if (msg == WM_CREATE)
	{
		/* Get mystery create parameter here and save it to WindowLongPtr */
		CREATESTRUCT *cs = (CREATESTRUCT*)lParam;
		renderer_info = (struct renderer_info*)cs->lpCreateParams;

		/* I think this could be set during the initialization. */
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)renderer_info);
	}
	else
	{
		/* Get stuff from long ptr here */
		renderer_info = (struct renderer_info*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	}

	switch (msg)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
#if RPLNN_RENDERER == RPLNN_RENDERER_GDI
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
#endif
		return 0;
	}

	return (DefWindowProc(hwnd, msg, wParam, lParam));
}

#pragma warning(push)
#pragma warning(disable : 4100) /* unreferenced formal parameter */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	struct api_info api_info = { .hwnd = NULL };
	struct renderer_info renderer_info;
#if RPLNN_RENDERER == RPLNN_RENDERER_GDI
	renderer_info.buffer = NULL;
#endif

	renderer_initialize(&renderer_info, 1280, 720);
	create_window(&api_info, hInstance, &renderer_info);

	main(&api_info, &renderer_info);

	renderer_destroy(&renderer_info);

	return TRUE;
}
#pragma warning(pop)

#endif