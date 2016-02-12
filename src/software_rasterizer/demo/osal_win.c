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

/* Missing C99 features before VS2015 */
#if defined(_MSC_VER) && _MSC_VER < 1900
/* Taken from http://stackoverflow.com/questions/2915672/snprintf-and-visual-studio-2010 */
#define snprintf c99_snprintf
#define vsnprintf c99_vsnprintf
int c99_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap)
{
	int count = -1;

	if (size != 0)
		count = _vsnprintf_s(outBuf, size, _TRUNCATE, format, ap);
	if (count == -1)
		count = _vscprintf(format, ap);

	return count;
}
int c99_snprintf(char *outBuf, size_t size, const char *format, ...)
{
	int count;
	va_list ap;

	va_start(ap, format);
	count = c99_vsnprintf(outBuf, size, format, ap);
	va_end(ap);

	return count;
}
#endif

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

struct thread
{
	CRITICAL_SECTION data_critical_section;
	CRITICAL_SECTION doing_task_critical_section;
	HANDLE handle;
	HANDLE sleep_semaphore; 
	void (*func)(void *);
	void *data;
	DWORD id;
	bool quit;
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
	bitmapinfo->bmiHeader.biHeight = height;
	bitmapinfo->bmiHeader.biPlanes = 1;
	bitmapinfo->bmiHeader.biBitCount = 32;
	bitmapinfo->bmiHeader.biCompression = BI_RGB;

	info->width = width;
	info->height = height;

	const size_t buffer_size = width * height * 4;
	info->buffer = malloc(buffer_size);
	memset(info->buffer, 0, buffer_size);
	
	renderer_clear_backbuffer(info, RGB_RED);
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
	assert(info && "get_blit_duration_ms: info is NULL");

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
	assert(info && "renderer_clear_backbuffer: info is NULL");

#if RPLNN_RENDERER == RPLNN_RENDERER_GDI
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

unsigned int get_logical_core_count(void)
{
	/* from https://msdn.microsoft.com/en-us/library/ms684139%28v=vs.85%29.aspx */
	//IsWow64Process is not available on all supported versions of Windows.
	//Use GetModuleHandle to get a handle to the DLL that contains the function
	//and GetProcAddress to get a pointer to the function if available.
	typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	LPFN_ISWOW64PROCESS IsWow64 = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");

	BOOL is_wow64 = false;
	if (IsWow64)
	{
		if (!IsWow64(GetCurrentProcess(), &is_wow64))
		{
			error_popup("Couldn't get wow64 information, assuming single core system", false);
			return 1;
		}
	}

	SYSTEM_INFO sys_info;
	if (is_wow64)
		GetNativeSystemInfo(&sys_info);
	else
		GetSystemInfo(&sys_info);

	return sys_info.dwNumberOfProcessors;
}

bool uint64_to_string(const uint64_t value, char *buffer, const size_t buffer_size)
{
	assert(buffer && "uint64_to_string: buffer is NULL");
	assert(buffer_size && "uint64_to_string: buffer_size is 0");

	return (_ui64toa_s(value, buffer, buffer_size, 10) == 0);
}

bool float_to_string(const float value, char *buffer, const size_t buffer_size)
{
	assert(buffer && "float_to_string: buffer is NULL");
	assert(buffer_size && "float_to_string: buffer_size is 0");

	return snprintf(buffer, buffer_size, "%.3f", value) < (int)buffer_size;
}

#pragma warning(push)
#pragma warning(disable: 4127) /* conditional expression is constant */
DWORD WINAPI thread_func(LPVOID lpParam)
{
	struct thread *me = (struct thread *)lpParam;

	while(true)
	{
		/* When there is nothing to do just wait here.
		 * We'll be signaled when a task is added. */
		if (WaitForSingleObject(me->sleep_semaphore, INFINITE) == WAIT_FAILED)
		{
			assert(false && "thread_func: WaitForSingleObject in thread loop failed, killing the thread");
			return GetLastError();
		}

		EnterCriticalSection(&me->doing_task_critical_section);
		EnterCriticalSection(&me->data_critical_section);
		if (me->quit)
		{
			LeaveCriticalSection(&me->data_critical_section);
			LeaveCriticalSection(&me->doing_task_critical_section);
			return 0;
		}
		void(*func)(void *) = me->func; 
		void *data = me->data;
		LeaveCriticalSection(&me->data_critical_section);
		
		if (func)
		{
			(*func)(data);

			EnterCriticalSection(&me->data_critical_section);
			me->func = NULL;
			me->data = NULL;
			LeaveCriticalSection(&me->data_critical_section);
		}
		else
		{
			assert(false && "Some one woke up the thread without a task when not quitting, shouldn't happen");
		}

		LeaveCriticalSection(&me->doing_task_critical_section);
	}

	return 0;
}
#pragma warning(pop)

struct thread *thread_create(const int core_id)
{
	struct thread *thread = malloc(sizeof(struct thread));

