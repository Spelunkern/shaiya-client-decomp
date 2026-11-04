#include "graphics/native_animation_timing.h"
#include <cstdint>

int main() {
    uint32_t total = 0;
    for (int i = 0; i < 4000; ++i) {
        native_world_animation_begin_frame(0.00025f);
        const auto interval = native_world_animation_elapsed(1.0f);
        if (native_world_animation_elapsed(5.0f) != interval) return 1;
        total += interval;
    }
    if (total < 999 || total > 1001) return 2;
    native_world_animation_begin_frame(0.08f);
    if (native_world_animation_elapsed(1.0f) < 80) return 3;
    return 0;
}
