#include "player_motion.h"
#include <cmath>

namespace genesis {
namespace {
constexpr float degrees_to_radians = 0.01745329238474369f; // 006be948
Vec3f horizontal_direction(const Vec3f& from, const Vec3f& to) {
    float x = to[0] - from[0], z = to[2] - from[2];
    float length = std::sqrt(x * x + z * z);
    if (length == 0.f)
        return {0, 0, 0};
    return {x / length, 0, z / length};
}
void rotate(MotionState& state, float degrees) {
    long double angle = static_cast<long double>(degrees) * degrees_to_radians;
    long double c = std::cos(angle), s = std::sin(angle);
    float hx = state.heading[0], hz = state.heading[2];
    state.heading[0] = float(c * hx - s * hz);
    state.heading[2] = float(s * hx + c * hz);
    float dx = state.eye[0] - state.target[0], dz = state.eye[2] - state.target[2];
    state.eye[0] = state.target[0] + float(c * dx - s * dz);
    state.eye[2] = float(s * dx + c * dz) + state.target[2];
}
float backward_factor(std::uint8_t speed_class) {
    switch (speed_class) {
    case 0: return .5f;
    case 1: return 1.f;
    case 2: return 1.5f;
    case 3: return 2.f;
    case 4: return 2.5f;
    default: return 0.f;
    }
}
} // namespace

void step_motion(MotionState& state, const MotionKeys& keys, const MotionParameters& parameters,
                 float delta, const CollisionWorld& world, const GroundHeight& ground) {
    const float speed = net::movement_speed(parameters.speed_class, parameters.mounted,
                                            parameters.mount_kind);
    const float step = (parameters.running ? speed : speed * .5f) * delta;
    if (keys.turn_left)
        rotate(state, step * 15.f);
    if (keys.turn_right)
        rotate(state, step * -15.f);
    Vec3f move{0, 0, 0};
    if (parameters.speed_class != 0xff) {
        auto dir = horizontal_direction(state.eye, state.target);
        if (keys.forward)
            for (int axis = 0; axis < 3; ++axis)
                move[axis] += step * dir[axis];
        if (keys.backward) {
            float factor = (parameters.mounted ? 2.f : backward_factor(parameters.speed_class)) * delta;
            for (int axis = 0; axis < 3; ++axis)
                move[axis] -= factor * dir[axis];
        }
        float side = (parameters.mounted ? 2.f : backward_factor(parameters.speed_class)) * delta;
        // cross(up, dir) = (dir.z, 0, -dir.x): +x (right) for a forward +z view.
        if (keys.strafe_right) {
            move[0] += side * dir[2];
            move[2] -= side * dir[0];
        }
        if (keys.strafe_left) {
            move[0] -= side * dir[2];
            move[2] += side * dir[0];
        }
    }
    state.vertical_velocity = net::fall_velocity_step(state.vertical_velocity, delta);
    Vec3f velocity = {move[0], state.vertical_velocity * delta, move[2]};
    Vec3f previous = state.position;
    state.position = world ? move_with_collision(world, state.position, velocity)
                           : Vec3f{state.position[0] + velocity[0], state.position[1] + velocity[1],
                                   state.position[2] + velocity[2]};
    if (ground) {
        float floor = ground(state.position[0], state.position[2]) + 0.9f;
        if (state.position[1] <= floor) {
            state.position[1] = floor;
            state.vertical_velocity = net::landing_velocity;
        }
    } else if (state.position[1] > previous[1] + velocity[1] - 1e-4f && velocity[1] < 0)
        state.vertical_velocity = net::landing_velocity; // stopped by a floor
    if (parameters.world_extent > 0) {
        auto bounds = net::clamp_to_world(state.position, parameters.world_extent);
        state.position = bounds.position;
        state.in_water = bounds.in_water;
    }
    // Camera follow: both ends move with the player and the target sits
    // 1.3 - 0.8 above the position (00708c94, 006bca00).
    Vec3f follow = {state.position[0] - previous[0], state.position[1] - previous[1],
                    state.position[2] - previous[2]};
    for (int axis = 0; axis < 3; ++axis)
        state.eye[axis] += follow[axis];
    state.target = {state.position[0], state.position[1] + (1.3f - .8f), state.position[2]};
}
} // namespace genesis
