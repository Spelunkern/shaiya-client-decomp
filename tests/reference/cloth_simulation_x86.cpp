#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
struct Matrix {
    float m[4][4];
};
struct Bone {
    unsigned char prefix[0xc0];
    Matrix final;
    unsigned char tail[0x18];
};
struct Clip {
    int start, end;
    Bone* bones;
};
struct Particle {
    float mass, inverse_mass, position[3], velocity[3], acceleration[3], force[3];
    std::uint32_t pinned;
    float uv[2], initial[3];
};
struct Spring {
    std::uint32_t ends[4];
    float k, damping, rest;
};
struct Vertical {
    float direction[3], pad;
    double limit;
};
struct Contact {
    int row, column;
    float vector[3], radius;
    std::uint32_t unused[2];
};
static_assert(sizeof(Bone) == 0x118 && sizeof(Particle) == 80 && sizeof(Spring) == 28 &&
              sizeof(Vertical) == 24);
extern "C" void __attribute__((thiscall)) FUN_00580e30(void*, int, int, float, float, float, float,
                                                       float, float, float, float, float, int);
extern "C" void __attribute__((thiscall)) FUN_00580da0(void*);
extern "C" void __attribute__((thiscall)) FUN_00581770(void*, float);
extern "C" int __attribute__((thiscall)) FUN_00581190(void*);
extern "C" void __attribute__((thiscall)) FUN_00584ba0(void*, float, unsigned);
extern "C" const float dat_006bc4dc = 1, dat_006bc9d4 = 2, dat_006bca1c = 3, dat_006bc9b8 = 6,
                       dat_006cd28c = .025f, dat_006c2bb4 = 0x1.555556p-2f, dat_006bb5fc = 0,
                       dat_006bb600 = .01f, dat_006bed88 = -100, dat_006cd288 = .0001f,
                       dat_006bc860 = .5f, dat_006cd290 = -.35f, dat_006bb5f8 = -1,
                       dat_006bca08 = 1.5f, dat_006bc85c = .005f, dat_006bc8b4 = .1f,
                       dat_006cd294 = -.0001f, dat_006be7fc = .2f, dat_006bc8b0 = 20,
                       dat_006c4be4 = 200;
