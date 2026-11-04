#include "cloth.h"
#include "cloth_particles.h"
#include "archive.h"
#include "asset_path.h"
#include "skin.h"
#include "action.h"
#include "animation_pose.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <set>
#include <limits>
#include <stdexcept>
using Bytes = std::vector<std::uint8_t>;
void expect(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
void u32(Bytes& b, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(std::uint8_t(n >> (8 * i)));
}
void f32(Bytes& b, float v) {
    std::uint32_t bits;
    std::memcpy(&bits, &v, 4);
    u32(b, bits);
}
template <class F> void rejects(F fn) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    expect(rejected, "Malformed cloth data accepted");
}
int main(int argc, char** argv) {
    try {
        Bytes mesh;
        u32(mesh, 4);
        mesh.insert(mesh.end(), {'a', 0xe9, 0, 0});
        u32(mesh, 3);
        for (unsigned i = 0; i < 3; ++i) {
            for (float v : {float(i), 1.f, 2.f})
                f32(mesh, v);
            u32(mesh, 0x7fc12345);
            u32(mesh, 0xffab1234); // Preserve, never validate as bone indices.
            for (float v : {0.f, 1.f, 0.f, .25f, .75f})
                f32(mesh, v);
        }
        u32(mesh, 1);
        mesh.insert(mesh.end(), {0, 0, 1, 0, 2, 0});
        auto decoded = genesis::decode_cloth_mesh(mesh);
        expect(decoded.embedded_name == std::string("a\xe9", 2) && decoded.vertices.size() == 3 &&
                   decoded.vertices[2].position[0] == 2 && decoded.vertices[0].uv[1] == .75f &&
                   decoded.vertices[0].attributes ==
                       std::array<std::uint32_t, 2>{0x7fc12345, 0xffab1234} &&
                   decoded.indices == std::vector<std::uint16_t>{0, 1, 2},
               "Cloth mesh fields/opaque bits");
        for (std::size_t i = 0; i < mesh.size(); ++i)
            rejects([&] { genesis::decode_cloth_mesh(Bytes(mesh.begin(), mesh.begin() + i)); });
        auto bad = mesh;
        bad[bad.size() - 2] = 3;
        rejects([&] { genesis::decode_cloth_mesh(bad); });
        bad = mesh;
        bad.push_back(0);
        rejects([&] { genesis::decode_cloth_mesh(bad); });
        bad = mesh;
        bad[0] = 255;
        bad[1] = 255;
        rejects([&] { genesis::decode_cloth_mesh(bad); });
        bad = mesh;
        bad[14] = 0x80;
        bad[15] = 0x7f;
        rejects([&] { genesis::decode_cloth_mesh(bad); });
        Bytes pc;
        Bytes ctl;
        u32(ctl, 2);
        ctl.resize(516, 0);
        std::memcpy(ctl.data() + 4, "cape.dds", 8);
        ctl[260] = 0xe9;
        auto textures = genesis::decode_cloth_textures(ctl);
        expect(textures == std::vector<std::string>{"cape.dds", std::string(1, char(0xe9))},
               "Cloth texture fixed-width names/legacy bytes");
        const char* race_codes[] = {"HU", "EL", "VI", "DE"};
        for (std::uint8_t race = 0; race < 4; ++race) {
            auto paths = genesis::cloth_texture_paths(race);
            expect(paths.directory == std::string("data/cloak/ClothTexture/") + race_codes[race] &&
                       paths.catalog == std::string("data/cloak/ClothTexture/CLOTH_TEXTHRE_") +
                                            race_codes[race] + ".CTL",
                   "Cloth texture race jump-table mapping");
        }
        rejects([] { genesis::cloth_texture_paths(4); });
        rejects([] { genesis::cloth_texture_paths(255); });
        for (std::size_t i = 0; i < ctl.size(); ++i)
            rejects([&] { genesis::decode_cloth_textures(Bytes(ctl.begin(), ctl.begin() + i)); });
        auto bad_ctl = ctl;
        bad_ctl.push_back(0);
        rejects([&] { genesis::decode_cloth_textures(bad_ctl); });
        for (auto count : {0u, 0xffffffffu, 0x7fffffffu}) {
            Bytes invalid;
            u32(invalid, count);
            rejects([&] { genesis::decode_cloth_textures(invalid); });
        }
        auto reference = [&](unsigned id, const char* name) {
            u32(pc, 1);
            u32(pc, id);
            auto p = pc.size();
            pc.resize(p + 128, 0);
            std::memcpy(pc.data() + p, name, std::strlen(name));
        };
        reference(9, "cloth.3DC");
        u32(pc, 1);
        for (unsigned i = 0; i < 524; ++i)
            pc.push_back(std::uint8_t(i));
        reference(17, "static.3DC");
        auto catalog = genesis::decode_cloth_catalog(pc);
        expect(catalog.dynamic_meshes[0].id == 9 && catalog.static_meshes[0].id == 17 &&
                   catalog.dynamic_meshes[0].name == "cloth.3DC" &&
                   catalog.static_meshes[0].name == "static.3DC" &&
                   catalog.simulation[0].dynamic_mesh == 0x03020100 &&
                   catalog.simulation[0].reserved == 0x2b2a2928 &&
                   catalog.simulation[0].anchors[0].control == 0x2f2e2d2c,
               "Cloth catalog fields");
        auto signed_pc = pc;
        for (auto offset :
             {std::size_t(140 + 12), std::size_t(140 + 44 + 4), std::size_t(140 + 44 + 8)})
            for (unsigned byte = 0; byte < 4; ++byte)
                signed_pc[offset + byte] = 255;
        auto signed_setup = genesis::decode_cloth_catalog(signed_pc).simulation[0];
        expect(signed_setup.static_mesh == -1 && signed_setup.anchors[0].vertex == -1 &&
                   signed_setup.anchors[0].bone == -1,
               "Cloth signed sentinel decoding");
        std::uint32_t last_bits;
        std::memcpy(&last_bits, &catalog.simulation[0].anchors[19].offset[2], 4);
        expect(last_bits == 0x0b0a0908, "Cloth setup final word was lost");
        genesis::ClothSetup setup;
        setup.columns = 2;
        setup.rows = 1;
        setup.anchors[0] = {0xabcd0001, 2, 1, {2, 3, 4}};
        setup.anchors[1] = {1, -1, -1, {0, 0, 0}};
        setup.anchors[2] = {1, 2, 0, {1, 2, 3}};
        setup.anchors[3].control = 2;
        setup.anchors[4] = {1, 0, 999, {0, 0, 0}}; // Prefix already ended.
        using Matrix = shaiya::graphics::native::Matrix;
        std::vector<Matrix> finals = {
            {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {-1, 0, 0, 1}}},
            {{{2, 0, 0, 0}, {0, 3, 0, 0}, {0, 0, 4, 0}, {10, 20, 30, 1}}}};
        auto pins = genesis::cloth_pins(setup, finals);
        expect(pins.size() == 2 && pins[0].vertex == 2 &&
                   pins[0].position == std::array<float, 3>{14, 29, 46} &&
                   pins[1].position == std::array<float, 3>{0, 2, 3},
               "Cloth prefix/negative/duplicate/bone-transform contract");
        auto nonfinite = setup;
        nonfinite.anchors[0].offset[0] = std::numeric_limits<float>::quiet_NaN();
        rejects([&] { genesis::cloth_pins(nonfinite, finals); });
        nonfinite = setup;
        nonfinite.anchors[4].offset[0] = std::numeric_limits<float>::quiet_NaN();
        expect(genesis::cloth_pins(nonfinite, finals).size() == 2,
               "Inactive cloth anchor data was consumed");
        auto invalid = setup;
        invalid.anchors[0].vertex = 6;
        rejects([&] { genesis::cloth_pins(invalid, finals); });
        invalid = setup;
        invalid.anchors[0].bone = 2;
        rejects([&] { genesis::cloth_pins(invalid, finals); });
        invalid = setup;
        invalid.columns = invalid.rows = 0xffffffff;
        rejects([&] { genesis::cloth_pins(invalid, finals); });
        for (std::size_t i = 0; i < pc.size(); ++i)
            rejects([&] { genesis::decode_cloth_catalog(Bytes(pc.begin(), pc.begin() + i)); });
        pc.push_back(0);
        rejects([&] { genesis::decode_cloth_catalog(pc); });
        if (argc > 1) {
            std::filesystem::path root(argv[1]);
            auto archive = shaiya::portable::Archive::open(root / "data.sah", root / "data.saf");
            std::size_t texture_count = 0;
            std::array<std::vector<std::string>, 4> race_textures;
            for (std::uint8_t race = 0; race < 4; ++race) {
                auto paths = genesis::cloth_texture_paths(race);
                auto names = genesis::decode_cloth_textures(archive.read(paths.catalog));
                for (const auto& name : names) {
                    expect(!name.empty(), "Empty original cape texture name");
                    expect(!archive.read(genesis::asset_path::join(paths.directory, name)).empty(),
                           "Missing cape texture from CTL");
                    ++texture_count;
                }
                race_textures[race] = std::move(names);
            }
            std::printf("Original cloth texture catalogs: %zu texture references resolved\n",
                        texture_count);
            std::set<std::string> dynamic, statics;
            unsigned catalogs = 0, setups_checked = 0, pins_checked = 0, moved_pins = 0;
            for (const auto& path : archive.paths()) {
                if (path.compare(0, 11, "data/cloak/") ||
                    genesis::asset_path::extension(path) != ".pc")
                    continue;
                auto table = genesis::decode_cloth_catalog(archive.read(path));
                auto filename = genesis::asset_path::stem(path);
                auto separator = filename.find('_');
                expect(separator != std::string::npos, "Cloth profile filename");
                auto character_root = genesis::asset_path::parent(path);
                character_root.erase(5, 6); // data/cloak/Character -> data/Character
                auto profile =
                    genesis::asset_path::join(character_root, filename.substr(separator + 1));
                auto directory = genesis::asset_path::stem(character_root);
                for (auto& c : directory)
                    if (c >= 'A' && c <= 'Z')
                        c += 32;
                expect(directory == "human" || directory == "elf" || directory == "vile" ||
                           directory == "deatheater",
                       "Unknown original cloth character directory");
                unsigned race = directory == "human"  ? 0
                                : directory == "elf"  ? 1
                                : directory == "vile" ? 2
                                                      : 3;
                auto actions = genesis::decode_actions(archive.read(profile + "_action.alt"));
                auto animation = genesis::decode_animation(archive.read(
                    genesis::asset_path::join(genesis::asset_path::join(character_root, "ANI"),
                                              actions.slots.at(0).animation)));
                genesis::AnimationPose pose(std::move(animation));
                for (const auto& setup : table.simulation) {
                    expect(setup.texture < race_textures[race].size(),
                           "Cloth setup texture selector exceeds racial CTL");
                    expect(setup.dynamic_mesh < table.dynamic_meshes.size(),
                           "Cloth setup mesh selector");
                    auto mesh_path = genesis::asset_path::join(
                        genesis::asset_path::join(genesis::asset_path::parent(path), "3DC"),
                        table.dynamic_meshes[setup.dynamic_mesh].name);
                    auto mesh = genesis::decode_cloth_mesh(archive.read(mesh_path));
                    expect((std::uint64_t(setup.columns) + 1) * (std::uint64_t(setup.rows) + 1) ==
                               mesh.vertices.size(),
                           "Cloth grid and mesh dimensions disagree");
                    auto pins = genesis::cloth_pins(setup, pose.sample(0));
                    genesis::ClothParticles particles(setup, mesh);
                    particles.pin(pins);
                    for (unsigned step = 1; step <= 5; ++step) {
                        auto anchors = genesis::cloth_pins(setup, pose.sample(float(step)));
                        particles.pin(anchors);
                        particles.external_forces({.75f, -.25f, .125f}, .2f);
                        expect(particles.spring_forces(), "Original cloth spring stage failed");
                        particles.integrate(.001f);
                        for (const auto& anchor : anchors)
                            expect(particles.particles()[anchor.vertex].position == anchor.position,
                                   "Environment integration moved a cloth anchor");
                        for (const auto& particle : particles.particles())
                            for (float value : particle.position)
                                expect(std::isfinite(value), "Non-finite cloth environment step");
                    }

                    expect(particles.particles().size() == mesh.vertices.size(),
                           "Cloth particle owner size");
                    auto moved = genesis::cloth_pins(setup, pose.sample(12));
                    expect(!pins.empty() && pins.size() == moved.size(), "Missing cloth anchors");
                    ++setups_checked;
                    pins_checked += unsigned(pins.size());
                    for (std::size_t i = 0; i < pins.size(); ++i)
                        if (pins[i].position != moved[i].position)
                            ++moved_pins;
                }
                ++catalogs;
                auto parent = genesis::asset_path::parent(path);
                for (const auto& mesh : table.dynamic_meshes)
                    dynamic.insert(genesis::asset_path::join(
                        genesis::asset_path::join(parent, "3DC"), mesh.name));
                for (const auto& mesh : table.static_meshes)
                    statics.insert(genesis::asset_path::join(
                        genesis::asset_path::join(parent, "3DC/Static"), mesh.name));
            }
            for (const auto& path : dynamic) {
                auto cloth = genesis::decode_cloth_mesh(archive.read(path));
                expect(!cloth.vertices.empty() && !cloth.indices.empty(), "Missing cloth topology");
            }
            for (const auto& path : statics)
                (void)genesis::decode_skin(archive.read(path));
            expect(catalogs == 128 && dynamic.size() == 48, "Cloak archive coverage changed");
            expect(moved_pins > 0, "Original ANI did not move any cloth anchor");
            std::printf(
                "Original cloth: %u catalogs, %zu dynamic meshes, %zu static meshes decoded; %u setups, %u pins, %u animated pins\n",
                catalogs, dynamic.size(), statics.size(), setups_checked, pins_checked, moved_pins);
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
