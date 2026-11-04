#pragma once
#include <cstdint>
namespace genesis {
// Typed counterpart of the actor's current/pending action, sampled frame and
// five-phase transition state. Table/resource existence is checked by its owner.
struct ActionTransition {
    std::uint32_t current = 0, pending = 0;
    float frame = 0, phase = 0;
    bool blending = false;
    // 004105b0: an earlier special handler may accept without starting a
    // blend. The owner supplies that result after its action-specific work.
    bool request(std::uint32_t action, bool locked = false, bool already_handled = false);
    // 0041b3a6: 30 phases/second; commit strictly after phase five. The sampled
    // source frame stays frozen during a blend and becomes zero on commit.
    bool advance_blend(float seconds);
};
} // namespace genesis
