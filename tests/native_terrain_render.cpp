#include "terrain_resource.h"
#include "world_objects.h"
#include "math.h"
#include "image.h"
#include "asset_path.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>
int main(int argc, char** argv) {
    try {
        genesis::Renderer renderer(128, 128, true);
        genesis::SceneUniforms scene;
        scene.view = genesis::look_at_lh({2, 5, -3}, {2, 0, 2}, {0, 1, 0});
        scene.view_projection =
            genesis::multiply(scene.view, genesis::perspective_lh(1, 1, .1f, 100));
        renderer.scene(scene);
        genesis::TerrainHeights heights(9, std::vector<std::uint16_t>(81, 0));
        auto cell = genesis::terrain_cell_mesh(heights, 0, 0, genesis::terrain_scale(1, 1, 0));
        const unsigned char pixel[] = {20, 220, 30, 255};
        auto texture = renderer.texture_rgba(1, 1, pixel);
        renderer.clear({0, 0, 0, 1});
        genesis::draw_terrain_cell(renderer, cell, texture, genesis::identity);
        auto pixels = renderer.read_pixels();
        renderer.clear({0, 0, 0, 1});
        genesis::draw_terrain_region(renderer, heights, 0, 0, 2, 2, genesis::terrain_scale(1, 1, 0),
                                     texture, genesis::identity);
        if (renderer.read_pixels() == pixels)
            throw std::runtime_error("Terrain region did not draw additional cells");
        for (auto size : {0u, 13108u}) {
            bool rejected = false;
            try {
                genesis::draw_terrain_region(renderer, heights, 0, 0, size, 1,
                                             genesis::terrain_scale(1, 1, 0), texture,
                                             genesis::identity);
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            if (!rejected)
                throw std::runtime_error("Invalid terrain region accepted");
        }
        unsigned visible = 0;
        for (std::size_t i = 0; i < pixels.size(); i += 4)
            if (pixels[i + 1] > 100 && pixels[i] < 80)
                ++visible;
        if (visible < 300 || renderer.api().GetError() != GL_NO_ERROR)
            throw std::runtime_error("Terrain cell did not render correctly");
        if (argc > 1) {
            std::filesystem::path root(argv[1]);
            auto archive = shaiya::portable::Archive::open(root / "data.sah", root / "data.saf");
            auto field = genesis::load_terrain_field(archive, "data/world/0.wld", 2.f);
            auto world_bytes = archive.read("data/world/0.wld");
            auto materials = genesis::decode_terrain_materials(
                world_bytes, field.prefix.bytes_consumed + field.grid.bytes_consumed);
            if (materials.empty())
                throw std::runtime_error("Original field has no material records");
            for (const auto& material : materials) {
                auto decoded = genesis::decode_image(
                    archive.read(genesis::terrain_material_texture_path(material)));
                if (!decoded.width || !decoded.height || decoded.pixels.empty())
                    throw std::runtime_error("Original terrain material texture is empty");
            }
            auto texture_path = genesis::terrain_material_texture_path(materials[0]);
            auto image = genesis::decode_image(archive.read(texture_path));
            auto detail_texture = renderer.texture_rgba(int(image.width), int(image.height),
                                                        image.pixels.data(), true);
            auto actual =
                genesis::TerrainHeights::from_grid(field.dimension, std::move(field.grid));
            auto y = actual.world_height(200, 200, .5f, .02f, -200, 50);
            scene.view = genesis::look_at_lh({208, y + 35, 170}, {208, y, 208}, {0, 1, 0});
            scene.view_projection =
                genesis::multiply(scene.view, genesis::perspective_lh(1, 1, .1f, 200));
            renderer.scene(scene);
            renderer.clear({0, 0, 0, 1});
            genesis::draw_terrain_region(renderer, actual, 25, 25, 2, 2,
                                         genesis::default_terrain_scale(), detail_texture,
                                         genesis::identity);
            auto real = renderer.read_pixels();
            unsigned actual_visible = 0;
            for (std::size_t i = 0; i < real.size(); i += 4)
                if (real[i] > 20 || real[i + 1] > 20 || real[i + 2] > 20)
                    ++actual_visible;
            renderer.release_texture(detail_texture);
            if (actual_visible < 50)
                throw std::runtime_error("Original field region did not render");
            std::printf("Original field terrain: %u visible pixels\n", actual_visible);
            genesis::TerrainResource uncentered(renderer, archive, "data/world/0.wld");
            genesis::TerrainResource centered(renderer, archive, "data/world/0.wld",
                                              genesis::default_terrain_scale(),
                                              std::array<float, 2>{0.f, 0.f});
            if (!uncentered.mesh_count() || !centered.mesh_count())
                throw std::runtime_error("TerrainResource produced no batches");
            auto recover_center = [](const genesis::Matrix& world) {
                return std::array<float, 2>{-world.m[3][0] / world.m[0][0],
                                            -world.m[3][2] / world.m[2][2]};
            };
            auto center_a = recover_center(uncentered.fitted_world());
            auto center_b = recover_center(centered.fitted_world());
            if (std::hypot(center_b[0], center_b[1]) >= std::hypot(center_a[0], center_a[1]))
                throw std::runtime_error(
                    "Centered terrain region was not closer to the requested world position");
            {
                auto tail = genesis::decode_world_tail(
                    world_bytes,
                    field.prefix.bytes_consumed + field.grid.bytes_consumed +
                        4 + 516 * materials.size() + 0x100,
                    true);
                genesis::WorldObjects objects(renderer, archive, tail);
                if (objects.dungeon_placement_count() != 2)
                    throw std::runtime_error("World 0 embedded dungeons were not resolved");
                // Draw near the first embedded fortress so it loads and renders.
                objects.draw({44.f, 1848.f}, 120.f, genesis::identity);
                if (!objects.drawn_last())
                    throw std::runtime_error("Embedded fortress was not drawn");
                unsigned probes = 0, blocked = 0;
                for (const auto& placement : objects.placements()) {
                    if (placement.list != 0 || probes >= 150)
                        continue;
                    ++probes;
                    float x = placement.world.m[3][0], y = placement.world.m[3][1],
                          z = placement.world.m[3][2];
                    genesis::CollisionWorld world = [&](genesis::CollisionPacket& packet) {
                        objects.collide(packet);
                    };
                    auto end = genesis::move_with_collision(world, {x - 25, y + 1.5f, z},
                                                            {50, 0, 0});
                    if (!std::isfinite(end[0]) || !std::isfinite(end[1]) || !std::isfinite(end[2]))
                        throw std::runtime_error("World object collision produced non-finite output");
                    if (end[0] < x + 24)
                        ++blocked;
                }
                if (!probes || !blocked)
                    throw std::runtime_error("No world building blocked a horizontal sweep");
                std::printf("World object collision: %u/%u building probes blocked\n", blocked,
                            probes);
            }
        }
        renderer.release_texture(texture);
        std::printf("OpenGL terrain cell: %u visible pixels\n", visible);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
