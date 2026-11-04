#include "cloth_particles.h"
#include "cloth_geometry.h"
#include "fixtures/cloth_simulation_x86.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
using namespace genesis;
using Matrix = shaiya::graphics::native::Matrix;
std::uint32_t bits(float f) {
    std::uint32_t n;
    std::memcpy(&n, &f, 4);
    return n;
}
float from_bits(std::uint32_t n) {
    float f;
    std::memcpy(&f, &n, 4);
    return f;
}
int main() {
    try {
        Matrix identity = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
        std::vector<Matrix> skeleton(6, identity);
        auto calm = cloth_scene(0, skeleton, identity, -12.5f, 0);
        const std::uint32_t samples[] = {0, 1, 16384, 32767};
        const std::uint32_t strengths[] = {0, 0x364cce67, 0x3d4cce67, 0x3dcccccd};
        for (unsigned i = 0; i < 4; ++i)
            if (bits(cloth_scene(0, skeleton, identity, 0, samples[i]).wind_strength) !=
                strengths[i])
                throw std::runtime_error("Actor cloth wind scaling/store mismatch");
        if (calm.floor != -12.5f || calm.wind_strength != 0 ||
            calm.wind_direction != std::array<float, 3>{0, 0, -1} ||
            calm.correction_direction != calm.wind_direction)
            throw std::runtime_error("Actor cloth scene lost ground/direction/random inputs");
        bool rejected = false;
        try {
            cloth_scene(0, skeleton, identity, 0, 32768);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        if (!rejected)
            throw std::runtime_error("Actor cloth scene accepted non-15-bit random sample");
        unsigned projected = 0;
        for (const auto& reference : cloth_simulation_reference::cases) {
            ClothMesh mesh;
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 4; ++col) {
                    ClothVertex v;
                    v.position = {.25f * (col + 1), 1.f + .5f * row, .1f * (row - col)};
                    v.uv = {.25f * col, .5f * row};
                    mesh.vertices.push_back(v);
                }
            ClothSetup setup;
            setup.rows = 2;
            setup.columns = 3;
            setup.mode = reference.mode;
            ClothParticles state(setup, mesh);
            for (unsigned i = 0; i < 4; ++i)
                state.pin({{i, mesh.vertices[i].position}});
            if (reference.scenario == 7)
                state.pin({{4, {mesh.vertices[4].position[0], 100, mesh.vertices[4].position[2]}}});
            if (reference.scenario == 9 || reference.scenario == 10) {
                std::vector<std::array<float, 3>> forces(12);
                forces[4][1] = state.particles()[4].mass * (reference.scenario == 9 ? 4.f : 30.f);
                state.forces(forces);
                state.integrate(1.f);
            }
            std::vector<Matrix> finals(6);
            for (auto& m : finals) {
                for (unsigned i = 0; i < 4; ++i)
                    m.m[i][i] = 1;
                m.m[3][0] = reference.scenario == 8 ? .5f : 1000.f;
                m.m[3][1] = reference.scenario == 8 ? 1.4f : 1000.f;
            }
            Matrix world = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
            ClothScene scene;
            scene.body = cloth_body(13, finals, world);
            scene.wind_direction = {.75f, -.25f, .125f};
            scene.wind_strength = .2f;
            scene.correction_direction = {reference.scenario == 8 ? 2.5f : 0,
                                          reference.scenario == 8 ? .5f : 0, -1};
            auto stats = state.advance(from_bits(reference.elapsed), reference.phase,
                                       {.1f, -.2f, .3f}, scene);
            if (stats.substeps != reference.substeps || stats.attempts != reference.attempts ||
                stats.resets != reference.resets) {
                char message[220];
                std::snprintf(
                    message, sizeof(message),
                    "Original cloth tick mode %u scenario %u: counts %u/%u/%u expected %u/%u/%u",
                    reference.mode, reference.scenario, stats.substeps, stats.attempts,
                    stats.resets, reference.substeps, reference.attempts, reference.resets);
                throw std::runtime_error(message);
            }
            auto vertices = cloth_vertices(state);
            for (unsigned i = 0; i < 12; ++i) {
                const auto& p = state.particles()[i];
                const auto& committed = state.committed_positions()[i];
                float values[] = {p.position[0],     p.position[1],     p.position[2],
                                  p.velocity[0],     p.velocity[1],     p.velocity[2],
                                  p.acceleration[0], p.acceleration[1], p.acceleration[2],
                                  p.force[0],        p.force[1],        p.force[2],
                                  committed[0],      committed[1],      committed[2]};
                for (unsigned field = 0; field < 15; ++field)
                    if (bits(values[field]) != reference.particles[i][field]) {
                        char message[220];
                        std::snprintf(
                            message, sizeof(message),
                            "Original cloth tick mode %u scenario %u particle %u field %u: actual %08x expected %08x",
                            reference.mode, reference.scenario, i, field, bits(values[field]),
                            reference.particles[i][field]);
                        throw std::runtime_error(message);
                    }
                for (unsigned c = 0; c < 3; ++c)
                    if (bits(vertices[i].position[c]) != reference.particles[i][12 + c])
                        throw std::runtime_error(
                            "Cloth GPU stream used working positions instead of committed output");
                if (vertices[i].argb != 0xffffffff || vertices[i].uv[0] != mesh.vertices[i].uv[0] ||
                    vertices[i].uv[1] != mesh.vertices[i].uv[1])
                    throw std::runtime_error("Cloth GPU stream lost original color/UVs");
                if (p.position != committed)
                    ++projected;
                if (unsigned(p.pinned) != reference.particles[i][15])
                    throw std::runtime_error("Original cloth tick pin flags changed");
            }
        }
        if (!projected)
            throw std::runtime_error("Original post-commit row-constraint branch was not covered");
        std::puts("Native cloth frame policy and both full solvers match original x86 cases");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
