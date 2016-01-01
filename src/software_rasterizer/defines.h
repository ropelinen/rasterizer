#ifndef RPLNN_DEFINES_H
#define RPLNN_DEFINES_H

/* A set of generic defines needed pretty much everywhere */

/* Disable useless warnings */
#if defined(_MSC_VER)
	#pragma warning(disable : 4115) /* named type definition in parentheses */
	#pragma warning(disable : 4514) /* unreferenced inline function has been removed */
	#pragma warning(disable : 4668) /* is not defined as a preprocessor macro, replacing with '0' for '#if/#elif' */
	#pragma warning(disable : 4710) /* The given function was selected for inline expansion, but the compiler did not perform the inlining. */
	#pragma warning(disable : 4711) /* function selected for automatic inline expansion */
	#pragma warning(disable : 4820) /* x bytes padding added after data member */
#endif

/* Other MSC definitions */
#if defined(_MSC_VER)
	#define WIN32_LEAN_AND_MEAN
	/* Don't want deprecated warnings for fopen and such */
	#define _CRT_SECURE_NO_WARNINGS 1
#endif

/* Generic includes */
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

/* Math defines */
#define PI 3.14159265f
#define RAD_TO_DEG(rad) (rad * (180.0f / PI))
#define DEG_TO_RAD(deg) (deg * (PI / 180.0f))

/* Platform defines 0x2 - 0x20 */
#define RPLNN_PLATFORM_WINDOWS 0x2

#if defined (_WIN32) || defined(_WIN64)
	#define RPLNN_PLATFORM RPLNN_PLATFORM_WINDOWS
#else
	#error "Platform not defined" 
#endif

/* Renderer defines 0x40 - 0x80*/
#define RPLNN_RENDERER_GDI 0x40

#define RPLNN_RENDERER RPLNN_RENDERER_GDI

#if !defined(RPLNN_RENDERER)
	#error "Renderer not defined"
#endif

/* Build configuration 0x100 - 0x110 */
#define RPLNN_DEBUG 0x100
#define RPLNN_RELEASE 0x105
#define RPLNN_PRODUCTION 0x110

#if defined (CONF_DEBUG)
	#define RPLNN_BUILD_TYPE RPLNN_DEBUG
#elif defined (CONF_RELEASE)
	#define RPLNN_BUILD_TYPE RPLNN_RELEASE
#elif defined (CONF_PRODUCTION)
	#define RPLNN_BUILD_TYPE RPLNN_PRODUCTION
#else
	#error "Build configuration not defined"
#endif

#endif /* RPLNN_DEFINES_H */
