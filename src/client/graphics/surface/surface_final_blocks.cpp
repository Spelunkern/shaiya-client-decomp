#include "graphics/surface/surface_final_blocks.h"
#include "graphics/surface/surface_color_match.h"
#include "graphics/surface/surface_format_configure.h"
#include "graphics/surface/surface_format_tables.h"
#include "core/untouched_lifetime.h"
#include "graphics/surface/surface_lifetimes.h"
#include "graphics/surface/surface_read_transform.h"
#include "graphics/surface/surface_write_color.h"
#include "graphics/surface/surface_write_transform.h"
#include "runtime/allocations.h"
// Instruction-level source recovery. No original image or code bytes are build inputs.
#pragma STDC FENV_ACCESS ON
namespace {
using u32 = uint32_t;
u32 word(u32 p) {
    u32 v;
    __asm__ volatile("movl (%1),%0" : "=r"(v) : "r"(p) : "memory");
    return v;
}
void put(u32 p, u32 v) {
    __asm__ volatile("movl %1,(%0)" ::"r"(p), "r"(v) : "memory");
}
void half(u32 p, u32 v) {
    __asm__ volatile("movw %w1,(%0)" ::"r"(p), "r"(v) : "memory");
}
long double sample(u32 p) {
    long double v;
    __asm__ volatile("flds (%1)" : "=t"(v) : "r"(p) : "memory");
    return v;
}
#define SURFACE_CONSTANT(name)                                                                     \
    long double name() {                                                                           \
        long double v;                                                                             \
        __asm__ volatile("flds _" #name : "=t"(v));                                                \
        return v;                                                                                  \
    }
SURFACE_CONSTANT(dat_006bb60c)
SURFACE_CONSTANT(dat_006bc860)
SURFACE_CONSTANT(dat_006be1d0)
SURFACE_CONSTANT(dat_006c1bb8)
SURFACE_CONSTANT(dat_006c1bbc)
SURFACE_CONSTANT(dat_006bc8c4)
SURFACE_CONSTANT(dat_006df3c0)
SURFACE_CONSTANT(dat_006df3c4)
SURFACE_CONSTANT(dat_006df3c8)
SURFACE_CONSTANT(dat_006df3cc)
SURFACE_CONSTANT(dat_006df3d0)
SURFACE_CONSTANT(dat_006df3d4)
SURFACE_CONSTANT(dat_006df3d8)
SURFACE_CONSTANT(dat_006df3dc)
SURFACE_CONSTANT(dat_006df3e0)
SURFACE_CONSTANT(dat_006df3e4)
SURFACE_CONSTANT(dat_006df3e8)
SURFACE_CONSTANT(dat_006df3ec)
SURFACE_CONSTANT(dat_006df3f0)
#undef SURFACE_CONSTANT
// FSTP to a float slot, FLD it back, FISTP under the caller's control word:
// out-of-range values give 0x80000000 and raise IE like the original.
u32 rounded(long double v) {
    const float slot = static_cast<float>(v);
    u32 r;
    __asm__ volatile("flds %1\n\tfistpl %0" : "=m"(r) : "m"(slot) : "st");
    return r;
}
u32 byte_clamp(u32 v) {
    if (static_cast<int32_t>(v) < 0)
        return 0;
    return static_cast<int32_t>(v) > 0xff ? 0xffu : v;
}
} // namespace
// Complete original 005f12b9..005f15bc: flush of the pending 4:2:2 row
// (YUY2/UYVY and RGBG/GRGB). `frame` is the original EBP-0x28 frame built by
// the entry below. FNSTCW writes only the low word of [EBP-0xC]; the dword
// saved into +0x7c keeps the two stale bytes above it, as in the original.
extern "C" u32 recovered_005f12b9(u32 self, u32 frame) {
    if (word(self + 0xa0) == 0 || word(self + 0xa4) == 0)
        return 0;
    u32 destination = word(self + 0x94) * word(self + 0x64) + word(self + 0x88) * word(self + 0x60);
    destination += word(self + 0x84) * 2u + word(self + 0x20);
    u32 source = word(self + 0x80);
    const u32 saved = frame + 0x1c, truncating = frame + 0x20;
    __asm__ volatile("fnstcw (%0)" ::"r"(saved) : "memory");
    put(truncating, word(saved) | 0x0c00u);
    __asm__ volatile("fldcw (%0)" ::"r"(truncating) : "memory");
    put(self + 0x7c, word(saved));
    const u32 format = word(self + 4);
    if (format == 0x32595559u || format == 0x59565955u) { // YUY2, UYVY
        for (u32 x = word(self + 0x84); x < word(self + 0x8c); x += 2u) {
            const long double y1 = static_cast<float>(sample(source + 0x10) * dat_006df3dc() +
                                                      sample(source + 0x18) * dat_006df3d8() +
                                                      sample(source + 0x14) * dat_006df3d4());
            const long double v = static_cast<float>(sample(source + 8) * dat_006df3d0() -
                                                     sample(source + 4) * dat_006df3cc() -
                                                     sample(source) * dat_006df3c8());
            const long double u = static_cast<float>(
                sample(source) * dat_006df3d0() -
                (sample(source + 4) * dat_006df3c4() + sample(source + 8) * dat_006df3c0()));
            u32 first = rounded(sample(source) * dat_006df3dc() +
                                sample(source + 4) * dat_006df3d4() +
                                sample(source + 8) * dat_006df3d8() + dat_006bc860()) +
                        0x10u;
            u32 second = rounded(y1 + dat_006bc860()) + 0x10u;
            u32 chroma0 = rounded(v + dat_006bc860()) + 0x80u;
            u32 chroma1 = rounded(u + dat_006bc860()) + 0x80u;
            first = byte_clamp(first);
            second = byte_clamp(second);
            chroma0 = byte_clamp(chroma0);
            chroma1 = byte_clamp(chroma1);
            const u32 high = word(self + 0xa8) & 31u, low = word(self + 0xac) & 31u;
            half(destination, first << high | chroma0 << low);
            half(destination + 2, second << high | chroma1 << low);
            source += 0x20;
            destination += 4;
        }
    } else if (format == 0x42475247u || format == 0x47424752u) { // GRGB, RGBG
        for (u32 x = word(self + 0x84); x < word(self + 0x8c); x += 2u) {
            const long double scale = dat_006bc8c4(), bias = dat_006bc860();
            const u32 a = rounded(sample(source + 4) * scale + bias);
            const u32 b = rounded(sample(source) * scale + bias);
            half(destination, a << (word(self + 0xa8) & 31u) | b << (word(self + 0xac) & 31u));
            const u32 c = rounded(sample(source + 0x14) * scale + bias);
            const u32 d = rounded(sample(source + 8) * scale + bias);
            half(destination + 2, c << (word(self + 0xa8) & 31u) | d << (word(self + 0xac) & 31u));
            source += 0x20;
            destination += 4;
        }
    }
    put(frame, word(self + 0x7c));
    __asm__ volatile("fldcw (%0)" ::"r"(frame) : "memory");
    put(self + 0xa0, 0);
    return 0;
}
extern "C" __attribute__((naked, thiscall)) u32 FUN_005f12b9(void*) {
    __asm__ volatile("pushl %ebp\n\tmovl %esp,%ebp\n\tsubl $0x28,%esp\n\tpushl %esp\n\tpushl %ecx\n\t"
                     "calll _recovered_005f12b9\n\tleave\n\tretl");
}
namespace {
u32 word16(u32 p) {
    u32 v;
    __asm__ volatile("movzwl (%1),%0" : "=r"(v) : "r"(p) : "memory");
    return v;
}
// SHR AX,CL and SHR EAX,CL as the original spells them; CL is the low byte
// of the 16-bit shift field.
u32 shr16(u32 v, u32 count) {
    __asm__("shrw %%cl,%w0" : "+r"(v) : "c"(count) : "cc");
    return v;
}
u32 shr32(u32 v, u32 count) {
    __asm__("shrl %%cl,%0" : "+r"(v) : "c"(count) : "cc");
    return v;
}
long double integer(u32 v) {
    long double r = static_cast<int32_t>(v);
    if (static_cast<int32_t>(v) < 0) // FILD of the unsigned sum; never taken after SHR 4
        r += dat_006bb60c();
    return r;
}
void store_float(u32 p, long double v) {
    const float f = static_cast<float>(v);
    __asm__ volatile("flds %1\n\tfstps (%0)" ::"r"(p), "m"(f) : "st", "memory");
}
void copy_float(u32 p, u32 from) {
    __asm__ volatile("flds (%1)\n\tfstps (%0)" ::"r"(p), "r"(from) : "st", "memory");
}
// FLD m32 + FCOMP against 0.0 or 1.0: FCOM, so a NaN raises IE.
u32 compare_float(u32 from, bool one) {
    u32 sw;
    if (one)
        __asm__ volatile("flds (%1)\n\tfcomps _dat_006bc4dc\n\tfnstsw %%ax" : "=a"(sw) : "r"(from) : "st", "memory");
    else
        __asm__ volatile("flds (%1)\n\tfcomps _dat_006bb5fc\n\tfnstsw %%ax" : "=a"(sw) : "r"(from) : "st", "memory");
    return sw;
}
u32 compare_value(long double v, bool one) {
    u32 sw;
    if (one)
        __asm__ volatile("fcoms _dat_006bc4dc\n\tfnstsw %%ax" : "=a"(sw) : "t"(v) : "memory");
    else
        __asm__ volatile("fcoms _dat_006bb5fc\n\tfnstsw %%ax" : "=a"(sw) : "t"(v) : "memory");
    return sw;
}
// TEST AH,5 / JP falls through only when exactly one of C0 and C2 is set:
// below. TEST AH,41h / JNE falls through only when above.
bool below(u32 sw) { return __builtin_parity((sw >> 8) & 5u) != 0; }
bool above(u32 sw) { return ((sw >> 8) & 0x41u) == 0; }
// Clamp the float stored at `from` into `to`, as the 005f1bb0 sequence does.
void clamp_stored(u32 to, u32 from) {
    if (below(compare_float(from, false)))
        store_float(to, 0.0L);
    else if (above(compare_float(from, true)))
        store_float(to, 1.0L);
    else
        copy_float(to, from);
}
} // namespace
// Complete original 005f15bc..005f1c41: decode one 4:2:2 row into the float
// row buffer at +0x80. Chroma for odd pixels uses the original 6-tap filter
// (9/16, -1/16) with its edge cases keyed to the even-rounded width.
extern "C" u32 recovered_005f15bc(u32 self, u32 row, u32 slice, u32 decode) {
    if (word(self + 0xa4) == 0)
        return 0x8007000eu;
    if (row >= word(self + 0x88) && row < word(self + 0x90) && slice >= word(self + 0x94) &&
        slice < word(self + 0x98))
        return 0;
    const u32 flushed = FUN_005f12b9(reinterpret_cast<void*>(self));
    if (static_cast<int32_t>(flushed) < 0)
        return flushed;
    put(self + 0x90, row + 1u);
    put(self + 0x88, row);
    put(self + 0x94, slice);
    put(self + 0x98, slice + 1u);
    if (decode == 0)
        return 0;
    u32 in = word(self + 0x60) * row + word(self + 0x64) * slice + word(self + 0x84) * 2u + word(self + 0x20);
    u32 out = word(self + 0x80);
    u32 width = word(self + 0x68);
    if (width & 1u)
        ++width;
    const u32 format = word(self + 4);
    if (format == 0x42475247u || format == 0x47424752u) { // GRGB, RGBG
        u32 x = word(self + 0x84);
        if (x >= word(self + 0x8c))
            return 0;
        const long double scale = dat_006be1d0();
        do {
            const u32 w0 = word16(in);
            x += 2u;
            in += 4u;
            const long double a = integer(shr16(w0, word16(self + 0xa8)) & 0xffu) * scale;
            const long double b = integer(shr16(w0, word16(self + 0xac)) & 0xffu) * scale;
            const u32 w1 = word16(in - 2u);
            const long double c = integer(shr16(w1, word16(self + 0xa8)) & 0xffu) * scale;
            const long double d = integer(shr16(w1, word16(self + 0xac)) & 0xffu) * scale;
            store_float(out + 0x14, c);
            store_float(out, b);
            store_float(out + 4, a);
            store_float(out + 8, d);
            store_float(out + 0xc, 1.0L);
            store_float(out + 0x10, b);
            store_float(out + 0x18, d);
            store_float(out + 0x1c, 1.0L);
            out += 0x20u;
        } while (x < word(self + 0x8c));
        return 0;
    }
    if (format != 0x32595559u && format != 0x59565955u) // YUY2, UYVY
        return 0;
    u32 x = word(self + 0x84);
    if (x >= word(self + 0x8c))
        return 0;
    const long double offset = dat_006c1bbc(); // 128
    do {
        const u32 raw = word16(in);
        const u32 luma = shr16(raw, word16(self + 0xa8)) & 0xffu;
        float yf = static_cast<float>(integer(luma) - dat_006c1bb8());
        const u32 s = word16(self + 0xac);
        auto at = [&](int32_t delta) { return shr32(word16(in + static_cast<u32>(delta)), s) & 0xffu; };
        u32 u, v;
        if ((x & 1u) == 0) {
            u = shr16(raw, s) & 0xffu;
            v = shr16(word16(in + 2u), s) & 0xffu;
        } else if (width == 2u) {
            u = shr16(word16(in - 2u), s) & 0xffu;
            v = shr16(raw, s) & 0xffu;
        } else if (x == 1u && width == 4u) {
            const u32 u0 = at(-2), u1 = at(2);
            u = ((u1 + u0) * 9u - u0 - u1 + 8u) >> 4;
            const u32 v0 = at(0), v1 = at(4);
            v = (v0 * 9u + v1 * 8u - v0 + 8u) >> 4;
        } else if (x == width - 1u) {
            const u32 u0 = at(-2);
            u = (u0 + (u0 << 4) - at(-6) + 8u) >> 4;
            const u32 v0 = at(0);
            v = (v0 + (v0 << 4) - at(-4) + 8u) >> 4;
        } else if (x == 1u) {
            const u32 u0 = at(-2), u1 = at(2);
            u = ((u1 + u0) * 9u - at(6) - u0 + 8u) >> 4;
            const u32 v0 = at(0), v1 = at(4);
            v = ((v1 + v0) * 9u - at(8) - v0 + 8u) >> 4;
        } else if (x == width - 3u) {
            const u32 u0 = at(2), u1 = at(-2);
            u = ((u1 + u0) * 9u - at(-6) - u0 + 8u) >> 4;
            const u32 v1 = at(4) * 8u + 8u - at(-4);
            v = (v1 + at(0) * 9u) >> 4;
        } else {
            const u32 u0 = at(-2), u1 = at(2);
            u = ((u0 + u1) * 9u - at(-6) - at(6) + 8u) >> 4;
            const u32 v0 = at(4), v1 = at(0);
            v = ((v0 + v1) * 9u - at(8) - at(-4) + 8u) >> 4;
        }
        const long double cu = integer(u) - offset, cv = integer(v) - offset;
        const long double ys = static_cast<long double>(yf) * dat_006df3f0();
        yf = static_cast<float>(ys);
        u32 r, g;
        const long double rv = cv * dat_006df3ec() + ys;
        store_float(reinterpret_cast<uintptr_t>(&r), rv);
        store_float(out, rv);
        const long double gv = ys - cu * dat_006df3e8() - cv * dat_006df3e4();
        store_float(reinterpret_cast<uintptr_t>(&g), gv);
        store_float(out + 4, gv);
        long double b = cu * dat_006df3e0() + static_cast<long double>(yf);
        store_float(out + 8, b);
        store_float(out + 0xc, 1.0L);
        clamp_stored(out, reinterpret_cast<uintptr_t>(&r));
        clamp_stored(out + 4, reinterpret_cast<uintptr_t>(&g));
        if (below(compare_value(b, false)))
            b = 0.0L;
        else if (above(compare_value(b, true)))
            b = 1.0L;
        store_float(out + 8, b);
        out += 0x10u;
        in += 2u;
        ++x;
    } while (x < word(self + 0x8c));
    return 0;
}
extern "C" __attribute__((naked, thiscall)) u32 FUN_005f15bc(void*, u32, u32, u32) {
    __asm__ volatile("pushl 12(%esp)\n\tpushl 12(%esp)\n\tpushl 12(%esp)\n\tpushl %ecx\n\t"
                     "calll _recovered_005f15bc\n\taddl $16,%esp\n\tretl $12");
}
namespace {
// REP MOVSD then REP MOVSB, as the original copies a row.
void copy_row(u32 to, u32 from, u32 bytes) {
    u32 dwords = bytes >> 2;
    __asm__ volatile("rep movsl\n\tmovl %3,%%ecx\n\tandl $3,%%ecx\n\trep movsb"
                     : "+D"(to), "+S"(from), "+c"(dwords)
                     : "r"(bytes)
                     : "memory");
}
} // namespace
// Original 005fa96d..005fa996: destructor body of the 4:2:2 surface codec.
// Installs its vtable, flushes the pending row and frees the row buffer
// before the base destructor; EAX is whatever 0x005f0d79 leaves.
extern "C" __attribute__((thiscall)) void FUN_005fa96d(void* object) {
    const u32 self = reinterpret_cast<uintptr_t>(object);
    put(self, reinterpret_cast<uintptr_t>(dat_006df6d4));
    FUN_005f12b9(object);
    if (const u32 buffer = word(self + 0x80))
        fun_005cc620(reinterpret_cast<void*>(buffer));
    FUN_005f0d79(object);
}
// Original 005fa996..005faa24: write one row. The result is the copied
// byte count (width * 16), or the decode error.
extern "C" u32 recovered_005fa996(u32 self, u32 x, u32 y, u32 pixels) {
    u32 in = pixels;
    if (word(self + 0x54))
        in = FUN_005f0da1(reinterpret_cast<void*>(self), reinterpret_cast<const void*>(pixels));
    if (word(self + 0x10))
        in = FUN_005f22d6(reinterpret_cast<void*>(self), reinterpret_cast<const void*>(in));
    const u32 decode = word(self + 0x9c) != word(self + 0x68);
    const u32 result = FUN_005f15bc(reinterpret_cast<void*>(self), x + word(self + 0x40),
                                    y + word(self + 0x4c), decode);
    if (static_cast<int32_t>(result) < 0)
        return result;
    const u32 to = ((word(self + 0x3c) - word(self + 0x84)) << 4) + word(self + 0x80);
    const u32 bytes = word(self + 0x68) << 4;
    copy_row(to, in, bytes);
    put(self + 0xa0, 1);
    return bytes;
}
extern "C" __attribute__((naked, thiscall)) u32 FUN_005fa996(void*, u32, u32, u32) {
    __asm__ volatile("pushl 12(%esp)\n\tpushl 12(%esp)\n\tpushl 12(%esp)\n\tpushl %ecx\n\t"
                     "calll _recovered_005fa996\n\taddl $16,%esp\n\tretl $12");
}
// Original 005faa24..005faa97: read one row. The result is the last
// post-processing call's, or the copied byte count, or the decode error.
extern "C" u32 recovered_005faa24(u32 self, u32 x, u32 y, u32 pixels) {
    u32 result = FUN_005f15bc(reinterpret_cast<void*>(self), x + word(self + 0x40), y + word(self + 0x4c), 1);
    if (static_cast<int32_t>(result) < 0)
        return result;
    const u32 bytes = word(self + 0x68) << 4;
    const u32 from = ((word(self + 0x3c) - word(self + 0x84)) << 4) + word(self + 0x80);
    copy_row(pixels, from, bytes);
    result = bytes;
    if (word(self + 0x18))
        result = FUN_005f225c(reinterpret_cast<void*>(self), reinterpret_cast<void*>(pixels));
    if (word(self + 0x10))
        result = FUN_005f24c9(reinterpret_cast<void*>(self), reinterpret_cast<void*>(pixels));
    return result;
}
extern "C" __attribute__((naked, thiscall)) u32 FUN_005faa24(void*, u32, u32, u32) {
    __asm__ volatile("pushl 12(%esp)\n\tpushl 12(%esp)\n\tpushl 12(%esp)\n\tpushl %ecx\n\t"
                     "calll _recovered_005faa24\n\taddl $16,%esp\n\tretl $12");
}
// Original 005fbc27..005fbc48: scalar deleting destructor (RET 4).
extern "C" __attribute__((thiscall)) void* FUN_005fbc27(void* object, u32 flags) {
    FUN_005fa96d(object);
    if (flags & 1u)
        fun_005cc620(object);
    return object;
}
namespace {
void zero_bytes(u32 to, u32 bytes) {
    u32 dwords = bytes >> 2;
    __asm__ volatile("rep stosl\n\tmovl %3,%%ecx\n\tandl $3,%%ecx\n\trep stosb"
                     : "+D"(to), "+c"(dwords)
                     : "a"(0), "r"(bytes)
                     : "memory");
}
// Four MOVSD: one 16-byte texel.
void copy_texel(u32 to, u32 from) {
    u32 count = 4;
    __asm__ volatile("rep movsl" : "+D"(to), "+S"(from), "+c"(count)::"memory");
}
using BlockDecode = u32(__attribute__((stdcall))*)(u32 texels, u32 block);
using BlockEncode = u32(__attribute__((stdcall))*)(u32 block, u32 texels, u32 flag);
void decode_block(u32 self, u32 texels, u32 block) {
    reinterpret_cast<BlockDecode>(word(self + 0x8c))(texels, block);
}
} // namespace
// Original 005fac81..005fb0a2: write one row of a DXT surface through the
// block cache at +0xf8 (one {row mask, texel buffer} pair per block column
// group). A new cache entry first decodes the blocks the row only partly
// covers; once all four rows are present the blocks are padded, encoded and
// the texel buffer is handed to the next entry or freed.
//
// EAX on return: 0 on an allocation failure, the new row bit while the
// block is incomplete, the handed-over buffer, or (after the free) whatever
// 0x005cc620 leaves, for which this returns 0. Every caller discards it.
extern "C" u32 recovered_005fac81(u32 self, u32 x, u32 y, u32 pixels) {
    x += word(self + 0x40);
    y += word(self + 0x4c);
    if (word(self + 0xf8) == 0) {
        const u32 table = reinterpret_cast<uintptr_t>(fun_005cc96d((word(self + 0xe0) * word(self + 0xe4)) << 3));
        put(self + 0xf8, table);
        if (table == 0)
            return 0;
        zero_bytes(table, (word(self + 0xe0) * word(self + 0xe4)) << 3);
    }
    const u32 entry = word(self + 0xf8) +
                      ((y - word(self + 0xd4)) * word(self + 0xe0) + ((x - word(self + 0xc8)) >> 2)) * 8u;
    if (word(entry + 4) == 0) {
        const u32 count = word(self + 0xdc) << 4;
        const u32 texels = reinterpret_cast<uintptr_t>(fun_005cc96d(count << 4));
        if (texels)
            FUN_00405c90(reinterpret_cast<void*>(texels), 0x10, count, reinterpret_cast<uintptr_t>(&FUN_004441b0));
        put(entry + 4, texels);
        if (texels == 0)
            return 0;
        put(entry, 0);
        put(self + 0xf4, word(self + 0xf4) + 1u);
    }
    if (word(entry) == 0) {
        const u32 left = x & 0xfffffffcu;
        bool whole;
        if (left < word(self + 0x40) && word(self + 0x40) > word(self + 0x98))
            whole = true;
        else
            whole = left + 4u > word(self + 0x48) && word(self + 0x48) < word(self + 0xa0);
        const u32 row_base = (x >> 2) * word(self + 0x60) + word(self + 0x64) * y;
        if (whole) {
            u32 z = word(self + 0xc4);
            u32 block = row_base + (z >> 2) * word(self + 0x88) + word(self + 0x20);
            u32 texels = word(entry + 4);
            while (z < word(self + 0xcc)) {
                decode_block(self, texels, block);
                z += 4u;
                block += word(self + 0x88);
                texels += 0x100u;
            }
        } else {
            const u32 first = word(self + 0xc4), start = word(self + 0x3c);
            if (first < start && start > word(self + 0x94))
                decode_block(self, word(entry + 4),
                             (first >> 2) * word(self + 0x88) + row_base + word(self + 0x20));
            const u32 last = word(self + 0xcc), end = word(self + 0x44);
            // The last partial column is decoded into the first texel group,
            // as in the original.
            if (last > end && end < word(self + 0x9c))
                decode_block(self, word(entry + 4),
                             ((last - 4u) >> 2) * word(self + 0x88) + row_base + word(self + 0x20));
        }
        for (u32 i = 0; i < 4; ++i) {
            const u32 at = i + left;
            if (at < word(self + 0x40) || at >= word(self + 0x48))
                put(entry, word(entry) | 1u << i);
        }
    }
    if (word(self + 0x54))
        pixels = FUN_005f0da1(reinterpret_cast<void*>(self), reinterpret_cast<const void*>(pixels));
    if (word(self + 0x10))
        pixels = FUN_005f22d6(reinterpret_cast<void*>(self), reinterpret_cast<const void*>(pixels));
    u32 column = word(self + 0x3c) - word(self + 0xc4);
    const u32 line = (x - word(self + 0xc8)) & 3u;
    const u32 stop = word(self + 0x68) + column;
    for (; column < stop; ++column) {
        copy_texel((((((column & 0xfffffffcu) | line) << 2) | (column & 3u)) << 4) + word(entry + 4), pixels);
        pixels += 16u;
    }
    const u32 bit = 1u << (x & 3u);
    put(entry, word(entry) | bit);
    if (word(entry) != 0xf)
        return bit;
    u32 z = word(self + 0xc4);
    u32 block = (x >> 2) * word(self + 0x60) + (z >> 2) * word(self + 0x88) + word(self + 0x64) * y +
                word(self + 0x20);
    u32 texels = word(entry + 4);
    if (z < word(self + 0xcc)) {
        // Replicated row/column for texels past the surface edge.
        const u32 source[4] = {0, 0, 0, 1};
        const u32 left = x & 0xfffffffcu;
        do {
            const u32 columns = word(self + 0x9c) - z, rows = word(self + 0xa0) - left;
            if (columns < 4u && rows != 0) {
                for (u32 r = 0; r < 4u;) {
                    for (u32 d = columns; d < 4u; ++d)
                        copy_texel((((r << 2) | d) << 4) + texels, (((r << 2) | source[d]) << 4) + texels);
                    if (++r >= rows)
                        break;
                }
            }
            if (rows < 4u)
                for (u32 r = rows; r < 4u; ++r)
                    for (u32 d = 0; d < 4u; ++d)
                        copy_texel((((r << 2) | d) << 4) + texels, (((source[r] << 2) | d) << 4) + texels);
            const u32 flag = word(self + 0xfc) != 0 || word(self + 0x5c) != 0;
            reinterpret_cast<BlockEncode>(word(self + 0x90))(block, texels, flag);
            z += 4u;
            block += word(self + 0x88);
            texels += 0x100u;
        } while (z < word(self + 0xcc));
    }
    const u32 next = entry + 8u;
    if (next < word(self + 0xf8) + word(self + 0xe4) * word(self + 0xe0) * 8u && word(next) == 0 &&
        word(entry + 0xc) == 0) {
        const u32 moved = word(entry + 4);
        put(entry + 0xc, moved);
        put(entry + 4, 0);
        put(entry + 4, 0);
        return moved;
    }
    fun_005cc620(reinterpret_cast<void*>(word(entry + 4)));
    put(self + 0xf4, word(self + 0xf4) - 1u);
    put(entry + 4, 0);
    return 0;
}
extern "C" __attribute__((naked, thiscall)) u32 FUN_005fac81(void*, u32, u32, u32) {
    __asm__ volatile("pushl 12(%esp)\n\tpushl 12(%esp)\n\tpushl 12(%esp)\n\tpushl %ecx\n\t"
                     "calll _recovered_005fac81\n\taddl $16,%esp\n\tretl $12");
}
__asm__(".text\n"
        ".intel_syntax noprefix\n"
        ".globl _FUN_005fb0a2\n"
        "_FUN_005fb0a2:\n"
        ".globl _surface_block_location_005fb0a2\n"
        "_surface_block_location_005fb0a2:\n"
        "mov edi, edi\n"
        ".globl _surface_block_location_005fb0a4\n"
        "_surface_block_location_005fb0a4:\n"
        "push ebp\n"
        ".globl _surface_block_location_005fb0a5\n"
        "_surface_block_location_005fb0a5:\n"
        "mov ebp, esp\n"
        ".globl _surface_block_location_005fb0a7\n"
        "_surface_block_location_005fb0a7:\n"
        "sub esp, 0x28\n"
        ".globl _surface_block_location_005fb0aa\n"
        "_surface_block_location_005fb0aa:\n"
        "push ebx\n"
        ".globl _surface_block_location_005fb0ab\n"
        "_surface_block_location_005fb0ab:\n"
        "push esi\n"
        ".globl _surface_block_location_005fb0ac\n"
        "_surface_block_location_005fb0ac:\n"
        "mov esi, dword ptr [ebp + 8]\n"
        ".globl _surface_block_location_005fb0af\n"
        "_surface_block_location_005fb0af:\n"
        "push edi\n"
        ".globl _surface_block_location_005fb0b0\n"
        "_surface_block_location_005fb0b0:\n"
        "mov edi, dword ptr [ebp + 0xc]\n"
        ".globl _surface_block_location_005fb0b3\n"
        "_surface_block_location_005fb0b3:\n"
        "mov ebx, ecx\n"
        ".globl _surface_block_location_005fb0b5\n"
        "_surface_block_location_005fb0b5:\n"
        "add esi, dword ptr [ebx + 0x40]\n"
        ".globl _surface_block_location_005fb0b8\n"
        "_surface_block_location_005fb0b8:\n"
        "add edi, dword ptr [ebx + 0x4c]\n"
        ".globl _surface_block_location_005fb0bb\n"
        "_surface_block_location_005fb0bb:\n"
        "cmp dword ptr [ebx + 0xf0], 0\n"
        ".globl _surface_block_location_005fb0c2\n"
        "_surface_block_location_005fb0c2:\n"
        "jne _surface_block_location_005fb106\n"
        ".globl _surface_block_location_005fb0c4\n"
        "_surface_block_location_005fb0c4:\n"
        "mov eax, dword ptr [ebx + 0xdc]\n"
        ".globl _surface_block_location_005fb0ca\n"
        "_surface_block_location_005fb0ca:\n"
        "shl eax, 4\n"
        ".globl _surface_block_location_005fb0cd\n"
        "_surface_block_location_005fb0cd:\n"
        "mov dword ptr [ebp + 8], eax\n"
        ".globl _surface_block_location_005fb0d0\n"
        "_surface_block_location_005fb0d0:\n"
        "shl eax, 4\n"
        ".globl _surface_block_location_005fb0d3\n"
        "_surface_block_location_005fb0d3:\n"
        "push eax\n"
        ".globl _surface_block_location_005fb0d4\n"
        "_surface_block_location_005fb0d4:\n"
        "call _FUN_005cc96d\n"
        ".globl _surface_block_location_005fb0d9\n"
        "_surface_block_location_005fb0d9:\n"
        "test eax, eax\n"
        ".globl _surface_block_location_005fb0db\n"
        "_surface_block_location_005fb0db:\n"
        "pop ecx\n"
        ".globl _surface_block_location_005fb0dc\n"
        "_surface_block_location_005fb0dc:\n"
        "mov dword ptr [ebp + 0xc], eax\n"
        ".globl _surface_block_location_005fb0df\n"
        "_surface_block_location_005fb0df:\n"
        "je _surface_block_location_005fb0f6\n"
        ".globl _surface_block_location_005fb0e1\n"
        "_surface_block_location_005fb0e1:\n"
        ".att_syntax prefix\npushl $_FUN_004441b0\n.intel_syntax noprefix\n"
        ".globl _surface_block_location_005fb0e6\n"
        "_surface_block_location_005fb0e6:\n"
        "push dword ptr [ebp + 8]\n"
        ".globl _surface_block_location_005fb0e9\n"
        "_surface_block_location_005fb0e9:\n"
        "push 0x10\n"
        ".globl _surface_block_location_005fb0eb\n"
        "_surface_block_location_005fb0eb:\n"
        "push eax\n"
        ".globl _surface_block_location_005fb0ec\n"
        "_surface_block_location_005fb0ec:\n"
        "call \"_FUN_00405c90@16\"\n"
        ".globl _surface_block_location_005fb0f1\n"
        "_surface_block_location_005fb0f1:\n"
        "mov eax, dword ptr [ebp + 0xc]\n"
        ".globl _surface_block_location_005fb0f4\n"
        "_surface_block_location_005fb0f4:\n"
        "jmp _surface_block_location_005fb0f8\n"
        ".globl _surface_block_location_005fb0f6\n"
        "_surface_block_location_005fb0f6:\n"
        "xor eax, eax\n"
        ".globl _surface_block_location_005fb0f8\n"
        "_surface_block_location_005fb0f8:\n"
        "test eax, eax\n"
        ".globl _surface_block_location_005fb0fa\n"
        "_surface_block_location_005fb0fa:\n"
        "mov dword ptr [ebx + 0xf0], eax\n"
        ".globl _surface_block_location_005fb100\n"
        "_surface_block_location_005fb100:\n"
        "je _surface_block_location_005fb326\n"
        ".globl _surface_block_location_005fb106\n"
        "_surface_block_location_005fb106:\n"
        "mov eax, esi\n"
        ".globl _surface_block_location_005fb108\n"
        "_surface_block_location_005fb108:\n"
        "lea ecx, [ebx + 0xe8]\n"
        ".globl _surface_block_location_005fb10e\n"
        "_surface_block_location_005fb10e:\n"
        "and eax, 0xfffffffc\n"
        ".globl _surface_block_location_005fb111\n"
        "_surface_block_location_005fb111:\n"
        "cmp eax, dword ptr [ecx]\n"
        ".globl _surface_block_location_005fb113\n"
        "_surface_block_location_005fb113:\n"
        "jne _surface_block_location_005fb11d\n"
        ".globl _surface_block_location_005fb115\n"
        "_surface_block_location_005fb115:\n"
        "cmp edi, dword ptr [ebx + 0xec]\n"
        ".globl _surface_block_location_005fb11b\n"
        "_surface_block_location_005fb11b:\n"
        "je _surface_block_location_005fb182\n"
        ".globl _surface_block_location_005fb11d\n"
        "_surface_block_location_005fb11d:\n"
        "mov dword ptr [ecx], eax\n"
        ".globl _surface_block_location_005fb11f\n"
        "_surface_block_location_005fb11f:\n"
        "mov ecx, dword ptr [ebx + 0xc4]\n"
        ".globl _surface_block_location_005fb125\n"
        "_surface_block_location_005fb125:\n"
        "shr eax, 2\n"
        ".globl _surface_block_location_005fb128\n"
        "_surface_block_location_005fb128:\n"
        "imul eax, dword ptr [ebx + 0x60]\n"
        ".globl _surface_block_location_005fb12c\n"
        "_surface_block_location_005fb12c:\n"
        "mov edx, ecx\n"
        ".globl _surface_block_location_005fb12e\n"
        "_surface_block_location_005fb12e:\n"
        "shr edx, 2\n"
        ".globl _surface_block_location_005fb131\n"
        "_surface_block_location_005fb131:\n"
        "imul edx, dword ptr [ebx + 0x88]\n"
        ".globl _surface_block_location_005fb138\n"
        "_surface_block_location_005fb138:\n"
        "add eax, edx\n"
        ".globl _surface_block_location_005fb13a\n"
        "_surface_block_location_005fb13a:\n"
        "mov edx, dword ptr [ebx + 0x64]\n"
        ".globl _surface_block_location_005fb13d\n"
        "_surface_block_location_005fb13d:\n"
        "imul edx, edi\n"
        ".globl _surface_block_location_005fb140\n"
        "_surface_block_location_005fb140:\n"
        "add eax, edx\n"
        ".globl _surface_block_location_005fb142\n"
        "_surface_block_location_005fb142:\n"
        "add eax, dword ptr [ebx + 0x20]\n"
        ".globl _surface_block_location_005fb145\n"
        "_surface_block_location_005fb145:\n"
        "mov dword ptr [ebx + 0xec], edi\n"
        ".globl _surface_block_location_005fb14b\n"
        "_surface_block_location_005fb14b:\n"
        "mov dword ptr [ebp + 8], eax\n"
        ".globl _surface_block_location_005fb14e\n"
        "_surface_block_location_005fb14e:\n"
        "mov eax, dword ptr [ebx + 0xf0]\n"
        ".globl _surface_block_location_005fb154\n"
        "_surface_block_location_005fb154:\n"
        "mov dword ptr [ebp + 0xc], eax\n"
        ".globl _surface_block_location_005fb157\n"
        "_surface_block_location_005fb157:\n"
        "mov edi, ecx\n"
        ".globl _surface_block_location_005fb159\n"
        "_surface_block_location_005fb159:\n"
        "jmp _surface_block_location_005fb17a\n"
        ".globl _surface_block_location_005fb15b\n"
        "_surface_block_location_005fb15b:\n"
        "push dword ptr [ebp + 8]\n"
        ".globl _surface_block_location_005fb15e\n"
        "_surface_block_location_005fb15e:\n"
        "push dword ptr [ebp + 0xc]\n"
        ".globl _surface_block_location_005fb161\n"
        "_surface_block_location_005fb161:\n"
        "call dword ptr [ebx + 0x8c]\n"
        ".globl _surface_block_location_005fb167\n"
        "_surface_block_location_005fb167:\n"
        "mov eax, dword ptr [ebx + 0x88]\n"
        ".globl _surface_block_location_005fb16d\n"
        "_surface_block_location_005fb16d:\n"
        "add dword ptr [ebp + 8], eax\n"
        ".globl _surface_block_location_005fb170\n"
        "_surface_block_location_005fb170:\n"
        "add dword ptr [ebp + 0xc], 0x100\n"
        ".globl _surface_block_location_005fb177\n"
        "_surface_block_location_005fb177:\n"
        "add edi, 4\n"
        ".globl _surface_block_location_005fb17a\n"
        "_surface_block_location_005fb17a:\n"
        "cmp edi, dword ptr [ebx + 0xcc]\n"
        ".globl _surface_block_location_005fb180\n"
        "_surface_block_location_005fb180:\n"
        "jb _surface_block_location_005fb15b\n"
        ".globl _surface_block_location_005fb182\n"
        "_surface_block_location_005fb182:\n"
        "sub esi, dword ptr [ebx + 0xc8]\n"
        ".globl _surface_block_location_005fb188\n"
        "_surface_block_location_005fb188:\n"
        "mov eax, dword ptr [ebx + 0x3c]\n"
        ".globl _surface_block_location_005fb18b\n"
        "_surface_block_location_005fb18b:\n"
        "sub eax, dword ptr [ebx + 0xc4]\n"
        ".globl _surface_block_location_005fb191\n"
        "_surface_block_location_005fb191:\n"
        "mov ecx, dword ptr [ebx + 0x68]\n"
        ".globl _surface_block_location_005fb194\n"
        "_surface_block_location_005fb194:\n"
        "mov edx, dword ptr [ebp + 0x10]\n"
        ".globl _surface_block_location_005fb197\n"
        "_surface_block_location_005fb197:\n"
        "and esi, 3\n"
        ".globl _surface_block_location_005fb19a\n"
        "_surface_block_location_005fb19a:\n"
        "mov dword ptr [ebp + 8], esi\n"
        ".globl _surface_block_location_005fb19d\n"
        "_surface_block_location_005fb19d:\n"
        "add ecx, eax\n"
        ".globl _surface_block_location_005fb19f\n"
        "_surface_block_location_005fb19f:\n"
        "cmp eax, ecx\n"
        ".globl _surface_block_location_005fb1a1\n"
        "_surface_block_location_005fb1a1:\n"
        "jae _surface_block_location_005fb1ca\n"
        ".globl _surface_block_location_005fb1a3\n"
        "_surface_block_location_005fb1a3:\n"
        "mov esi, eax\n"
        ".globl _surface_block_location_005fb1a5\n"
        "_surface_block_location_005fb1a5:\n"
        "and esi, 0xfffffffc\n"
        ".globl _surface_block_location_005fb1a8\n"
        "_surface_block_location_005fb1a8:\n"
        "or esi, dword ptr [ebp + 8]\n"
        ".globl _surface_block_location_005fb1ab\n"
        "_surface_block_location_005fb1ab:\n"
        "mov edi, eax\n"
        ".globl _surface_block_location_005fb1ad\n"
        "_surface_block_location_005fb1ad:\n"
        "shl esi, 2\n"
        ".globl _surface_block_location_005fb1b0\n"
        "_surface_block_location_005fb1b0:\n"
        "and edi, 3\n"
        ".globl _surface_block_location_005fb1b3\n"
        "_surface_block_location_005fb1b3:\n"
        "or esi, edi\n"
        ".globl _surface_block_location_005fb1b5\n"
        "_surface_block_location_005fb1b5:\n"
        "shl esi, 4\n"
        ".globl _surface_block_location_005fb1b8\n"
        "_surface_block_location_005fb1b8:\n"
        "add esi, dword ptr [ebx + 0xf0]\n"
        ".globl _surface_block_location_005fb1be\n"
        "_surface_block_location_005fb1be:\n"
        "mov edi, edx\n"
        ".globl _surface_block_location_005fb1c0\n"
        "_surface_block_location_005fb1c0:\n"
        "movsd dword ptr es:[edi], dword ptr [esi]\n"
        ".globl _surface_block_location_005fb1c1\n"
        "_surface_block_location_005fb1c1:\n"
        "movsd dword ptr es:[edi], dword ptr [esi]\n"
        ".globl _surface_block_location_005fb1c2\n"
        "_surface_block_location_005fb1c2:\n"
        "movsd dword ptr es:[edi], dword ptr [esi]\n"
        ".globl _surface_block_location_005fb1c3\n"
        "_surface_block_location_005fb1c3:\n"
        "add edx, 0x10\n"
        ".globl _surface_block_location_005fb1c6\n"
        "_surface_block_location_005fb1c6:\n"
        "movsd dword ptr es:[edi], dword ptr [esi]\n"
        ".globl _surface_block_location_005fb1c7\n"
        "_surface_block_location_005fb1c7:\n"
        "inc eax\n"
        ".globl _surface_block_location_005fb1c8\n"
        "_surface_block_location_005fb1c8:\n"
        "jmp _surface_block_location_005fb19f\n"
        ".globl _surface_block_location_005fb1ca\n"
        "_surface_block_location_005fb1ca:\n"
        "cmp dword ptr [ebx + 0x18], 0\n"
        ".globl _surface_block_location_005fb1ce\n"
        "_surface_block_location_005fb1ce:\n"
        "je _surface_block_location_005fb310\n"
        ".globl _surface_block_location_005fb1d4\n"
        "_surface_block_location_005fb1d4:\n"
        "mov eax, dword ptr [ebx + 0x68]\n"
        ".globl _surface_block_location_005fb1d7\n"
        "_surface_block_location_005fb1d7:\n"
        "neg eax\n"
        ".globl _surface_block_location_005fb1d9\n"
        "_surface_block_location_005fb1d9:\n"
        "shl eax, 4\n"
        ".globl _surface_block_location_005fb1dc\n"
        "_surface_block_location_005fb1dc:\n"
        "mov ecx, edx\n"
        ".globl _surface_block_location_005fb1de\n"
        "_surface_block_location_005fb1de:\n"
        "add edx, eax\n"
        ".globl _surface_block_location_005fb1e0\n"
        "_surface_block_location_005fb1e0:\n"
        "fnstcw word ptr [ebp + 0x10]\n"
        ".globl _surface_block_location_005fb1e3\n"
        "_surface_block_location_005fb1e3:\n"
        "mov eax, dword ptr [ebp + 0x10]\n"
        ".globl _surface_block_location_005fb1e6\n"
        "_surface_block_location_005fb1e6:\n"
        "or eax, 0xc00\n"
        ".globl _surface_block_location_005fb1eb\n"
        "_surface_block_location_005fb1eb:\n"
        "mov dword ptr [ebp + 8], eax\n"
        ".globl _surface_block_location_005fb1ee\n"
        "_surface_block_location_005fb1ee:\n"
        "fldcw word ptr [ebp + 8]\n"
        ".globl _surface_block_location_005fb1f1\n"
        "_surface_block_location_005fb1f1:\n"
        "mov eax, dword ptr [ebp + 0x10]\n"
        ".globl _surface_block_location_005fb1f4\n"
        "_surface_block_location_005fb1f4:\n"
        "mov dword ptr [ebx + 0x7c], eax\n"
        ".globl _surface_block_location_005fb1f7\n"
        "_surface_block_location_005fb1f7:\n"
        "jmp _surface_block_location_005fb2ff\n"
        ".globl _surface_block_location_005fb1fc\n"
        "_surface_block_location_005fb1fc:\n"
        "fld dword ptr [edx]\n"
        ".globl _surface_block_location_005fb1fe\n"
        "_surface_block_location_005fb1fe:\n"
        "fmul dword ptr [_dat_006c651c]\n"
        ".globl _surface_block_location_005fb204\n"
        "_surface_block_location_005fb204:\n"
        "fadd dword ptr [_dat_006bc860]\n"
        ".globl _surface_block_location_005fb20a\n"
        "_surface_block_location_005fb20a:\n"
        "fstp dword ptr [ebp + 0x10]\n"
        ".globl _surface_block_location_005fb20d\n"
        "_surface_block_location_005fb20d:\n"
        "fld dword ptr [ebp + 0x10]\n"
        ".globl _surface_block_location_005fb210\n"
        "_surface_block_location_005fb210:\n"
        "fistp dword ptr [ebp + 8]\n"
        ".globl _surface_block_location_005fb213\n"
        "_surface_block_location_005fb213:\n"
        "mov eax, dword ptr [ebp + 8]\n"
        ".globl _surface_block_location_005fb216\n"
        "_surface_block_location_005fb216:\n"
        "mov dword ptr [ebp + 0x10], eax\n"
        ".globl _surface_block_location_005fb219\n"
        "_surface_block_location_005fb219:\n"
        "fild dword ptr [ebp + 0x10]\n"
        ".globl _surface_block_location_005fb21c\n"
        "_surface_block_location_005fb21c:\n"
        "fmul dword ptr [_dat_006c4be8]\n"
        ".globl _surface_block_location_005fb222\n"
        "_surface_block_location_005fb222:\n"
        "fstp dword ptr [ebp - 0x18]\n"
        ".globl _surface_block_location_005fb225\n"
        "_surface_block_location_005fb225:\n"
        "fld dword ptr [edx + 4]\n"
        ".globl _surface_block_location_005fb228\n"
        "_surface_block_location_005fb228:\n"
        "fmul dword ptr [_dat_006df408]\n"
        ".globl _surface_block_location_005fb22e\n"
        "_surface_block_location_005fb22e:\n"
        "fadd dword ptr [_dat_006bc860]\n"
        ".globl _surface_block_location_005fb234\n"
        "_surface_block_location_005fb234:\n"
        "fstp dword ptr [ebp + 0x10]\n"
        ".globl _surface_block_location_005fb237\n"
        "_surface_block_location_005fb237:\n"
        "fld dword ptr [ebp + 0x10]\n"
        ".globl _surface_block_location_005fb23a\n"
        "_surface_block_location_005fb23a:\n"
        "fistp dword ptr [ebp + 0xc]\n"
        ".globl _surface_block_location_005fb23d\n"
        "_surface_block_location_005fb23d:\n"
        "mov eax, dword ptr [ebp + 0xc]\n"
        ".globl _surface_block_location_005fb240\n"
        "_surface_block_location_005fb240:\n"
        "mov dword ptr [ebp + 0x10], eax\n"
        ".globl _surface_block_location_005fb243\n"
        "_surface_block_location_005fb243:\n"
        "fild dword ptr [ebp + 0x10]\n"
        ".globl _surface_block_location_005fb246\n"
        "_surface_block_location_005fb246:\n"
        "fmul dword ptr [_dat_006df404]\n"
        ".globl _surface_block_location_005fb24c\n"
        "_surface_block_location_005fb24c:\n"
        "fstp dword ptr [ebp - 0x14]\n"
        ".globl _surface_block_location_005fb24f\n"
        "_surface_block_location_005fb24f:\n"
        "fld dword ptr [edx + 8]\n"
        ".globl _surface_block_location_005fb252\n"
        "_surface_block_location_005fb252:\n"
        "fmul dword ptr [_dat_006c651c]\n"
        ".globl _surface_block_location_005fb258\n"
        "_surface_block_location_005fb258:\n"
        "fadd dword ptr [_dat_006bc860]\n"
        ".globl _surface_block_location_005fb25e\n"
        "_surface_block_location_005fb25e:\n"
        "fstp dword ptr [ebp + 0x10]\n"
        ".globl _surface_block_location_005fb261\n"
        "_surface_block_location_005fb261:\n"
        "fld dword ptr [ebp + 0x10]\n"
        ".globl _surface_block_location_005fb264\n"
        "_surface_block_location_005fb264:\n"
        "fistp dword ptr [ebp - 4]\n"
        ".globl _surface_block_location_005fb267\n"
        "_surface_block_location_005fb267:\n"
        "mov eax, dword ptr [ebp - 4]\n"
        ".globl _surface_block_location_005fb26a\n"
        "_surface_block_location_005fb26a:\n"
        "mov dword ptr [ebp + 0x10], eax\n"
        ".globl _surface_block_location_005fb26d\n"
        "_surface_block_location_005fb26d:\n"
        "fild dword ptr [ebp + 0x10]\n"
        ".globl _surface_block_location_005fb270\n"
        "_surface_block_location_005fb270:\n"
        "fmul dword ptr [_dat_006c4be8]\n"
        ".globl _surface_block_location_005fb276\n"
        "_surface_block_location_005fb276:\n"
        "fstp dword ptr [ebp - 0x10]\n"
        ".globl _surface_block_location_005fb279\n"
        "_surface_block_location_005fb279:\n"
        "fld dword ptr [edx + 0xc]\n"
        ".globl _surface_block_location_005fb27c\n"
        "_surface_block_location_005fb27c:\n"
        "fmul dword ptr [ebx + 0x80]\n"
        ".globl _surface_block_location_005fb282\n"
        "_surface_block_location_005fb282:\n"
        "fadd dword ptr [_dat_006bc860]\n"
        ".globl _surface_block_location_005fb288\n"
        "_surface_block_location_005fb288:\n"
        "fstp dword ptr [ebp + 0x10]\n"
        ".globl _surface_block_location_005fb28b\n"
        "_surface_block_location_005fb28b:\n"
        "fld dword ptr [ebp + 0x10]\n"
        ".globl _surface_block_location_005fb28e\n"
        "_surface_block_location_005fb28e:\n"
        "fistp dword ptr [ebp - 8]\n"
        ".globl _surface_block_location_005fb291\n"
        "_surface_block_location_005fb291:\n"
        "mov eax, dword ptr [ebp - 8]\n"
        ".globl _surface_block_location_005fb294\n"
        "_surface_block_location_005fb294:\n"
        "mov dword ptr [ebp + 0x10], eax\n"
        ".globl _surface_block_location_005fb297\n"
        "_surface_block_location_005fb297:\n"
        "fild dword ptr [ebp + 0x10]\n"
        ".globl _surface_block_location_005fb29a\n"
        "_surface_block_location_005fb29a:\n"
        "fmul dword ptr [ebx + 0x84]\n"
        ".globl _surface_block_location_005fb2a0\n"
        "_surface_block_location_005fb2a0:\n"
        "fstp dword ptr [ebp - 0xc]\n"
        ".globl _surface_block_location_005fb2a3\n"
        "_surface_block_location_005fb2a3:\n"
        "fld dword ptr [ebx + 0x24]\n"
        ".globl _surface_block_location_005fb2a6\n"
        "_surface_block_location_005fb2a6:\n"
        "fld dword ptr [ebp - 0x18]\n"
        ".globl _surface_block_location_005fb2a9\n"
        "_surface_block_location_005fb2a9:\n"
        "fucompp\n"
        ".globl _surface_block_location_005fb2ab\n"
        "_surface_block_location_005fb2ab:\n"
        "fnstsw ax\n"
        ".globl _surface_block_location_005fb2ad\n"
        "_surface_block_location_005fb2ad:\n"
        "test ah, 0x44\n"
        ".globl _surface_block_location_005fb2b0\n"
        "_surface_block_location_005fb2b0:\n"
        "jp _surface_block_location_005fb2fc\n"
        ".globl _surface_block_location_005fb2b2\n"
        "_surface_block_location_005fb2b2:\n"
        "fld dword ptr [ebx + 0x28]\n"
        ".globl _surface_block_location_005fb2b5\n"
        "_surface_block_location_005fb2b5:\n"
        "fld dword ptr [ebp - 0x14]\n"
        ".globl _surface_block_location_005fb2b8\n"
        "_surface_block_location_005fb2b8:\n"
        "fucompp\n"
        ".globl _surface_block_location_005fb2ba\n"
        "_surface_block_location_005fb2ba:\n"
        "fnstsw ax\n"
        ".globl _surface_block_location_005fb2bc\n"
        "_surface_block_location_005fb2bc:\n"
        "test ah, 0x44\n"
        ".globl _surface_block_location_005fb2bf\n"
        "_surface_block_location_005fb2bf:\n"
        "jp _surface_block_location_005fb2fc\n"
        ".globl _surface_block_location_005fb2c1\n"
        "_surface_block_location_005fb2c1:\n"
        "fld dword ptr [ebx + 0x2c]\n"
        ".globl _surface_block_location_005fb2c4\n"
        "_surface_block_location_005fb2c4:\n"
        "fld dword ptr [ebp - 0x10]\n"
        ".globl _surface_block_location_005fb2c7\n"
        "_surface_block_location_005fb2c7:\n"
        "fucompp\n"
        ".globl _surface_block_location_005fb2c9\n"
        "_surface_block_location_005fb2c9:\n"
        "fnstsw ax\n"
        ".globl _surface_block_location_005fb2cb\n"
        "_surface_block_location_005fb2cb:\n"
        "test ah, 0x44\n"
        ".globl _surface_block_location_005fb2ce\n"
        "_surface_block_location_005fb2ce:\n"
        "jp _surface_block_location_005fb2fc\n"
        ".globl _surface_block_location_005fb2d0\n"
        "_surface_block_location_005fb2d0:\n"
        "fld dword ptr [ebx + 0x30]\n"
        ".globl _surface_block_location_005fb2d3\n"
        "_surface_block_location_005fb2d3:\n"
        "fld dword ptr [ebp - 0xc]\n"
        ".globl _surface_block_location_005fb2d6\n"
        "_surface_block_location_005fb2d6:\n"
        "fucompp\n"
        ".globl _surface_block_location_005fb2d8\n"
        "_surface_block_location_005fb2d8:\n"
        "fnstsw ax\n"
        ".globl _surface_block_location_005fb2da\n"
        "_surface_block_location_005fb2da:\n"
        "test ah, 0x44\n"
        ".globl _surface_block_location_005fb2dd\n"
        "_surface_block_location_005fb2dd:\n"
        "jp _surface_block_location_005fb2fc\n"
        ".globl _surface_block_location_005fb2df\n"
        "_surface_block_location_005fb2df:\n"
        "fldz\n"
        ".globl _surface_block_location_005fb2e1\n"
        "_surface_block_location_005fb2e1:\n"
        "lea esi, [ebp - 0x28]\n"
        ".globl _surface_block_location_005fb2e4\n"
        "_surface_block_location_005fb2e4:\n"
        "fstp dword ptr [ebp - 0x28]\n"
        ".globl _surface_block_location_005fb2e7\n"
        "_surface_block_location_005fb2e7:\n"
        "mov edi, edx\n"
        ".globl _surface_block_location_005fb2e9\n"
        "_surface_block_location_005fb2e9:\n"
        "fldz\n"
        ".globl _surface_block_location_005fb2eb\n"
        "_surface_block_location_005fb2eb:\n"
        "fstp dword ptr [ebp - 0x24]\n"
        ".globl _surface_block_location_005fb2ee\n"
        "_surface_block_location_005fb2ee:\n"
        "fldz\n"
        ".globl _surface_block_location_005fb2f0\n"
        "_surface_block_location_005fb2f0:\n"
        "fstp dword ptr [ebp - 0x20]\n"
        ".globl _surface_block_location_005fb2f3\n"
        "_surface_block_location_005fb2f3:\n"
        "fldz\n"
        ".globl _surface_block_location_005fb2f5\n"
        "_surface_block_location_005fb2f5:\n"
        "fstp dword ptr [ebp - 0x1c]\n"
        ".globl _surface_block_location_005fb2f8\n"
        "_surface_block_location_005fb2f8:\n"
        "movsd dword ptr es:[edi], dword ptr [esi]\n"
        ".globl _surface_block_location_005fb2f9\n"
        "_surface_block_location_005fb2f9:\n"
        "movsd dword ptr es:[edi], dword ptr [esi]\n"
        ".globl _surface_block_location_005fb2fa\n"
        "_surface_block_location_005fb2fa:\n"
        "movsd dword ptr es:[edi], dword ptr [esi]\n"
        ".globl _surface_block_location_005fb2fb\n"
        "_surface_block_location_005fb2fb:\n"
        "movsd dword ptr es:[edi], dword ptr [esi]\n"
        ".globl _surface_block_location_005fb2fc\n"
        "_surface_block_location_005fb2fc:\n"
        "add edx, 0x10\n"
        ".globl _surface_block_location_005fb2ff\n"
        "_surface_block_location_005fb2ff:\n"
        "cmp edx, ecx\n"
        ".globl _surface_block_location_005fb301\n"
        "_surface_block_location_005fb301:\n"
        "jb _surface_block_location_005fb1fc\n"
        ".globl _surface_block_location_005fb307\n"
        "_surface_block_location_005fb307:\n"
        "mov eax, dword ptr [ebx + 0x7c]\n"
        ".globl _surface_block_location_005fb30a\n"
        "_surface_block_location_005fb30a:\n"
        "mov dword ptr [ebp + 0x10], eax\n"
        ".globl _surface_block_location_005fb30d\n"
        "_surface_block_location_005fb30d:\n"
        "fldcw word ptr [ebp + 0x10]\n"
        ".globl _surface_block_location_005fb310\n"
        "_surface_block_location_005fb310:\n"
        "cmp dword ptr [ebx + 0x10], 0\n"
        ".globl _surface_block_location_005fb314\n"
        "_surface_block_location_005fb314:\n"
        "je _surface_block_location_005fb326\n"
        ".globl _surface_block_location_005fb316\n"
        "_surface_block_location_005fb316:\n"
        "mov eax, dword ptr [ebx + 0x68]\n"
        ".globl _surface_block_location_005fb319\n"
        "_surface_block_location_005fb319:\n"
        "shl eax, 4\n"
        ".globl _surface_block_location_005fb31c\n"
        "_surface_block_location_005fb31c:\n"
        "sub edx, eax\n"
        ".globl _surface_block_location_005fb31e\n"
        "_surface_block_location_005fb31e:\n"
        "push edx\n"
        ".globl _surface_block_location_005fb31f\n"
        "_surface_block_location_005fb31f:\n"
        "mov ecx, ebx\n"
        ".globl _surface_block_location_005fb321\n"
        "_surface_block_location_005fb321:\n"
        "call _FUN_005f24c9\n"
        ".globl _surface_block_location_005fb326\n"
        "_surface_block_location_005fb326:\n"
        "pop edi\n"
        ".globl _surface_block_location_005fb327\n"
        "_surface_block_location_005fb327:\n"
        "pop esi\n"
        ".globl _surface_block_location_005fb328\n"
        "_surface_block_location_005fb328:\n"
        "pop ebx\n"
        ".globl _surface_block_location_005fb329\n"
        "_surface_block_location_005fb329:\n"
        "leave\n"
        ".globl _surface_block_location_005fb32a\n"
        "_surface_block_location_005fb32a:\n"
        "ret 0xc\n"
        ".att_syntax prefix\n");
