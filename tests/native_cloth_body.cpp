#include "cloth_body.h"
#include "cloth_particles.h"
#include "cloth.h"
#include "fixtures/cloth_body_x86.h"
#include "character_profile.h"
#include "action.h"
#include "animation_pose.h"
#include "archive.h"
#include "asset_path.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
using namespace genesis;
using Matrix = shaiya::graphics::native::Matrix;
void expect(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
std::uint32_t bits(float f) {
    std::uint32_t n;
    std::memcpy(&n, &f, 4);
    return n;
}
template <class F> void rejects(F fn) {
    bool failed = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    expect(failed, "Invalid cloth body input accepted");
}
float from_bits(std::uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}
int main(int argc, char** argv) {
    try {
        Matrix orientation = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
        orientation.m[3][0] = 1234;
        orientation.m[3][1] = -5678;
        expect(cloth_correction_direction(orientation) == std::array<float, 3>{0, 0, -1},
               "Cloth correction used translation or wrong local axis/sign");
        orientation.m[2][0] = 3;
        orientation.m[2][1] = 0;
        orientation.m[2][2] = 4;
        auto correction = cloth_correction_direction(orientation);
        expect(correction == std::array<float, 3>{-.6f, 0, -.8f},
               "Cloth correction did not normalize composed orientation");
        orientation.m[2][0] = orientation.m[2][2] = 0;
        rejects([&] { cloth_correction_direction(orientation); });
        std::vector<Matrix> finals(6);
        for (unsigned bone = 0; bone < 6; ++bone) {
            auto& m = finals[bone];
            for (unsigned axis = 0; axis < 4; ++axis)
                m.m[axis][axis] = 1;
            m.m[3][0] = bone + .125f;
            m.m[3][1] = 2 * bone + .25f;
            m.m[3][2] = -int(bone) + .5f;
        }
        Matrix world = {{{2, 0, 0, 0}, {0, 3, 0, 0}, {0, 0, 4, 0}, {10, 20, 30, 1}}};
        expect(cloth_correction_direction(finals, world) == std::array<float, 3>{0, 0, -1},
               "Cloth correction bone/world composition");
        auto rotated = finals;
        rotated[4].m[2][0] = 1;
        rotated[4].m[2][2] = 0;
        expect(cloth_correction_direction(rotated, world) == std::array<float, 3>{-1, 0, 0},
               "Cloth correction did not select bone four");
        rejects([&] { cloth_correction_direction(std::vector<Matrix>(4), world); });
        for (unsigned kind = 0; kind < 16; ++kind) {
            auto body = cloth_body(std::uint8_t(kind), finals, world);
            for (unsigned sphere = 0; sphere < 4; ++sphere) {
                for (unsigned axis = 0; axis < 3; ++axis)
                    expect(bits(body[sphere].center[axis]) ==
                               cloth_body_reference::shapes[kind][sphere][axis],
                           "Original x86 cloth sphere center mismatch");
                expect(bits(body[sphere].radius) == cloth_body_reference::shapes[kind][sphere][3],
                       "Original x86 cloth sphere radius mismatch");
            }
        }
        rejects([&] { cloth_body(16, finals, world); });
        rejects([&] { cloth_body(0, std::vector<Matrix>(5), world); });
        rejects([] { character_kind(4, 0, 0); });
        auto collision_finals = finals;
        for (unsigned bone = 0; bone < 6; ++bone) {
            collision_finals[bone].m[3][0] = float(2 * int(bone) - 2);
            collision_finals[bone].m[3][1] = collision_finals[bone].m[3][2] = 0;
        }
        unsigned case_index = 0;
        for (const auto& reference : cloth_body_reference::detection) {
            Matrix world = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
            if (reference.collapsed_world)
                world.m[0][0] = world.m[1][1] = world.m[2][2] = 0;
            auto body = cloth_body(std::uint8_t(reference.kind), collision_finals, world);
            ClothMesh mesh;
            for (const auto& point : reference.input) {
                ClothVertex v;
                for (unsigned axis = 0; axis < 3; ++axis)
                    v.position[axis] = from_bits(point[axis]);
                mesh.vertices.push_back(v);
            }
            ClothSetup setup;
            setup.rows = setup.columns = 1;
            ClothParticles particles(setup, mesh);
            for (unsigned i = 0; i < 4; ++i)
                if (reference.pinned & (1u << i))
                    particles.pin({{i, mesh.vertices[i].position}});
            auto detection =
                particles.detect_contacts(body, from_bits(reference.floor), reference.phase);
            if (detection.status != reference.status ||
                detection.records.size() != reference.count) {
                char message[180];
                std::snprintf(message, sizeof(message),
                              "Original detector case %u: status/count %d/%zu expected %d/%u",
                              case_index, detection.status, detection.records.size(),
                              reference.status, reference.count);
                throw std::runtime_error(message);
            }
            for (unsigned i = 0; i < 4; ++i)
                for (unsigned axis = 0; axis < 3; ++axis)
                    expect(bits(particles.particles()[i].position[axis]) ==
                               reference.position[i][axis],
                           "Original detector floor correction mismatch");
            for (unsigned record = 0; record < reference.count; ++record) {
                const auto& entry = detection.records[record];
                std::uint32_t actual[] = {std::uint32_t(entry.vertex),
                                          bits(entry.normal_or_center[0]),
                                          bits(entry.normal_or_center[1]),
                                          bits(entry.normal_or_center[2]), bits(entry.radius)};
                for (unsigned field = 0; field < 5; ++field)
                    if (actual[field] != reference.records[record][field]) {
                        char message[190];
                        std::snprintf(
                            message, sizeof(message),
                            "Original detector case %u record %u field %u: actual %08x expected %08x",
                            case_index, record, field, actual[field],
                            reference.records[record][field]);
                        throw std::runtime_error(message);
                    }
            }
            ++case_index;
        }
        expect(case_index == 42, "Original detector fixture coverage changed");
        if (argc > 1) {
            std::filesystem::path root(argv[1]);
            auto archive = shaiya::portable::Archive::open(root / "data.sah", root / "data.saf");
            unsigned moved = 0, collision_records = 0;
            for (unsigned race = 0; race < 4; ++race)
                for (unsigned gender = 0; gender < 2; ++gender)
                    for (unsigned role = 0; role < 2; ++role) {
                        auto job = role ? 4 : ((race == 1 || race == 2) ? 2 : 0);
                        auto prefix = character_profile(std::uint8_t(race), std::uint8_t(job),
                                                        std::uint8_t(gender));
                        auto actions = decode_actions(archive.read(prefix + "_action.alt"));
                        auto path =
                            asset_path::join(asset_path::join(asset_path::parent(prefix), "ANI"),
                                             actions.slots.at(0).animation);
                        AnimationPose pose(decode_animation(archive.read(path)));
                        auto kind = character_kind(std::uint8_t(race), std::uint8_t(job),
                                                   std::uint8_t(gender));
                        auto first = cloth_body(kind, pose.sample(0), world),
                             last = cloth_body(kind, pose.sample(12), world);
                        bool changed = false;
                        for (unsigned sphere = 0; sphere < 4; ++sphere) {
                            for (float v : last[sphere].center)
                                expect(std::isfinite(v), "Non-finite animated cloth body");
                            changed |= first[sphere].center != last[sphere].center;
                        }
                        auto cloth_root = asset_path::parent(prefix);
                        cloth_root.insert(5, "cloak/");
                        auto catalog = decode_cloth_catalog(archive.read(
                            asset_path::join(cloth_root, "1_" + asset_path::stem(prefix) + ".pc")));
                        const auto& setup = catalog.simulation.at(0);
                        auto mesh_path =
                            asset_path::join(asset_path::join(cloth_root, "3DC"),
                                             catalog.dynamic_meshes.at(setup.dynamic_mesh).name);
                        ClothParticles particles(setup, decode_cloth_mesh(archive.read(mesh_path)));
                        auto finals = pose.sample(0);
                        particles.pin(cloth_pins(setup, finals));
                        particles.external_forces({.75f, -.25f, .125f}, .2f);
                        expect(particles.spring_forces(),
                               "Animated original cloth spring stage failed");
                        particles.integrate(.001f);
                        Matrix identity = {
                            {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
                        auto detected =
                            particles.detect_contacts(cloth_body(kind, finals, identity), -100.f);
                        expect(detected.status >= -1 && detected.status <= 1,
                               "Invalid cloth collision status");
                        for (const auto& record : detected.records) {
                            expect(record.vertex >= 0 &&
                                       std::size_t(record.vertex) < particles.particles().size(),
                                   "Original cloth contact index exceeds particles");
                            for (float value : record.normal_or_center)
                                expect(std::isfinite(value),
                                       "Non-finite original cloth contact record");
                            ++collision_records;
                        }
                        if (detected.status == -1)
                            for (auto correction :
                                 {ClothCorrection::scaled, ClothCorrection::unscaled}) {
                                auto corrected = particles;
                                corrected.correct_penetrations(detected.records, {0, 0, -1},
                                                               correction);
                                for (const auto& particle : corrected.particles())
                                    for (float value : particle.position)
                                        expect(std::isfinite(value),
                                               "Non-finite original cloth penetration correction");
                            }
                        ClothParticles simulation(setup,
                                                  decode_cloth_mesh(archive.read(mesh_path)));
                        for (unsigned frame = 0; frame < 10; ++frame) {
                            auto animated = pose.sample(float(frame));
                            simulation.pin(cloth_pins(setup, animated));
                            ClothScene scene;
                            scene.body = cloth_body(kind, animated, identity);
                            scene.wind_direction = {.75f, -.25f, .125f};
                            scene.wind_strength = .2f;
                            scene.correction_direction = {0, 0, -1};
                            auto stats = simulation.advance(1.f / 60, 0, {0, 0, 0}, scene);
                            expect(stats.substeps == 4 && stats.attempts == (setup.mode ? 12u : 4u),
                                   "Original animated cloth frame cadence changed");
                            for (const auto& particle : simulation.particles())
                                for (float value : particle.position)
                                    expect(std::isfinite(value),
                                           "Non-finite full cloth simulation");
                        }
                        moved += changed;
                    }
            std::printf("Original animated cloth: %u profiles, %u collision records\n", moved,
                        collision_records);
            expect(moved == 16, "Original ANI did not move every cloth body profile");
        }
        std::puts("Native 16 body profiles and 42 contact detector cases match original x86");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
