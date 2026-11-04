#pragma once
#include "collision.h"
#include "movement.h"
#include <functional>
#include <optional>

namespace genesis {
// Default key bindings: `KEY%d` in the [OPTION] section of the settings file,
// falling back to the table at 0070a4f0 (DirectInput scancodes). The movement
// entries traced in 0043bfa0: A (index 0) turns left, D (5) turns right,
// W (1) walks forward, S (6) walks backward, Q (2) / E (7) strafe.
constexpr std::array<std::uint8_t, 20> default_key_bindings = {
    0x1e, 0x11, 0x10, 0x0f, 0x2e, 0x20, 0x1f, 0x12, 0x11, 0x39,
    0x14, 0x25, 0x16, 0x18, 0x2f, 0x17, 0x30, 0x22, 0x32, 0x01};
enum KeyBinding : unsigned { key_turn_left = 0, key_forward = 1, key_strafe_a = 2, key_turn_right = 5,
                             key_backward = 6, key_strafe_b = 7 };

struct MotionKeys {
    bool forward = false, backward = false, turn_left = false, turn_right = false;
};
struct MotionParameters {
    std::uint8_t speed_class = 2; // player +0x281
    bool mounted = false;         // player +900
    std::uint32_t mount_kind = 0; // player +0x388
    bool running = false;         // player +0x174; walking is half speed
    std::int32_t world_extent = 0;
};
// The camera pair at 022094c8 (eye) / 022094d4 (target); the heading is the
// player's +0x160 direction vector (y is kept zero).
struct MotionState {
    Vec3f position{};
    Vec3f heading{0, 0, 1};
    Vec3f eye{}, target{};
    float vertical_velocity = net::landing_velocity;
    bool in_water = false;
};
// Outdoor ground height (005a2c30); empty inside dungeons, where the vertical
// collision pass supplies the floor.
using GroundHeight = std::function<float(float x, float z)>;
// One frame of the keyboard movement of 0043bfa0: turning (speed * delta * 15
// degrees, rotating heading and camera about the target), forward at
// speed * delta (half while walking), backward at the class factor
// {0.5, 1, 1.5, 2, 2.5} * delta, then gravity, collision, ground clamp, the
// world clamp and camera follow.
void step_motion(MotionState& state, const MotionKeys& keys, const MotionParameters& parameters,
                 float frame_delta, const CollisionWorld& world, const GroundHeight& ground);
} // namespace genesis
