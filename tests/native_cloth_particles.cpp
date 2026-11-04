#include "cloth_particles.h"
#include "cloth_geometry.h"
#include "fixtures/cloth_particles_x86.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <limits>
#include <stdexcept>
using namespace genesis;
void expect(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void rejects(F fn) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected, "Invalid cloth particle operation accepted");
}
std::uint32_t bits(float f) {
    std::uint32_t result;
    std::memcpy(&result, &f, 4);
    return result;
}
int main() {
    try {
        ClothMesh mesh;
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 4; ++column) {
                ClothVertex vertex;
                vertex.position = {.25f * (column + 1), 1.f + .5f * row, .1f * (row - column)};
                vertex.uv = {.25f * column, .5f * row};
                mesh.vertices.push_back(vertex);
            }
        for (unsigned mode = 0; mode < 2; ++mode) {
            ClothSetup setup;
            setup.rows = 2;
            setup.columns = 3;
            setup.mode = mode;
            ClothParticles state(setup, mesh);
            expect(state.springs().size() == 42, "Original spring count");
            for (unsigned i = 0; i < state.springs().size(); ++i) {
                const auto& spring = state.springs()[i];
                std::uint32_t values[] = {
                    std::uint32_t(spring.first / 4),  std::uint32_t(spring.first % 4),
                    std::uint32_t(spring.second / 4), std::uint32_t(spring.second % 4),
                    bits(spring.stiffness),           bits(spring.damping),
                    bits(spring.rest_length)};
                for (unsigned field = 0; field < 7; ++field)
                    expect(values[field] == cloth_reference::topology[mode][i][field],
                           "Original spring topology/order/rest length mismatch");
            }
            std::vector<std::array<float, 3>> forces;
            for (unsigned i = 0; i < 12; ++i) {
                forces.push_back({.75f + i * .125f, -3.5f, .2f * (i % 3)});
                expect(state.particles()[i].position == mesh.vertices[i].position &&
                           state.particles()[i].uv == mesh.vertices[i].uv,
                       "Cloth initial positions/UVs");
            }
            state.forces(forces);
            state.pin({{0, mesh.vertices[0].position}, {3, mesh.vertices[3].position}});
            state.integrate(.008f);
            state.integrate(.016f);
            for (unsigned i = 0; i < 12; ++i) {
                const auto& p = state.particles()[i];
                float values[] = {p.mass,           p.position[0],     p.position[1],
                                  p.position[2],    p.velocity[0],     p.velocity[1],
                                  p.velocity[2],    p.acceleration[0], p.acceleration[1],
                                  p.acceleration[2]};
                for (unsigned field = 0; field < 10; ++field)
                    if (bits(values[field]) != cloth_reference::data[mode][i][field]) {
                        char message[180];
                        std::snprintf(
                            message, sizeof(message),
                            "Original x86 cloth mismatch: mode %u particle %u field %u actual %08x expected %08x",
                            mode, i, field, bits(values[field]),
                            cloth_reference::data[mode][i][field]);
                        throw std::runtime_error(message);
                    }
            }
            state.external_forces({.75f, -.25f, .125f}, 2.5f);
            expect(state.spring_forces(), "Spring force unexpectedly failed");
            for (unsigned i = 0; i < 12; ++i)
                for (unsigned axis = 0; axis < 3; ++axis) {
                    auto actual = bits(state.particles()[i].force[axis]);
                    auto expected = cloth_reference::spring_force[mode][i][axis];
                    if (actual != expected) {
                        char message[180];
                        std::snprintf(
                            message, sizeof(message),
                            "Original spring force mismatch: mode %u particle %u axis %u actual %08x expected %08x",
                            mode, i, axis, actual, expected);
                        throw std::runtime_error(message);
                    }
                }
            auto previous = state.particles()[1];
            state.pin({{1, {10, 20, 30}}});
            state.integrate(.1f);
            expect(state.particles()[1].position == std::array<float, 3>{10, 20, 30} &&
                       state.particles()[1].velocity == previous.velocity &&
                       state.particles()[1].acceleration == previous.acceleration,
                   "Pinned particle was integrated");
            state.reset({1, 2, 3});
            expect(state.particles()[1].pinned &&
                       state.particles()[1].velocity == std::array<float, 3>{} &&
                       state.particles()[1].force == std::array<float, 3>{} &&
                       state.particles()[1].position == std::array<float, 3>{1.5f, 3, 2.9f} &&
                       state.particles()[1].acceleration == previous.acceleration,
                   "Cloth reset state contract");
            rejects([&] { state.integrate(-.1f); });
            rejects([&] { state.integrate(std::numeric_limits<float>::infinity()); });
            rejects([&] { state.forces({}); });
            rejects([&] { state.pin({{12, {0, 0, 0}}}); });
            auto bad = setup;
            bad.mode = 2;
            rejects([&] { ClothParticles invalid(bad, mesh); });
            bad = setup;
            bad.rows = 0;
            rejects([&] { ClothParticles invalid(bad, mesh); });
            bad = setup;
            bad.rows = 3;
            rejects([&] { ClothParticles invalid(bad, mesh); });
        }
        for (unsigned mode = 0; mode < 2; ++mode)
            for (unsigned scenario = 0; scenario < 4; ++scenario) {
                ClothSetup setup;
                setup.rows = 2;
                setup.columns = 3;
                setup.mode = mode;
                ClothParticles state(setup, mesh);
                std::vector<std::array<float, 3>> values;
                volatile float quiet_x = .0001f, quiet_y = .00000001f;
                for (unsigned i = 0; i < 12; ++i) {
                    if (scenario == 3)
                        values.push_back({state.particles()[i].mass * quiet_x,
                                          state.particles()[i].mass * quiet_y, 0});
                    else
                        values.push_back({.75f + i * .125f, -3.5f, .2f * (i % 3)});
                }
                state.forces(values);
                state.pin({{0, mesh.vertices[0].position}, {3, mesh.vertices[3].position}});
                if (scenario == 3)
                    state.integrate(1.f);
                else {
                    state.integrate(.008f);
                    state.integrate(.016f);
                }
                state.external_forces({.75f, -.25f, .125f},
                                      scenario == 1 ? 2.5f : (scenario == 2 ? -.5f : 0));
                for (unsigned i = 0; i < 12; ++i)
                    for (unsigned axis = 0; axis < 3; ++axis) {
                        auto actual = bits(state.particles()[i].force[axis]);
                        auto expected = cloth_reference::environment[mode][scenario][i][axis];
                        if (actual != expected) {
                            char message[200];
                            std::snprintf(
                                message, sizeof(message),
                                "Original x86 force mismatch: mode %u scenario %u particle %u axis %u actual %08x expected %08x",
                                mode, scenario, i, axis, actual, expected);
                            throw std::runtime_error(message);
                        }
                    }
                auto before = state.particles()[1].force;
                rejects([&] {
                    state.external_forces({1, 2, std::numeric_limits<float>::infinity()}, 1);
                });
                expect(state.particles()[1].force == before,
                       "Invalid wind partially changed force state");
            }
        for (unsigned mode = 0; mode < 2; ++mode) {
            auto collapsed = mesh;
            for (auto& vertex : collapsed.vertices)
                vertex.position = {};
            ClothSetup setup;
            setup.columns = 3;
            setup.rows = 2;
            setup.mode = mode;
            ClothParticles zero_length(setup, collapsed);
            zero_length.external_forces({}, 0);
            expect(unsigned(zero_length.spring_forces()) == cloth_reference::collapsed[mode][0] &&
                       unsigned(std::isnan(zero_length.particles()[1].force[0])) ==
                           cloth_reference::collapsed[mode][1],
                   "Original zero-length spring IEEE/status semantics changed");
        }
        for (unsigned mode = 0; mode < 2; ++mode) {
            ClothSetup setup;
            setup.columns = 3;
            setup.rows = 2;
            setup.mode = mode;
            ClothParticles state(setup, mesh);
            std::vector<std::array<float, 3>> values;
            for (unsigned i = 0; i < 12; ++i)
                values.push_back({.75f + i * .125f, -3.5f, .2f * (i % 3)});
            state.forces(values);
            state.pin({{0, mesh.vertices[0].position}, {3, mesh.vertices[3].position}});
            state.integrate(.008f);
            state.integrate(.016f);
            state.pin({{1, state.particles()[1].position}});
            auto before = state.particles()[1];
            state.respond_contacts({{-1, {0, std::numeric_limits<float>::infinity(), 0}},
                                    {1, {0, 1, 0}},
                                    {2, {.5f, -1, .25f}},
                                    {4, {1, 0, 0}},
                                    {1, {0, 0, -1}},
                                    {5, {0, 0, 0}},
                                    {11, {-.6f, .8f, 0}},
                                    {0, {1, 1, 1}},
                                    {3, {0, 0, 1}}});
            for (unsigned i = 0; i < 12; ++i)
                for (unsigned axis = 0; axis < 3; ++axis) {
                    auto actual = bits(state.particles()[i].velocity[axis]);
                    auto expected = cloth_reference::contact_velocity[mode][i][axis];
                    if (actual != expected) {
                        char message[190];
                        std::snprintf(
                            message, sizeof(message),
                            "Original contact velocity mismatch: mode %u particle %u axis %u actual %08x expected %08x",
                            mode, i, axis, actual, expected);
                        throw std::runtime_error(message);
                    }
                }
            const auto& after = state.particles()[1];
            expect(after.pinned && after.velocity != before.velocity &&
                       after.position == before.position && after.force == before.force &&
                       after.acceleration == before.acceleration,
                   "Contact response changed non-velocity state or skipped a pinned contact");
            auto velocity = after.velocity;
            rejects([&] { state.respond_contacts({{1, {1, 0, 0}}, {12, {0, 1, 0}}}); });
            expect(state.particles()[1].velocity == velocity,
                   "Invalid contacts partially changed particles");
            rejects([&] { state.respond_contacts({{-2, {0, 1, 0}}}); });
        }
        const std::array<float, 3> directions[] = {
            {0, 0, 1}, {.75f, -.25f, .125f}, {0, 0, 0}, {-1.25f, .5f, -.75f}};
        const std::vector<ClothCollisionRecord> records = {
            {1, {.1f, 1, .2f}, .5f},
            {8, {-.25f, .75f, .125f}, .9f},
            {1, {.1f, 1, .2f}, .5f},
            {0, {1, 1, 1}, .4f},
            {-1, {0, std::numeric_limits<float>::infinity(), 0}, -1},
            {2, {0, 0, 0}, .3f}};
        for (unsigned variant = 0; variant < 2; ++variant)
            for (unsigned scenario = 0; scenario < 4; ++scenario) {
                ClothSetup setup;
                setup.columns = 3;
                setup.rows = 2;
                ClothParticles state(setup, mesh);
                state.pin({{0, mesh.vertices[0].position}});
                state.correct_penetrations(records, directions[scenario],
                                           variant == 0 ? ClothCorrection::scaled
                                                        : ClothCorrection::unscaled);
                for (unsigned i = 0; i < 12; ++i) {
                    const auto& particle = state.particles()[i];
                    for (unsigned axis = 0; axis < 3; ++axis) {
                        auto actual = bits(particle.position[axis]);
                        auto expected = cloth_reference::correction[variant][scenario][i][axis];
                        if (actual != expected) {
                            char message[190];
                            std::snprintf(
                                message, sizeof(message),
                                "Original penetration correction mismatch: variant %u scenario %u particle %u axis %u actual %08x expected %08x",
                                variant, scenario, i, axis, actual, expected);
                            throw std::runtime_error(message);
                        }
                    }
                    expect(particle.velocity == std::array<float, 3>{} &&
                               particle.acceleration == std::array<float, 3>{} &&
                               particle.force == std::array<float, 3>{},
                           "Penetration correction changed non-position state");
                }
                expect(state.particles()[0].pinned, "Penetration correction changed pin state");
                auto previous = state.particles()[1].position;
                rejects([&] {
                    state.correct_penetrations({{1, {0, 0, 0}, 1}, {12, {0, 0, 0}, 1}},
                                               directions[scenario], ClothCorrection::scaled);
                });
                expect(state.particles()[1].position == previous,
                       "Invalid penetration partially changed particles");
            }
        ClothSetup fallback_setup;
        fallback_setup.columns = 3;
        fallback_setup.rows = 2;
        ClothParticles shifted(fallback_setup, mesh);
        for (unsigned column = 0; column < 4; ++column) {
            auto position = mesh.vertices[column].position;
            position[0] += float(column);
            position[1] += 2.f;
            position[2] -= .5f;
            shifted.pin({{column, position}});
        }
        auto fallback = cloth_fallback_vertices(mesh, shifted, 3);
        auto committed = cloth_vertices(shifted);
        for (unsigned i = 0; i < 12; ++i) {
            expect(fallback[i].position[0] == mesh.vertices[i].position[0] + float(i % 4) &&
                       fallback[i].position[1] == mesh.vertices[i].position[1] + 2.f &&
                       fallback[i].position[2] == mesh.vertices[i].position[2] - .5f,
                   "Cloth fallback did not translate authored columns from current first row");
            expect(committed[i].position[0] == mesh.vertices[i].position[0],
                   "Cloth committed stream used uncommitted pin positions");
            expect(fallback[i].argb == 0xffffffff && fallback[i].uv[0] == mesh.vertices[i].uv[0] &&
                       fallback[i].uv[1] == mesh.vertices[i].uv[1],
                   "Cloth fallback attributes changed");
        }
        rejects([&] { cloth_fallback_vertices(mesh, shifted, 4); });
        std::puts(
            "Cloth particle mass/integration/environment/spring/contact/correction operations match original x86 golden bits in both modes");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
