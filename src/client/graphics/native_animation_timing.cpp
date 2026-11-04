#include "graphics/native_animation_timing.h"

namespace {
double fractional_milliseconds = 0;
float frame_seconds = 0;
uint32_t elapsed_milliseconds = 0;
bool sampled = false;
} // namespace

extern "C" void SHAIYA_CDECL native_world_animation_begin_frame(float elapsed_seconds) {
    frame_seconds = elapsed_seconds;
    sampled = false;
}

extern "C" uint32_t SHAIYA_STDCALL native_world_animation_elapsed(float speed_scale) {
    if (sampled)
        return elapsed_milliseconds;
    sampled = true;
    elapsed_milliseconds = 0;
    // Latch the scene's authored speed for this update, just as every instance
    // shares the same frame delta. Carry the remainder through speed changes.
    const double milliseconds =
        fractional_milliseconds + double(frame_seconds) * 1000.0 * double(speed_scale);
    if (milliseconds > 0 && milliseconds < 2147483647.0) {
        elapsed_milliseconds = uint32_t(milliseconds);
        fractional_milliseconds = milliseconds - double(elapsed_milliseconds);
    }
    return elapsed_milliseconds;
}
