#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
extern "C" float FUN_005b3640(const float*, const float*, const float*, float, float);
extern "C" float __attribute__((thiscall)) FUN_005b3360(void*, std::int32_t, std::int32_t);
extern "C" float __attribute__((thiscall)) FUN_005b3720(void*, float, float);
extern "C" float __attribute__((thiscall)) FUN_005a2c30(void*, float, float);
extern "C" const float dat_0224e640 = .5f, dat_0224e648 = 2.f;
extern "C" const float dat_006bb5fc = 0, dat_006bfe14 = .25f, dat_006bc9cc = 4, dat_006bc9d4 = 2,
                       dat_0224e644 = 7.25f, dat_0224e63c = 2;
// The original _ftol helper truncates/pops ST0 and returns the integer in EAX.
asm(".intel_syntax noprefix\n.text\n.globl _FUN_005cd414\n_FUN_005cd414:\n"
    "sub esp, 12\nfnstcw word ptr [esp+8]\nmov ax, word ptr [esp+8]\n"
    "or ax, 0x0c00\nmov word ptr [esp+10], ax\nfldcw word ptr [esp+10]\n"
    "fistp qword ptr [esp]\nfldcw word ptr [esp+8]\nmov eax, dword ptr [esp]\n"
    "mov edx, dword ptr [esp+4]\nadd esp, 12\nret\n.att_syntax prefix\n");
extern "C" {
std::int32_t dat_0224e650 = 4;
}
extern "C" const std::uint32_t dat_006bc4dc = 0x3f800000;
extern "C" const std::uint32_t dat_006bb5f8 = 0xbf800000;
std::uint32_t bits(float f) {
    std::uint32_t n;
    std::memcpy(&n, &f, 4);
    return n;
}
int main() {
    std::puts(
        "#pragma once\n#include <cstdint>\nnamespace terrain_reference {\ninline constexpr std::uint32_t cases[][12] = {");
    for (int i = 0; i < 32; ++i) {
        float a[3] = {float(i - 12) * .125f, float(i % 7 - 3) * .3f, float(i - 9) * .25f};
        float b[3] = {a[0] + 4.f, float(i % 5 - 2) * 1.1f, a[2]};
        float c[3] = {a[0] + (i % 2 ? 2.f : 0.f), float(i % 11 - 5) * .7f, a[2] + 4.f};
        float x = a[0] + float(i % 9) * .5f, z = a[2] + float(i % 7) * .5f;
        volatile float result = FUN_005b3640(a, b, c, x, z);
        std::printf("{");
        for (auto p : {a, b, c})
            for (int j = 0; j < 3; ++j)
                std::printf("0x%08xu,", bits(p[j]));
        std::printf("0x%08xu,0x%08xu,0x%08xu},\n", bits(x), bits(z), bits(result));
    }
    std::puts("};\ninline constexpr std::int32_t sample_cases[][3] = {");
    std::uint16_t samples[21];
    for (unsigned i = 0; i < 21; ++i)
        samples[i] = std::uint16_t(i * 3201);
    struct {
        const std::uint16_t* data;
    } owner = {samples};
    for (auto x : {INT32_MIN, -1, 0, 1, 3, 4, 5, INT32_MAX})
        for (auto z : {INT32_MIN, -1, 0, 1, 3, 4, 5, INT32_MAX}) {
            volatile float value = FUN_005b3360(&owner, x, z);
            std::printf("{%d,%d,%d},\n", x, z, int(value));
        }
    std::puts("};\ninline constexpr std::uint32_t height_cases[][3] = {");
    std::uint16_t varied[73];
    for (unsigned i = 0; i < 73; ++i)
        varied[i] = std::uint16_t(i * i * 3201 + i * 913);
    owner.data = varied;
    dat_0224e650 = 8;
    for (int ix = -1; ix <= 32; ++ix)
        for (int iz = -1; iz <= 32; ++iz) {
            float x = float(ix) * .25f, z = float(iz) * .25f;
            volatile float value = FUN_005b3720(&owner, x, z);
            std::printf("{0x%08xu,0x%08xu,0x%08xu},\n", bits(x), bits(z), bits(value));
        }
    std::puts("};\ninline constexpr std::uint32_t world_cases[][3] = {");
    static unsigned char world_owner[0x13cc28 + sizeof(owner)] = {};
    std::memcpy(world_owner + 0x13cc28, &owner, sizeof(owner));
    for (int ix = -2; ix <= 34; ++ix)
        for (int iz = -2; iz <= 34; ++iz) {
            float x = float(ix) * .5f, z = float(iz) * .5f;
            volatile float value = FUN_005a2c30(world_owner, x, z);
            std::printf("{0x%08xu,0x%08xu,0x%08xu},\n", bits(x), bits(z), bits(value));
        }
    std::puts("};\n}");
}