	thread->sleep_semaphore = CreateSemaphore(NULL, 0, 100, NULL);
	if (!thread->sleep_semaphore)
	{
		assert(false && "thread_create: failed to create has_tasks semaphore");
		free(thread);
		return NULL;
	}

	thread->func = NULL;
	thread->data = NULL;
	thread->quit = false;

	thread->handle = CreateThread(NULL, 0, &thread_func, thread, 0, &thread->id);
	if (!thread->handle)
	{
		CloseHandle(thread->sleep_semaphore);
		free(thread);
		return NULL;
	}

	if (core_id >= 0 && !SetThreadAffinityMask(thread->handle, (DWORD_PTR)(1 << core_id)))
	{
		assert(false && "thread_create: failed to set the desired core");
		thread->quit = true;

		if (!ReleaseSemaphore(thread->sleep_semaphore, 1, NULL))
		{
			assert(false && "thread_create: failed to release the sleep semaphore, trying to terminate the thread");
			TerminateThread(thread->handle, GetLastError());
		}
		WaitForSingleObject(thread->handle, INFINITE);
		CloseHandle(thread->sleep_semaphore);
		CloseHandle(thread->handle);
		free(thread);

		return NULL;
	}

	InitializeCriticalSectionAndSpinCount(&thread->data_critical_section, 0x00000400);
	InitializeCriticalSectionAndSpinCount(&thread->doing_task_critical_section, 0x00000400);

	return thread;
}

void thread_destroy(struct thread **thread)
{
	assert(thread && "thread_destroy: thread is NULL");
	assert(*thread && "thread_destroy: *thread is NULL");
	assert((*thread)->handle && "thread_destroy: (*thread)->handle is NULL");
	assert((*thread)->sleep_semaphore && "thread_destroy: (*thread)->sleep_semaphore is NULL");

	EnterCriticalSection(&(*thread)->data_critical_section);
	(*thread)->quit = true;
	LeaveCriticalSection(&(*thread)->data_critical_section);

	if (!ReleaseSemaphore((*thread)->sleep_semaphore, 1, NULL))
	{
		assert(false && "thread_destroy: failed to release the sleep semaphore, trying to terminate the thread");
		TerminateThread((*thread)->handle, GetLastError());
	}
	WaitForSingleObject((*thread)->handle, INFINITE);

	CloseHandle((*thread)->handle);
	DeleteCriticalSection(&(*thread)->data_critical_section);
	DeleteCriticalSection(&(*thread)->doing_task_critical_section);
	CloseHandle((*thread)->sleep_semaphore);

	free(*thread);
	*thread = NULL;
}

bool thread_set_task(struct thread *thread, void(*func)(void *), void *data)
{
	assert(thread && "thread_set_task: thread is NULL");
	assert(func && "thread_set_task: func is NULL");

	bool success = true;
	EnterCriticalSection(&thread->data_critical_section);
	assert(!thread->quit && "thread_set_task: thread is quitting/has quit");
	if (thread->func || thread->quit)
	{
		success = false;
	}
	else
	{
		thread->func = func;
		thread->data = data;
	}
	LeaveCriticalSection(&thread->data_critical_section);

	if (success)
	{
		if (!ReleaseSemaphore(thread->sleep_semaphore, 1, NULL))
		{
			assert(false && "thread_set_task: Failed to release the sleep semaphore, the task won't be run");
			EnterCriticalSection(&thread->data_critical_section);
			thread->func = NULL;
			thread->data = NULL;
			LeaveCriticalSection(&thread->data_critical_section);
			return false;
		}
	}

	return success;
}

bool thread_has_task(struct thread *thread)
{
	assert(thread && "thread_in_task: thread is NULL");

	bool has_task;
	EnterCriticalSection(&thread->data_critical_section);
	has_task = thread->func;
	LeaveCriticalSection(&thread->data_critical_section);

	return has_task;
}

void thread_wait_for_task(struct thread *thread)
{
	assert(thread && "thread_wait_for_task: thread is NULL");

	if (!thread_has_task(thread))
	{
		/* If the thread doesn't have a task function set we can just get out here */
		return;
	}

	EnterCriticalSection(&thread->doing_task_critical_section);
	LeaveCriticalSection(&thread->doing_task_critical_section);

	/* If this function is called immediately after setting the task there is a small chance that
	 * the thread has not got far enough to enter doing_task_critical_section.
	 * This loop fixes that as the task is removed after completion. */
	while(thread_has_task(thread))
	{
		Sleep(0);
	}
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
	assert(api_info && "is_key_down: api_info is NULL");

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
	if (!SetProcessAffinityMask(GetCurrentProcess(), (1 << get_logical_core_count()) - 1))
		error_popup("Failed to set process affinity mask, threading might not work properly", false);

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