extern "C" const double dat_006cd298 = 5;
extern "C" {
float dat_0085d17c = .75f, dat_0085d180 = -.25f, dat_0085d184 = .125f, dat_0085d188 = .2f;
alignas(16) unsigned char dat_02209408[632] = {};
unsigned trace_substeps = 0, trace_attempts = 0, trace_resets = 0;
}
extern "C" Matrix* __attribute__((stdcall)) FUN_005e4db8(Matrix* out, const Matrix* a,
                                                         const Matrix* b) {
    Matrix m = {};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                m.m[i][j] += a->m[i][k] * b->m[k][j];
    *out = m;
    return out;
}
void put(unsigned char* p, unsigned offset, std::uint32_t value) {
    std::memcpy(p + offset, &value, 4);
}
void bits(float f) {
    std::uint32_t v;
    std::memcpy(&v, &f, 4);
    std::printf("0x%08xu,", v);
}
int main() {
    std::puts(
        "// Full original cloth solvers/frame policy, with transparent call counters.\n#pragma once\n#include <cstdint>\nnamespace cloth_simulation_reference {\nstruct Case {unsigned mode,scenario,phase;std::uint32_t elapsed;unsigned substeps,attempts,resets;std::uint32_t particles[12][16];};\ninline constexpr Case cases[]={");
    for (unsigned mode = 0; mode < 2; ++mode)
        for (unsigned scenario = 0; scenario < 11; ++scenario) {
            alignas(16) unsigned char owner[0xe4] = {};
            Particle original[3][4] = {}, current[3][4] = {};
            Particle *o[3] = {original[0], original[1], original[2]},
                     *c[3] = {current[0], current[1], current[2]};
            Vertical vertical[3][4] = {};
            Vertical* limits[3] = {vertical[0], vertical[1], vertical[2]};
            Spring springs[96] = {};
            Contact contacts[1000] = {};
            Bone bones[6] = {};
            Clip clip = {0, 0, bones};
            for (auto& bone : bones) {
                for (int i = 0; i < 4; ++i)
                    bone.final.m[i][i] = 1;
                bone.final.m[3][0] = scenario == 8 ? .5f : 1000.f;
                bone.final.m[3][1] = scenario == 8 ? 1.4f : 1000.f;
            }
            Matrix world = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
            std::memcpy(dat_02209408 + 508, &world, 64);
            float floor = -100;
            std::memcpy(dat_02209408 + 572, &floor, 4);
            float direction[3] = {scenario == 8 ? 2.5f : 0.f, scenario == 8 ? .5f : 0.f, -1};
            std::memcpy(dat_02209408 + 620, direction, 12);
            put(owner, 0, mode);
            put(owner, 4, 13);
            put(owner, 8, 2);
            put(owner, 12, 3);
            put(owner, 0x64, reinterpret_cast<std::uint32_t>(o));
            put(owner, 0x68, reinterpret_cast<std::uint32_t>(c));
            put(owner, 0x6c, reinterpret_cast<std::uint32_t>(limits));
            put(owner, 0x78, reinterpret_cast<std::uint32_t>(springs));
            put(owner, 0x7c, reinterpret_cast<std::uint32_t>(contacts));
            put(owner, 0x4c, reinterpret_cast<std::uint32_t>(&clip));
            put(owner, 0x3c, mode ? 500 : 600);
            put(owner, 0x44, mode ? 1 : 2);
            float reset[3] = {.1f, -.2f, .3f};
            std::memcpy(owner + 0x80, reset, 12);
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 4; ++column) {
                    FUN_00580e30(owner, row, column, .5f, .25f * (column + 1), 1.f + .5f * row,
                                 .1f * (row - column), 0, 0, 0, .25f * column, .5f * row, 10000);
                    current[row][column] = original[row][column];
                    current[row][column].pinned = row == 0;
                    if (row) {
                        const auto& a = original[row - 1][column].position;
                        const auto& b = original[row][column].position;
                        auto x = static_cast<long double>(a[0]) - b[0],
                             y = static_cast<long double>(a[1]) - b[1],
                             z = static_cast<long double>(a[2]) - b[2];
                        vertical[row][column].limit =
                            double(std::sqrt((x * x + y * y) + z * z) * (mode ? 10.L : 1.5L));
                    }
                }
            if (scenario == 7) {
                current[1][0].position[1] = 100;
                current[1][0].pinned = 1;
            }
            if (scenario == 9 || scenario == 10) {
                current[1][0].force[1] = current[1][0].mass * (scenario == 9 ? 4.f : 30.f);
                FUN_00581770(owner, 1.f);
            }
            FUN_00580da0(owner);
            FUN_00581190(owner);
            unsigned phase = scenario == 2 ? 1 : (scenario == 3 ? 2 : (scenario == 4 ? 3 : 0));
            const float elapsed =
                (scenario == 0 || scenario == 9)
                    ? 0.f
                    : (scenario == 5 ? .005f : (scenario == 6 ? .005000001f : .016f));
            trace_substeps = trace_attempts = trace_resets = 0;
            FUN_00584ba0(owner, elapsed, phase);
            std::printf("{%u,%u,%u,", mode, scenario, phase);
            bits(elapsed);
            std::printf("%u,%u,%u,{", trace_substeps, trace_attempts, trace_resets);
            for (unsigned i = 0; i < 12; ++i) {
                const auto& p = current[i / 4][i % 4];
                const auto& committed = original[i / 4][i % 4];
                std::printf("{");
                for (float v : p.position)
                    bits(v);
                for (float v : p.velocity)
                    bits(v);
                for (float v : p.acceleration)
                    bits(v);
                for (float v : p.force)
                    bits(v);
                for (float v : committed.position)
                    bits(v);
                std::printf("%u,},", p.pinned);
            }
            std::puts("}},");
        }
    std::puts("};\n}");
}
