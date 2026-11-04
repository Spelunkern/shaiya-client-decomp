#include "terrain.h"
#include "terrain_resource.h"
#include "archive.h"
#include "world.h"
#include "fixtures/terrain_x86.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <stdexcept>
float from_bits(std::uint32_t n) {
    float f;
    std::memcpy(&f, &n, 4);
    return f;
}
std::uint32_t bits(float f) {
    std::uint32_t n;
    std::memcpy(&n, &f, 4);
    return n;
}
int main(int argc, char** argv) {
    try {
        if (genesis::terrain_dimension(2048, 2.f) != 1025 ||
            genesis::terrain_dimension(17, 2.f) != 9)
            throw std::runtime_error("Terrain dimension conversion failed");
        for (auto input : {std::pair<std::int32_t, float>{0, 2.f}, {2048, 0.f}, {-1, 2.f}}) {
            bool rejected = false;
            try {
                genesis::terrain_dimension(input.first, input.second);
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            if (!rejected)
                throw std::runtime_error("Invalid terrain dimension input accepted");
        }
        std::vector<std::uint8_t> prefix(265);
        prefix[0] = 'F';
        prefix[1] = 'L';
        prefix[2] = 'D';
        prefix[4] = 0x34;
        prefix[5] = 0x12;
        prefix[263] = 0xa5;
        auto header = genesis::decode_terrain_prefix(prefix);
        if (header.field != 0x1234 || header.bytes_consumed != 8)
            throw std::runtime_error("FLD prefix decoding failed");
        bool bad_prefix = false;
        try {
            genesis::decode_terrain_prefix(std::vector<std::uint8_t>(7));
        } catch (const std::invalid_argument&) {
            bad_prefix = true;
        }
        if (!bad_prefix)
            throw std::runtime_error("Truncated FLD prefix accepted");
        {
            std::vector<std::uint8_t> dun(0x104, 0xcc);
            const char name[] = "A.dg";
            dun[0] = 'D';
            dun[1] = 'U';
            dun[2] = 'N';
            dun[3] = 0;
            std::memcpy(&dun[4], name, sizeof name);
            if (genesis::decode_world_kind(dun) != genesis::WorldKind::dungeon)
                throw std::runtime_error("DUN tag not recognised");
            auto d = genesis::decode_dungeon_descriptor(dun);
            if (d.name != "A.dg" || d.path != "data/world/dungeon/A.dg" || d.bytes_consumed != 0x104)
                throw std::runtime_error("Dungeon descriptor decoding failed");
            auto expect_throw = [](auto&& f, const char* what) {
                try {
                    f();
                } catch (const std::invalid_argument&) {
                    return;
                }
                throw std::runtime_error(what);
            };
            auto unterminated = dun;
            std::fill(unterminated.begin() + 4, unterminated.end(), 'x');
            expect_throw([&] { genesis::decode_dungeon_descriptor(unterminated); },
                         "Unterminated dungeon name accepted");
            auto truncated = dun;
            truncated.resize(0x103);
            expect_throw([&] { genesis::decode_dungeon_descriptor(truncated); },
                         "Truncated dungeon descriptor accepted");
            auto bad_tag = dun;
            bad_tag[0] = 'X';
            expect_throw([&] { genesis::decode_world_kind(bad_tag); }, "Bad world tag accepted");
            expect_throw([&] { genesis::decode_dungeon_descriptor(prefix); },
                         "FLD accepted as dungeon");
        }
        {
            std::vector<std::uint8_t> wtr(16 + 3 * 0x100);
            wtr[2] = 0x80;
            wtr[3] = 0x42;
            wtr[4] = 0x11;
            wtr[8] = 150;
            wtr[12] = 3;
            const char a[] = "B_caust00.jpg";
            std::memcpy(&wtr[16], a, sizeof a);
            const char c[] = "x.tga";
            std::memcpy(&wtr[16 + 0x200], c, sizeof c);
            auto water = genesis::decode_water_definition(wtr);
            if (water.tile_size != 64.f || water.argb != 0x11 || water.frame_period != 150 ||
                water.frames.size() != 3 || water.frames[0] != "B_caust00.dds" ||
                !water.frames[1].empty() || water.frames[2] != "x.dds")
                throw std::runtime_error("Water definition decoding failed");
            auto short_file = wtr;
            short_file.pop_back();
            auto bad = [](auto&& f) {
                try {
                    f();
                } catch (const std::invalid_argument&) {
                    return true;
                }
                return false;
            };
            if (!bad([&] { genesis::decode_water_definition(short_file); }) ||
                !bad([&] { genesis::decode_water_definition({}); }))
                throw std::runtime_error("Truncated water definition accepted");
            std::vector<std::uint8_t> name(0x100);
            if (!genesis::decode_terrain_water_name(name, 0).empty() ||
                !bad([&] { genesis::decode_terrain_water_name(name, 1); }))
                throw std::runtime_error("Terrain water name bounds wrong");
        }
        {
            genesis::WaterFrameClock clock;
            clock.period = 150;
            clock.frame_count = 3;
            clock.advance(100);
            if (clock.frame != 0 || clock.elapsed != 100)
                throw std::runtime_error("Water clock advanced early");
            clock.advance(50);
            if (clock.frame != 1 || clock.elapsed != 0)
                throw std::runtime_error("Water clock did not step at the period");
            clock.advance(300);
            clock.advance(300);
            if (clock.frame != 0)
                throw std::runtime_error("Water clock did not wrap");
            if (genesis::water_frame_step(.05f) != 100 || genesis::water_frame_step(0.f) != 0)
                throw std::runtime_error("Water frame step mismatch");
            auto plane = genesis::basic_water_plane(64.f);
            if (plane.uv_extent != 16.f || plane.min_corner != -2048.f || plane.max_corner != 3072.f)
                throw std::runtime_error("Water plane extents mismatch");
        }
        if (argc > 1) {
            std::filesystem::path root(argv[1]);
            auto archive = shaiya::portable::Archive::open(root / "data.sah", root / "data.saf");
            auto source = archive.read(genesis::net::world_map_path(0));
            auto actual_header = genesis::decode_terrain_prefix(source);
            if (actual_header.field != 2048 || actual_header.bytes_consumed != 8)
                throw std::runtime_error("Original FLD prefix differs from archive");
            auto decoded =
                genesis::load_terrain_field(archive, genesis::net::world_map_path(0), 2.f);
            auto dimension = decoded.dimension;
            auto materials = genesis::decode_terrain_materials(
                source, decoded.prefix.bytes_consumed + decoded.grid.bytes_consumed);
            if (materials.size() != 6 || materials[0].texture != "R1_Base.tga" ||
                materials[0].sound != "ch_walke001.wav" ||
                materials[0].parameter_bits != 0x40800000)
                throw std::runtime_error("Original terrain material record mismatch");
            std::size_t missing_materials = 0;
            for (auto index : decoded.grid.attributes)
                if (index >= materials.size())
                    ++missing_materials;
            if (missing_materials != 593)
                throw std::runtime_error("Original terrain material sentinel coverage changed");
            auto actual_terrain =
                genesis::TerrainHeights::from_grid(dimension, std::move(decoded.grid));
            auto actual_height = actual_terrain.world_height(200.f, 200.f, .5f, .02f, -200.f, 50.f);
            if (!std::isfinite(actual_height))
                throw std::runtime_error("Original FLD terrain sample was not finite");
            std::printf("World 0 candidate grid: %u x %u, sample height %.3f\n", dimension,
                        dimension, actual_height);
            {
                auto name = genesis::decode_terrain_water_name(
                    source, decoded.prefix.bytes_consumed + decoded.grid.bytes_consumed + 4 +
                                516 * materials.size());
                if (name != "B1_Water.wtr")
                    throw std::runtime_error("World 0 water name mismatch");
                auto water = genesis::decode_water_definition(
                    archive.read("data/entity/water/" + name));
                if (water.tile_size != 64.f || water.argb != 0xd8ffffff ||
                    water.frame_period != 150 || water.frames.size() != 32 ||
                    water.frames[0] != "B_caust00.dds" ||
                    !archive.find("data/entity/water/" + water.frames[31]))
                    throw std::runtime_error("B1_Water.wtr differs from the recovered contract");
            }
            unsigned fields = 0, dungeons = 0, grids = 0, with_water = 0, no_water = 0;
            for (const auto& path : archive.paths()) {
                if (path.rfind("data/world/", 0) != 0 || path.size() < 4 ||
                    path.substr(path.size() - 4) != ".wld")
                    continue;
                auto world = archive.read(path);
                if (world.size() >= 4 && world[0] == 'F' && world[1] == 'L' && world[2] == 'D' &&
                    world[3] == 0) {
                    auto field = genesis::decode_terrain_prefix(world);
                    if (field.bytes_consumed != 8)
                        throw std::runtime_error("Unexpected FLD prefix length");
                    try {
                        auto candidate = genesis::decode_terrain_field(world, 2.f);
                        if (candidate.grid.heights.size() ==
                            std::size_t(candidate.dimension) * candidate.dimension)
                            ++grids;
                        auto records = genesis::decode_terrain_materials(
                            world, candidate.prefix.bytes_consumed + candidate.grid.bytes_consumed);
                        auto water_name = genesis::decode_terrain_water_name(
                            world, candidate.prefix.bytes_consumed + candidate.grid.bytes_consumed +
                                       4 + 516 * records.size());
                        if (water_name.empty())
                            ++no_water;
                        else {
                            auto definition = genesis::decode_water_definition(
                                archive.read("data/entity/water/" + water_name));
                            for (const auto& frame : definition.frames)
                                if (!archive.find("data/entity/water/" + frame))
                                    throw std::runtime_error("Water frame missing: " + frame);
                            ++with_water;
                        }
                    } catch (const std::invalid_argument&) {
                        // A field may use different dimensions or a different stream layout.
                    }
                    ++fields;
                } else if (world.size() >= 4 && world[0] == 'D' && world[1] == 'U' &&
                           world[2] == 'N' && world[3] == 0) {
                    auto descriptor = genesis::decode_dungeon_descriptor(world);
                    if (!archive.find(descriptor.path))
                        throw std::runtime_error("Dungeon descriptor names a missing DG: " +
                                                 descriptor.path);
                    ++dungeons;
                }
            }
            if (!fields)
                throw std::runtime_error("No original FLD worlds found");
            if (with_water != 31 || no_water != 2)
                throw std::runtime_error("Field water definition coverage changed");
            std::printf("Original world prefixes: %u FLD, %u DUN; %u candidate grids fit\n", fields,
                        dungeons, grids);
        }
        auto grid =
            genesis::decode_terrain_grid(2, {0x34, 0x12, 0xff, 0xff, 0, 0, 1, 0, 4, 5, 6, 7, 99});
        if (grid.heights != std::vector<std::uint16_t>{0x1234, 65535, 0, 1} ||
            grid.attributes != std::vector<std::uint8_t>{4, 5, 6, 7} || grid.bytes_consumed != 12)
            throw std::runtime_error("Terrain grid record decoding failed");
        auto decoded_field = genesis::TerrainHeights::from_grid(2, grid);
        std::vector<genesis::TerrainMaterialRecord> material_table(7);
        if (genesis::terrain_surface_at(grid, 2, material_table, 2.f, 0.f, .5f) !=
                &material_table[5] ||
            genesis::terrain_surface_at(grid, 2, material_table, 0.f, 2.f, .5f) !=
                &material_table[6])
            throw std::runtime_error("Terrain material world-coordinate selection failed");
        if (genesis::terrain_attribute(grid, 2, 1.9f, .2f) != 5 ||
            genesis::terrain_attribute(grid, 2, 0, 1) != 6 ||
            genesis::terrain_attribute(grid, 2, 2, 1) != 0)
            throw std::runtime_error("Terrain attribute coordinates or boundary failed");
        if (decoded_field.extent() != 2)
            throw std::runtime_error("Decoded terrain extent was not retained");
        if (decoded_field.sample(0, 0) != 0x1234 || decoded_field.sample(1, 0) != 65535)
            throw std::runtime_error("Decoded terrain heights were not available to queries");
        std::vector<std::uint8_t> field_bytes = {'F',  'L', 'D', 0, 2, 0, 0, 0, 0x34, 0x12, 0xff,
                                                 0xff, 0,   0,   1, 0, 4, 5, 6, 7,    99};
        auto parsed_field = genesis::decode_terrain_field(field_bytes, 2.f);
        if (parsed_field.dimension != 2 || parsed_field.grid.heights != grid.heights ||
            parsed_field.grid.attributes != grid.attributes ||
            parsed_field.grid.bytes_consumed != 12)
            throw std::runtime_error("FLD header and terrain grid integration failed");
        field_bytes.resize(19);
        bool incomplete_field = false;
        try {
            genesis::decode_terrain_field(field_bytes, 2.f);
        } catch (const std::invalid_argument&) {
            incomplete_field = true;
        }
        if (!incomplete_field)
            throw std::runtime_error("Truncated FLD terrain data accepted");
        bool truncated_grid = false;
        try {
            genesis::decode_terrain_grid(2, std::vector<std::uint8_t>(11));
        } catch (const std::invalid_argument&) {
            truncated_grid = true;
        }
        if (!truncated_grid)
            throw std::runtime_error("Truncated terrain grid accepted");
        for (std::size_t length = 0; length < 12; ++length) {
            bool rejected = false;
            try {
                genesis::decode_terrain_grid(2, std::vector<std::uint8_t>(length));
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            if (!rejected)
                throw std::runtime_error("Incomplete terrain block accepted");
        }
        bool oversized = false;
        try {
            genesis::decode_terrain_grid(0xffffffffu, {});
        } catch (const std::invalid_argument&) {
            oversized = true;
        }
        if (!oversized)
            throw std::runtime_error("Overflowing terrain dimension accepted");
        auto scale = genesis::terrain_scale(2.f, .5f, 7.25f);
        auto initial = genesis::default_terrain_scale();
        if (initial.horizontal != 2.f || initial.vertical != .02f || initial.offset != -200.f ||
            initial.inverse_horizontal != .5f || initial.inverse_vertical != 50.f)
            throw std::runtime_error("Original startup terrain scale failed");
        if (scale.inverse_horizontal != .5f || scale.inverse_vertical != 2.f ||
            scale.offset != 7.25f)
            throw std::runtime_error("Terrain scale initialization failed");
        std::vector<std::uint16_t> samples(21);
        for (unsigned i = 0; i < samples.size(); ++i)
            samples[i] = std::uint16_t(i * 3201);
        genesis::TerrainHeights field(4, samples);
        genesis::TerrainHeights flat(4, std::vector<std::uint16_t>(21, 100));
        genesis::TerrainHeights mesh_heights(5, std::vector<std::uint16_t>(25, 100));
        auto cell =
            genesis::terrain_cell_mesh(mesh_heights, 0, 0, genesis::default_terrain_scale());
        if (cell.vertices[2].position[0] != 8.f || cell.vertices[2].position[2] != 8.f ||
            cell.vertices[4].position[1] != -198.f || cell.indices[0] != 0 || cell.indices[1] != 4)
            throw std::runtime_error("Terrain cell geometry or scaling failed");
        if (flat.world_height(1, 1, 1, 1, 1e20f, 1e20f) != 1e20f)
            throw std::runtime_error("Interior terrain query evaluated unused outside height");
        if (flat.world_height(1, 1, .5f, 2.f, 7.25f, .5f) != 207.25f ||
            flat.world_height(-1, 1, 1.f, 2.f, 7.25f, .5f) != 1.25f)
            throw std::runtime_error("Terrain world scaling or outside conversion failed");
        std::vector<std::uint16_t> varied(73);
        for (unsigned i = 0; i < varied.size(); ++i)
            varied[i] = std::uint16_t(i * i * 3201 + i * 913);
        genesis::TerrainHeights terrain(8, varied);
        auto slope_mesh =
            genesis::terrain_cell_mesh(terrain, 0, 0, genesis::default_terrain_scale());
        for (unsigned triangle = 0; triangle < 4; ++triangle) {
            const auto& a = slope_mesh.vertices[slope_mesh.indices[triangle * 3]];
            const auto& b = slope_mesh.vertices[slope_mesh.indices[triangle * 3 + 1]];
            const auto& c = slope_mesh.vertices[slope_mesh.indices[triangle * 3 + 2]];
            auto x = (a.position[0] + b.position[0] + c.position[0]) / 3.f;
            auto z = (a.position[2] + b.position[2] + c.position[2]) / 3.f;
            auto expected_y = (a.position[1] + b.position[1] + c.position[1]) / 3.f;
            auto sampled_y = terrain.world_height(x, z, .5f, .02f, -200.f, 50.f);
            if (std::abs(sampled_y - expected_y) > .001f)
                throw std::runtime_error("Terrain geometry disagrees with collision height");
        }
        unsigned world_queries = 0;
        for (const auto& reference : terrain_reference::world_cases) {
            auto actual = terrain.world_height(from_bits(reference[0]), from_bits(reference[1]),
                                               .5f, 2.f, 7.25f, 2.f);
            if (bits(actual) != reference[2]) {
                std::fprintf(stderr, "World terrain query %u: %08x != original %08x\n",
                             world_queries, bits(actual), reference[2]);
                return 1;
            }
            ++world_queries;
        }
        std::printf("Native world terrain: %u original x86 cases matched\n", world_queries);
        unsigned queries = 0;
        for (const auto& reference : terrain_reference::height_cases) {
            auto actual = terrain.height(from_bits(reference[0]), from_bits(reference[1]), -14);
            if (bits(actual) != reference[2]) {
                std::fprintf(stderr, "Terrain query %u: %08x != original %08x\n", queries,
                             bits(actual), reference[2]);
                return 1;
            }
            ++queries;
        }
        std::printf("Native terrain triangle selection: %u original x86 cases matched\n", queries);
        for (const auto& reference : terrain_reference::sample_cases)
            if (field.sample(reference[0], reference[1]) != float(reference[2]))
                throw std::runtime_error("Terrain sample clamp/pitch differs from original x86");
        genesis::TerrainHeights exact_grid(4, std::vector<std::uint16_t>(16, 12));
        if (exact_grid.sample(3, 3) != 12)
            throw std::runtime_error("Original square terrain allocation rejected");
        bool edge_rejected = false;
        try {
            exact_grid.sample(4, 4);
        } catch (const std::out_of_range&) {
            edge_rejected = true;
        }
        if (!edge_rejected)
            throw std::runtime_error("Terrain read beyond actual square allocation");
        for (auto extent : {0u, 5u, 0xffffffffu}) {
            bool rejected = false;
            try {
                genesis::TerrainHeights invalid(extent, std::vector<std::uint16_t>(20));
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            if (!rejected)
                throw std::runtime_error("Terrain accepted incomplete/invalid sample extent");
        }
        unsigned count = 0;
        for (const auto& reference : terrain_reference::cases) {
            std::array<float, 3> a, b, c;
            for (unsigned i = 0; i < 3; ++i) {
                a[i] = from_bits(reference[i]);
                b[i] = from_bits(reference[i + 3]);
                c[i] = from_bits(reference[i + 6]);
            }
            auto result = genesis::terrain_triangle_height(a, b, c, from_bits(reference[9]),
                                                           from_bits(reference[10]));
            if (bits(result) != reference[11]) {
                std::fprintf(stderr, "Terrain case %u: %08x != original %08x\n", count,
                             bits(result), reference[11]);
                return 1;
            }
            ++count;
        }
        std::printf("Native terrain plane height: %u original x86 cases matched bit for bit\n",
                    count);
        std::puts("Native terrain sampling: 64 original x86 boundary cases matched");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
