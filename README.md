# A simple cpu rasterizer
- Using SIMD and x64 is recommended
- Only use x86 with no SIMD optimizations as a last resort
- Functions do not check for NULL pointers, it is the responsibility of the caller (there are asserts for that though)
- Contains a visual studio project which has been tested on VS2013 and VS2015
- VS2012 is not supported as the project uses C99 (mainly mixed code and variables)
- Use Production build configuraion, it's the most optimized one
- The required texture and a font are provided in the bin folder

## Features
- Depth buffer
- Perspective correct texturing
- 4bit sub-pixel precision
- Guard-band clipping
- Threading support
- SIMD implementation (SSE2)
- Render target and depth buffer tiling

## To-do
- Generic optimizations

## Maybe later
- Proper z-clipping
- 8bit sub-pixel precision
- Stencil buffer
- Restore support for vertex colors
- Mipmaps