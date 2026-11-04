#include "action_transition.h"
#include <cmath>
#include <stdexcept>
namespace genesis {
bool ActionTransition::request(std::uint32_t action, bool locked, bool already_handled) {
    if (locked && action != 9)
        return false;
    if (already_handled)
        return true;
    if (action == current || blending)
        return false;
    pending = action;
    phase = 0;
    blending = true;
    return true;
}
bool ActionTransition::advance_blend(float seconds) {
    if (!std::isfinite(seconds) || seconds < 0 || !std::isfinite(phase))
        throw std::invalid_argument("Invalid action transition time");
    if (!blending)
        return false;
    // The original compares the extended result before rounding the stored
    // phase to float, so a rounded phase of 5 may still commit this frame.
    long double next = static_cast<long double>(phase) + static_cast<long double>(seconds) * 30;
    phase = float(next);
    if (next <= 5)
        return false;
    current = pending;
    frame = 0;
    blending = false;
    return true;
}
} // namespace genesis
