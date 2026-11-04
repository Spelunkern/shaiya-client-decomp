#pragma once
#include "dungeon.h"
#include <array>
#include <functional>

namespace genesis {
using Vec3f = std::array<float, 3>;
// The sweep state of the original ellipsoid collision packet (`0x13fed0` in the
// game object): everything is expressed in ellipsoid space (world / radius).
struct CollisionPacket {
    Vec3f radius{0.3f, 0.9f, 0.3f};        // +0x30..+0x38 (3e99999a, 3f666666, 3e99999a)
    Vec3f velocity{}, normalized_velocity{}; // +0x54.., +0x60..
    Vec3f base{};                            // +0x6c..
    bool found = false;                      // +0x78
    double nearest = 0;                      // +0x80
    Vec3f intersection{};                    // +0x88..
};
// 005a0ef0: sphere-versus-triangle sweep (plane, vertices, edges) updating the
// packet's nearest hit. Triangles are in ellipsoid space.
void check_triangle(CollisionPacket& packet, const Vec3f& a, const Vec3f& b, const Vec3f& c);
// 005a1920: feed a world-space triangle list, scaling every vertex by 1/radius.
void check_world_triangles(CollisionPacket& packet, const std::vector<Vec3f>& vertices,
                           const std::vector<std::uint16_t>& indices);
// Calls check_triangle for the collision geometry of every dungeon node whose
// collision bounds touch the swept ellipsoid (conservative replacement for the
// octree box tests of 005a4750; the triangle test itself is the original).
void check_dungeon(CollisionPacket& packet, const Dungeon& dungeon);
// 00437b60: recursive slide. `depth` is the original's non-zero recursion flag
// (at most six passes, counted in `0071cffc`). Position and velocity are in
// ellipsoid space; `world` fills `packet.found/nearest/intersection` for the
// prepared sweep (it receives the packet with base/velocity already set).
using CollisionWorld = std::function<void(CollisionPacket&)>;
Vec3f collide_and_slide(const CollisionWorld& world, const Vec3f& radius, Vec3f base,
                        Vec3f velocity, bool recurse, int& pass_counter);
// 004383d0: world-space move. First a horizontal pass (vertical velocity
// forced to zero, result never allowed below the start height), then a
// separate vertical pass with the original vertical velocity and no
// recursion. Returns the new world position.
Vec3f move_with_collision(const CollisionWorld& world, const Vec3f& position,
                          const Vec3f& velocity, bool recurse = true,
                          const Vec3f& radius = {0.3f, 0.9f, 0.3f});
} // namespace genesis
