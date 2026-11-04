#include "player_motion.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>
using namespace genesis;
static void expect(bool ok, const char* what) {
    if (!ok)
        throw std::runtime_error(what);
}
static bool near(float a, float b, float e = 1e-3f) {
    return std::fabs(a - b) <= e;
}
int main() {
    try {
        expect(default_key_bindings[key_forward] == 0x11 && default_key_bindings[key_backward] == 0x1f &&
                   default_key_bindings[key_turn_left] == 0x1e &&
                   default_key_bindings[key_turn_right] == 0x20,
               "default movement keys (W/S/A/D scancodes)");
        GroundHeight flat = [](float, float) { return 0.f; };
        MotionState state;
        state.position = {100, 0.9f, 100};
        state.eye = {100, 2, 95}; // behind the player, looking along +z
        state.target = {100, 1.4f, 100};
        MotionParameters walking;
        walking.world_extent = 2049;
        MotionKeys forward;
        forward.forward = true;
        step_motion(state, forward, walking, 1.f, nullptr, flat);
        expect(near(state.position[2], 102.5f) && near(state.position[0], 100), "walk is half speed");
        walking.running = true;
        step_motion(state, forward, walking, 1.f, nullptr, flat);
        expect(near(state.position[2], 107.5f), "run is full speed");
        MotionKeys back;
        back.backward = true;
        step_motion(state, back, walking, 1.f, nullptr, flat);
        expect(near(state.position[2], 106.f), "backward uses the class factor");
        expect(near(state.position[1], 0.9f), "ground clamp keeps the player on the floor");
        // Turning left by speed*delta*15 degrees = 75 degrees at speed 5.
        MotionState turning = state;
        turning.eye = {100, 2, 95};
        turning.target = {100, 1.4f, 100};
        MotionKeys left;
        left.turn_left = true;
        step_motion(turning, left, walking, 1.f, nullptr, flat);
        float angle = 75.f * 0.01745329238474369f;
        expect(near(turning.heading[0], -std::sin(angle)) && near(turning.heading[2], std::cos(angle)),
               "heading rotation");
        MotionState strafing = state;
        strafing.eye = {100, 2, 95};
        strafing.target = {100, 1.4f, 100};
        MotionKeys right;
        right.strafe_right = true;
        step_motion(strafing, right, walking, 1.f, nullptr, flat);
        expect(near(strafing.position[0], 101.5f) && near(strafing.position[2], state.position[2]),
               "E strafes right at the class factor");
        MotionKeys strafe_left;
        strafe_left.strafe_left = true;
        step_motion(strafing, strafe_left, walking, 1.f, nullptr, flat);
        expect(near(strafing.position[0], 100.f), "Q strafes left");
        // Fixed 0xff class does not move.
        MotionParameters frozen = walking;
        frozen.speed_class = 0xff;
        MotionState still = state;
        step_motion(still, forward, frozen, 1.f, nullptr, flat);
        expect(near(still.position[2], state.position[2]), "class 0xff is immobile");
        // Falling with a floor collision world lands on it.
        std::vector<Vec3f> floor = {{-50, 0, -50}, {-50, 0, 50}, {50, 0, 50}, {50, 0, -50}};
        std::vector<std::uint16_t> idx = {0, 1, 2, 0, 2, 3};
        CollisionWorld world = [&](CollisionPacket& p) { check_world_triangles(p, floor, idx); };
        MotionState drop;
        drop.position = {0, 4, 0};
        drop.eye = {0, 5, -5};
        drop.target = {0, 4.5f, 0};
        MotionParameters indoor;
        for (int i = 0; i < 120; ++i)
            step_motion(drop, {}, indoor, 1.f / 30, world, nullptr);
        expect(near(drop.position[1], 0.9f + .05f * .9f, 0.02f), "settles on the floor");
        std::puts("Native player motion passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
