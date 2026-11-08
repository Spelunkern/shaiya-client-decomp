#include "graphics/render_rsqrt.h"
extern "C" {
#include "graphics/render_rsqrt_coefficients.h"
}

namespace {
uint32_t bits_of(float value) {
    uint32_t bits;
    __builtin_memcpy(&bits, &value, 4);
    return bits;
}
float float_of(uint32_t bits) {
    float value;
    __builtin_memcpy(&value, &bits, 4);
    return value;
}
// The near-unit path copies the components as integers: an x87 load and
// store would quiet signalling NaNs and raise exception flags.
void copy_word(float* to, const float* from) {
    const uint32_t word = *reinterpret_cast<const volatile uint32_t*>(from);
    *reinterpret_cast<volatile uint32_t*>(to) = word;
}
} // namespace

// 0x0064f3e9 (D3DXVec3Normalize): out = in / |in|, using the table-driven
// reciprocal square root of dat_00715908.
//
// The arithmetic runs on the x87 stack in its current precision: products,
// sums and the reciprocal stay in registers (long double) and are rounded to
// float only where the original stores them. A zero length writes +0.0f; a
// length within 1e-5 of one copies the input unchanged.
extern "C" float* __attribute__((stdcall)) FUN_0064f3e9(float* out, const float* in) {
    using x87 = long double;
    const x87 length_squared =
        x87(in[0]) * in[0] + x87(in[1]) * in[1] + x87(in[2]) * in[2];
    const uint32_t bits = bits_of(float(length_squared));
    if (bits == 0) {
        out[0] = float_of(0);
        out[1] = float_of(0);
        out[2] = float_of(0);
        // The original discards the length with FFREE, which leaves TOP one
        // slot lower than a pop would; rotate it the same way.
        __asm__ volatile("fdecstp" ::: "memory");
        return out;
    }
    if (int32_t(bits_of(float(length_squared - 1)) & 0x7fffffffu) <= 0x3727c5ac) {
        if (out != in) {
            copy_word(out, in);
            copy_word(out + 1, in + 1);
            copy_word(out + 2, in + 2);
        }
        return out;
    }
    const RsqrtCoefficients& coefficient = dat_00715908[(bits >> 15) & 0x1ff];
    const float mantissa = float_of((bits & 0x00ffffffu) | 0x3f000000u);
    const float exponent = float_of(((0xbeffffffu - bits) >> 1) & 0xff800000u);
    const x87 reciprocal = (x87(mantissa) * coefficient.multiplier + coefficient.addend) * exponent;
    // All three inputs are read before the first store, so out may alias in.
    const float x = float(in[0] * reciprocal);
    const float y = float(in[1] * reciprocal);
    const float z = float(in[2] * reciprocal);
    out[0] = x;
    out[1] = y;
    out[2] = z;
    return out;
}
