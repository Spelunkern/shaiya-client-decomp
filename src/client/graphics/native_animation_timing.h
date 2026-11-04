#pragma once

#include <stdint.h>

#if defined(_WIN32) && defined(__i386__)
#define SHAIYA_CDECL __attribute__((cdecl))
#define SHAIYA_STDCALL __attribute__((stdcall))
#else
#define SHAIYA_CDECL
#define SHAIYA_STDCALL
#endif

// The application starts one animation interval per update. World animation
// consumers share its integer milliseconds without discarding sub-ms time.
extern "C" void SHAIYA_CDECL native_world_animation_begin_frame(float elapsed_seconds);
extern "C" uint32_t SHAIYA_STDCALL native_world_animation_elapsed(float speed_scale);
