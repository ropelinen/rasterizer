# A simple cpu rasterizer
- Using SIMD and x64 is recommended
- Only use x86 with no SIMD optimizations as a last resort
- Functions do not check for NULL pointers, it is the responsibility of the caller (there are asserts for that though)

## Features
- Depth buffer
- Perspective correct texturing
- 4bit sub-pixel precision
- Guard-band clipping
- Threading support
- SIMD implementation (SSE2)

## To-do
- Generic optimizations

## Maybe later
- Proper z-clipping
- 8bit sub-pixel precision
- Stencil buffer
- Restore support for vertex colors
- Mipmaps