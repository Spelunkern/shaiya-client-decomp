#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
struct Matrix {
    float m[4][4];
};
struct Bone {
    std::uint8_t prefix[0xc0];
    Matrix final;
    std::uint8_t tail[0x118 - 0x100];
};
static_assert(sizeof(Bone) == 0x118);
struct Particle {
    float mass, inverse_mass, position[3], velocity[3], acceleration[3], force[3];
    std::uint32_t pinned;
    float uv[2], initial[3];
};
struct Contact {
    int row, column;
    float vector[3], radius;
    std::uint32_t unused[2];
};
static_assert(sizeof(Particle) == 80 && sizeof(Contact) == 32);
struct Clip {
    int start, end;
    Bone* bones;
};
extern "C" int __attribute__((thiscall)) FUN_00583370(void*);
extern "C" const float dat_006bb5fc = 0, dat_006bb600 = .01f, dat_006bc4dc = 1,
                       dat_006bc85c = .005f, dat_006bc8b4 = .1f, dat_006cd294 = -.0001f;
extern "C" {
alignas(16) unsigned char dat_02209408[576] = {};
}
extern "C" Matrix* __attribute__((stdcall)) FUN_005e4db8(Matrix* output, const Matrix* a,
                                                         const Matrix* b) {
    Matrix value = {};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                value.m[i][j] += a->m[i][k] * b->m[k][j];
    *output = value;
    return output;
}
void put(unsigned char* owner, std::size_t offset, std::uint32_t value) {
    std::memcpy(owner + offset, &value, 4);
}
void bits(float f) {
    std::uint32_t n;
    std::memcpy(&n, &f, 4);
    std::printf("0x%08xu,", n);
}
int main() {
    Bone bones[6] = {};
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 4; ++j)
            bones[i].final.m[j][j] = 1;
        bones[i].final.m[3][0] = i + .125f;
        bones[i].final.m[3][1] = 2 * i + .25f;
        bones[i].final.m[3][2] = -i + .5f;
    }
    Clip clip = {0, 0, bones};
    Matrix world = {{{2, 0, 0, 0}, {0, 3, 0, 0}, {0, 0, 4, 0}, {10, 20, 30, 1}}};
    std::memcpy(dat_02209408 + 508, &world, 64);
    std::puts(
        "// Original 00583370 shape preparation, without particle traversal.\n#pragma once\n#include <cstdint>\nnamespace cloth_body_reference {\ninline constexpr std::uint32_t shapes[16][4][4]={");
    for (unsigned kind = 0; kind < 16; ++kind) {
        alignas(16) unsigned char owner[0xe4] = {};
        put(owner, 4, kind);
        put(owner, 8, 0xffffffff);
        put(owner, 12, 0xffffffff);
        put(owner, 0x4c, reinterpret_cast<std::uint32_t>(&clip));
        for (int sphere = 0; sphere < 4; ++sphere) {
            float center[3] = {1000, 2000, 3000}, radius = -1;
            std::memcpy(owner + 0xa4 + sphere * 12, center, 12);
            std::memcpy(owner + 0xd4 + sphere * 4, &radius, 4);
        }
        FUN_00583370(owner);
        std::puts("{");
        for (int sphere = 0; sphere < 4; ++sphere) {
            std::printf("{");
            for (int axis = 0; axis < 3; ++axis) {
                float v;
                std::memcpy(&v, owner + 0xa4 + sphere * 12 + axis * 4, 4);
                bits(v);
            }
            float radius;
            std::memcpy(&radius, owner + 0xd4 + sphere * 4, 4);
            bits(radius);
            std::puts("},");
        }
        std::puts("},");
    }
    std::puts("};");
    std::puts(
        "struct Detection { unsigned kind,phase,collapsed_world,pinned; std::uint32_t floor; std::uint32_t input[4][3]; int status; std::uint32_t position[4][3]; unsigned count; std::uint32_t records[16][5]; };\ninline constexpr Detection detection[]={");
    for (unsigned kind_index = 0; kind_index < 3; ++kind_index)
        for (unsigned scenario = 0; scenario < 14; ++scenario) {
            unsigned kind = kind_index == 0 ? 0 : (kind_index == 1 ? 9 : 13);
            alignas(16) unsigned char owner[0xe4] = {};
            for (int bone = 0; bone < 6; ++bone) {
                bones[bone].final.m[3][0] = float(2 * bone - 2);
                bones[bone].final.m[3][1] = bones[bone].final.m[3][2] = 0;
            }
            Matrix world = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
            if (scenario == 12)
                world.m[0][0] = world.m[1][1] = world.m[2][2] = 0;
            std::memcpy(dat_02209408 + 508, &world, 64);
            put(owner, 4, kind);
            put(owner, 8, 0xffffffff);
            put(owner, 12, 0xffffffff);
            put(owner, 0x4c, reinterpret_cast<std::uint32_t>(&clip));
            FUN_00583370(owner); // Prepare the shapes to construct exact float inputs.
            float centers[4][3], radii[4];
            std::memcpy(centers, owner + 0xa4, 48);
            std::memcpy(radii, owner + 0xd4, 16);
            Particle points[2][2] = {};
            Particle* rows[2] = {points[0], points[1]};
            Contact contacts[64] = {};
            unsigned phase = scenario == 9 ? 1 : (scenario == 10 ? 2 : (scenario == 11 ? 3 : 0));
            unsigned pinned = scenario == 6 ? 2 : (scenario == 7 ? 5 : 0);
            float floor = scenario == 6 ? 0.f : -10.f;
            std::memcpy(dat_02209408 + 572, &floor, 4);
            for (unsigned i = 0; i < 4; ++i) {
                auto& point = points[i / 2][i % 2];
                point.pinned = (pinned >> i) & 1;
                point.position[0] = -20.f + float(i);
                point.position[1] = 1.f;
                unsigned sphere = i;
                if (scenario == 1 || scenario == 13)
                    sphere = 0;
                if (scenario == 3)
                    sphere = i == 0 ? 0 : (i == 1 ? 2 : (i == 2 ? 3 : 1));
                if (scenario == 4)
                    sphere = 3;
                if (scenario != 0 && scenario != 6) {
                    std::memcpy(point.position, centers[sphere], 12);
                    volatile float r = radii[sphere];
                    if (scenario == 1) {
                        const float offsets[] = {.005f, .01f, .010001f, -.0001f};
                        point.position[0] += r + offsets[i];
                    } else if (scenario == 5) {
                    } // Exact-center case.
                    else if (scenario == 8) {
                        float effective = r > 0 ? float(r) : .005f;
                        point.position[0] += .6f * effective;
                        point.position[1] += .8f * effective;
                    } else if (scenario == 12)
                        point.position[0] += .15f;
                    else if (scenario == 13)
                        point.position[0] += r - .0001f;
                    else if (scenario == 3 && (i == 0 || i == 2))
                        point.position[0] += r + .005f;
                    else
                        point.position[0] += r > 0 ? r * .5f : .00001f;
                }
                if (scenario == 6)
                    point.position[1] = -1;
            }
            std::printf("{%u,%u,%u,%u,", kind, phase, unsigned(scenario == 12), pinned);
            bits(floor);
            std::printf("{");
            for (const auto& row : points)
                for (const auto& point : row) {
                    std::printf("{");
                    for (float f : point.position)
                        bits(f);
                    std::printf("},");
                }
            std::printf("},");
            put(owner, 8, 1);
            put(owner, 12, 1);
            put(owner, 0x48, phase);
            put(owner, 0x2c, 64);
            put(owner, 0x68, reinterpret_cast<std::uint32_t>(rows));
            put(owner, 0x7c, reinterpret_cast<std::uint32_t>(contacts));
            int status = FUN_00583370(owner);
            std::printf("%d,{", status);
            for (const auto& row : points)
                for (const auto& point : row) {
                    std::printf("{");
                    for (float f : point.position)
                        bits(f);
                    std::printf("},");
                }
            unsigned count = 0;
            for (unsigned i = 0; i < 64; ++i)
                if (contacts[i].row != -1)
                    count = i + 1;
            if (count > 16)
                return 2;
            std::printf("},%u,{", count);
            for (unsigned i = 0; i < count; ++i) {
                const auto& c = contacts[i];
                std::printf("{%u,", unsigned(c.row * 2 + c.column));
                for (float f : c.vector)
                    bits(f);
                bits(c.radius);
                std::printf("},");
            }
            std::puts("}},");
        }
    std::puts("};\n}");
}
