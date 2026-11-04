#include <initializer_list>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cstring>
extern "C" void* recovered_005e5adf(std::uint32_t*);
extern "C" void* recovered_005e6d56(std::uint32_t*);
extern "C" void* recovered_005e6789(std::uint32_t*);
struct Mat {
    float m[4][4];
};
struct Key {
    int frame;
    float x, y, z;
};
struct Bone {
    Mat raw, inverse, local, final;
    int parent;
    unsigned rotations, translations;
    void* r;
    Key* t;
    unsigned active;
};
struct Clip {
    int start, end;
    Bone* bones;
    int count;
};
static_assert(sizeof(Bone) == 0x118);
extern "C" int __attribute__((thiscall)) FUN_00552910(Clip*, float);
extern "C" int __attribute__((thiscall)) FUN_00552cb0(Clip*, float, float, Clip*);
extern "C" const float dat_006bc9d4 = 2.f;
extern "C" const float dat_006be7fc = .2f;
extern "C" void* __attribute__((stdcall)) FUN_005e6783(void* out, const void* in) {
    std::uint32_t args[] = {reinterpret_cast<std::uint32_t>(out),
                            reinterpret_cast<std::uint32_t>(in)};
    return recovered_005e6789(args);
}
extern "C" Mat* __attribute__((stdcall)) FUN_005e4db8(Mat* out, const Mat* a, const Mat* b) {
    Mat result = {};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                result.m[i][j] += a->m[i][k] * b->m[k][j];
    *out = result;
    return out;
}
extern "C" void* __attribute__((stdcall)) FUN_005e5ad9(void* out, const void* in) {
    std::uint32_t args[] = {reinterpret_cast<std::uint32_t>(out),
                            reinterpret_cast<std::uint32_t>(in)};
    return recovered_005e5adf(args);
}
extern "C" void* __attribute__((stdcall)) FUN_005e6d50(void* out, const void* a, const void* b,
                                                       float t) {
    std::uint32_t bits;
    std::memcpy(&bits, &t, 4);
    std::uint32_t args[] = {reinterpret_cast<std::uint32_t>(out),
                            reinterpret_cast<std::uint32_t>(a), reinterpret_cast<std::uint32_t>(b),
                            bits};
    return recovered_005e6d56(args);
}
int main() {
    Bone bone = {};
    bone.parent = -1;
    bone.active = 1;
    bone.translations = 5;
    Key keys[] = {{0, 0, 0, 0}, {1, 1, 0, 0}, {2, 8, 0, 0}, {3, 27, 0, 0}, {4, 64, 0, 0}};
    bone.t = keys;
    for (int i = 0; i < 4; ++i)
        bone.local.m[i][i] = 1;
    Clip clip = {0, 4, &bone, 1};
    for (float frame : {-1.f, 0.f, 1.5f, 2.5f, 3.5f, 4.f, 5.f}) {
        FUN_00552910(&clip, frame);
        std::printf("%.2f %.9g\n", frame, bone.final.m[3][0]);
    }
    struct Rotation {
        int frame;
        float q[4];
    };
    Rotation rotations[] = {{0, {0, 0, 0, 1}}, {10, {0, 0, 1, 0}}};
    bone.rotations = 2;
    bone.r = rotations;
    bone.translations = 0;
    clip.end = 10;
    for (float frame : {0.f, 2.5f, 5.f, 10.f}) {
        FUN_00552910(&clip, frame);
        std::printf("rotation %.2f %.9g %.9g %.9g\n", frame, bone.final.m[0][0], bone.final.m[0][1],
                    bone.final.m[1][0]);
    }
    clip.end = 20;
    FUN_00552910(&clip, 5.f);
    std::printf("end20 %.9g %.9g\n", bone.final.m[0][0], bone.final.m[0][1]);
    Bone target = {};
    target.parent = -1;
    target.active = 1;
    for (int i = 0; i < 4; ++i)
        target.local.m[i][i] = 1;
    target.local.m[3][0] = 10;
    Key target_key = {0, 1000, 0, 0};
    target.translations = 1;
    target.t = &target_key;
    Clip destination = {0, 20, &target, 1};
    Key moving[] = {{0, 0, 0, 0}, {20, 4, 0, 0}};
    bone.rotations = 0;
    bone.translations = 2;
    bone.t = moving;
    for (float frame : {0.f, 10.f, 20.f})
        for (float phase : {0.f, 1.f, 2.5f, 4.f, 5.f}) {
            FUN_00552cb0(&clip, frame, phase, &destination);
            std::printf("blend %.2f %.2f %.9g\n", frame, phase, bone.final.m[3][0]);
        }
    bone.rotations = 2;
    bone.r = rotations;
    bone.translations = 0;
    clip.end = 10;
    target.local.m[0][0] = -1;
    target.local.m[1][1] = -1;
    for (float phase : {0.f, 1.f, 2.5f, 4.f, 5.f}) {
        FUN_00552cb0(&clip, 0.f, phase, &destination);
        std::printf("blend_rotation %.2f %.9g %.9g %.9g\n", phase, bone.final.m[0][0],
                    bone.final.m[0][1], bone.final.m[1][0]);
    }
    return 0;
}
