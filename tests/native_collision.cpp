#include "collision.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include "archive.h"
using namespace genesis;
static void expect(bool ok, const char* what) {
    if (!ok)
        throw std::runtime_error(what);
}
static bool near(float a, float b, float e = 1e-3f) {
    return std::fabs(a - b) <= e;
}
// Large floor at y = 0 (facing up) and a wall at x = 5 (facing -x).
static CollisionWorld world_of(const std::vector<Vec3f>& vertices,
                               const std::vector<std::uint16_t>& indices) {
    return [=](CollisionPacket& packet) { check_world_triangles(packet, vertices, indices); };
}
int main(int argc, char** argv) {
    try {
        std::vector<Vec3f> floor = {{-50, 0, -50}, {-50, 0, 50}, {50, 0, 50}, {50, 0, -50}};
        std::vector<std::uint16_t> floor_indices = {0, 1, 2, 0, 2, 3};
        auto ground = world_of(floor, floor_indices);
        // Falling onto the floor stops 0.05 ellipsoid units above the contact.
        auto landed = move_with_collision(ground, {0, 2, 0}, {0, -3, 0});
        expect(near(landed[1], 0.9f + 0.05f * 0.9f, 2e-3f), "ground contact height");
        expect(near(landed[0], 0) && near(landed[2], 0), "ground contact drift");
        // Free movement above the floor is exact.
        auto free_move = move_with_collision(ground, {1, 5, 1}, {2, -1, 3});
        expect(near(free_move[0], 3) && near(free_move[1], 4) && near(free_move[2], 4),
               "unobstructed move");
        // A wall at x = 5 (triangles facing -x) stops a 0.3 radius player and
        // lets the z component slide.
        std::vector<Vec3f> wall = {{5, -50, -50}, {5, 50, -50}, {5, 50, 50}, {5, -50, 50}};
        std::vector<std::uint16_t> wall_indices = {0, 2, 1, 0, 3, 2};
        auto blocked = move_with_collision(world_of(wall, wall_indices), {4, 0, 0}, {2, 0, 2});
        expect(blocked[0] < 5.f - 0.3f + 1e-3f && blocked[0] > 4.5f, "wall stop");
        expect(near(blocked[2], 2, 0.05f), "wall slide");
        // Nothing found: the packet is untouched.
        auto none = move_with_collision([](CollisionPacket&) {}, {0, 0, 0}, {1, 2, 3});
        expect(near(none[0], 1) && near(none[1], 2) && near(none[2], 3), "empty world");
        // A back-facing triangle (moving away through its back) is ignored.
        auto behind = move_with_collision(ground, {0, 2, 0}, {0, 3, 0});
        expect(near(behind[1], 5), "upward move through the floor's front side");
        if (argc > 1) {
            std::filesystem::path root(argv[1]);
            auto archive = shaiya::portable::Archive::open(root / "data.sah", root / "data.saf");
            unsigned landed_count = 0, tried = 0;
            for (const char* name : {"A1_Dun1_1F.dg", "A1_Dun1_2F.dg", "A3_DUN1_1F.dg"}) {
            auto dungeon = decode_dungeon(archive.read(std::string("data/world/dungeon/") + name));
            CollisionWorld world = [&](CollisionPacket& packet) { check_dungeon(packet, dungeon); };
            for (const auto& node : dungeon.nodes) {
                for (std::size_t i = 0; i + 2 < node.collision_indices.size() && tried < 40; i += 3) {
                    const auto& a = node.collision_vertices[node.collision_indices[i]];
                    const auto& b = node.collision_vertices[node.collision_indices[i + 1]];
                    const auto& c = node.collision_vertices[node.collision_indices[i + 2]];
                    float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
                    float vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
                    float ny = uz * vx - ux * vz;
                    float nx = uy * vz - uz * vy, nz = ux * vy - uy * vx;
                    float len = std::sqrt(nx * nx + ny * ny + nz * nz);
                    if (len < 1e-3f || std::fabs(ny) / len < 0.99f)
                        continue; // only flat, large, floor-like triangles
                    float sign = ny > 0 ? 1.f : -1.f;
                    Vec3f centre = {(a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3,
                                    (a[2] + b[2] + c[2]) / 3};
                    ++tried;
                    auto result = move_with_collision(
                        world, {centre[0], centre[1] + sign * 3.f, centre[2]}, {0, -sign * 6.f, 0});
                    expect(std::isfinite(result[0]) && std::isfinite(result[1]) &&
                               std::isfinite(result[2]),
                           "dungeon move produced non-finite output");
                    if (std::fabs(result[1] - (centre[1] + sign * 3.f)) > 0.5f)
                        ++landed_count;
                }
            }
            }
            expect(tried > 0 && landed_count > 0, "no real dungeon floor stopped the player");
            std::printf("Dungeon collision: %u/%u floor probes stopped by real geometry\n",
                        landed_count, tried);
        }
        std::puts("Native ellipsoid collision sweep passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
