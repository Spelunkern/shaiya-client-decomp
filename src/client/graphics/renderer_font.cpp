#include "graphics/render_surface_resource.h"
// Methods of the font object whose vtable is dat_006df828. Only the field
// offsets the original reads are recovered: +0x0c the sprite the font draws
// with, +0x10 its memory DC, +0x14 its HFONT, +0x30 the cached TEXTMETRICW
// and +0x4fc the mip level count. The vtable and the asm still in
// renderer_font_instructions.cpp reference the undecorated original names,
// so every definition keeps its symbol label.
namespace {
using u32 = uint32_t;
u32 word(u32 p) {
    u32 result;
    __asm__ volatile("movl (%1),%0" : "=r"(result) : "r"(p) : "memory");
    return result;
}
void put(u32 p, u32 v) {
    __asm__ volatile("movl %1,(%0)" ::"r"(p), "r"(v) : "memory");
}
u32 byte(u32 p) {
    u32 result;
    __asm__ volatile("movzbl (%1),%0" : "=r"(result) : "r"(p) : "memory");
    return result;
}
void put_byte(u32 p, u32 v) {
    __asm__ volatile("movb %b1,(%0)" ::"r"(p), "q"(v) : "memory");
}
void put_half(u32 p, u32 v) {
    __asm__ volatile("movw %w1,(%0)" ::"r"(p), "r"(v) : "memory");
}
u32 address(const void* p) {
    return static_cast<u32>(reinterpret_cast<uintptr_t>(p));
}
void* pointer(u32 p) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(p));
}
// REP MOVSD of `dwords` dwords.
void copy_dwords(u32 to, u32 from, u32 dwords) {
    __asm__ volatile("rep movsl" : "+D"(to), "+S"(from), "+c"(dwords)::"memory");
}
template <typename Method> Method method(u32 object, u32 offset) {
    return reinterpret_cast<Method>(word(word(object) + offset));
}
using Method0 = u32(__attribute__((stdcall))*)(u32);
// REPE CMPSD over four dwords.
bool equal_guid(u32 left, u32 right) {
    for (u32 i = 0; i < 16; i += 4)
        if (word(left + i) != word(right + i))
            return false;
    return true;
}
// The fields shared by D3DXFONT_DESCA and D3DXFONT_DESCW, from a LOGFONT at
// `font`: height, width, weight, the mip levels of the object, italic as a
// zero-extended BOOL, then charset, output precision, quality and pitch.
void store_desc_head(u32 desc, u32 font, u32 self) {
    put(desc, word(font));
    put(desc + 4, word(font + 4));
    put(desc + 8, word(font + 0x10));
    put(desc + 0x0c, word(self + 0x4fc));
    put(desc + 0x10, byte(font + 0x14));
    put_byte(desc + 0x14, byte(font + 0x17));
    put_byte(desc + 0x15, byte(font + 0x18));
    put_byte(desc + 0x16, byte(font + 0x1a));
    put_byte(desc + 0x17, byte(font + 0x1b));
}
// Shared body of OnLostDevice/OnResetDevice: forward to the sprite at +0x0c
// and return its failure, or 0.
u32 forward_to_sprite(u32 self, u32 offset) {
    if (word(self + 0x0c)) {
        const u32 sprite = word(self + 0x0c);
        const u32 result = method<Method0>(sprite, offset)(sprite);
        if (static_cast<int32_t>(result) < 0)
            return result;
    }
    return 0;
}
constexpr u32 invalid_call = 0x8876086c;
} // namespace
extern "C" {
__declspec(dllimport) int32_t __attribute__((stdcall)) GetObjectA(void*, int32_t, void*);
__declspec(dllimport) int32_t __attribute__((stdcall)) GetObjectW(void*, int32_t, void*);
__declspec(dllimport) int32_t __attribute__((stdcall)) GetTextMetricsA(void*, void*);
__declspec(dllimport) uint32_t __attribute__((stdcall))
GetGlyphOutlineA(void*, uint32_t, uint32_t, void*, uint32_t, void*, const void*);
__declspec(dllimport) int32_t __attribute__((stdcall))
MultiByteToWideChar(uint32_t, uint32_t, const char*, int32_t, uint16_t*, int32_t);
// Nonzero when the wide GDI entry points are usable (renderer_font_storage.cpp).
extern uint32_t dat_0224f34c;
// The interface id of the font and its identity glyph matrix (renderer_font_storage.cpp).
extern const unsigned char font_iid[] __asm__("_dat_006effe4");
extern const unsigned char identity_glyph_matrix[] __asm__("_dat_006df814");
u32 __attribute__((stdcall)) font_query_interface(u32, u32, u32) __asm__("_FUN_005fee06");
u32 __attribute__((stdcall)) font_get_desc_a(u32, u32) __asm__("_FUN_005fee4c");
u32 __attribute__((stdcall)) font_get_desc_w(u32, u32) __asm__("_FUN_005feecd");
u32 __attribute__((stdcall)) font_get_text_metrics_a(u32, u32) __asm__("_FUN_005fefc6");
u32 __attribute__((stdcall)) font_get_text_metrics_w(u32, u32) __asm__("_FUN_005fefde");
u32 __attribute__((stdcall)) font_get_dc(u32) __asm__("_FUN_005ff000");
u32 __attribute__((stdcall)) font_on_lost_device(u32) __asm__("_FUN_005ff00f");
u32 __attribute__((stdcall)) font_on_reset_device(u32) __asm__("_FUN_005ff030");
u32 __attribute__((thiscall)) font_has_glyph(u32, u32) __asm__("_FUN_005ff051");
}
// Original 005fee06..005fee4c (RET 12). The output is written only on success.
u32 font_query_interface(u32 self, u32 iid, u32 output) {
    if (!equal_guid(iid, address(font_iid)) && !equal_guid(iid, address(dat_006f73f8)))
        return 0x80004002;
    put(output, self);
    method<Method0>(self, 4)(self);
    return 0;
}
// Original 005fee4c..005feecd (RET 8): D3DXFONT_DESCA from GetObjectA of
// the HFONT. The original reads a stale frame if GetObjectA writes fewer
// than 0x3c bytes; here the unwritten bytes are zero.
u32 font_get_desc_a(u32 self, u32 desc) {
    if (!desc)
        return invalid_call;
    u32 font[15] = {};
    if (!GetObjectA(pointer(word(self + 0x14)), 0x3c, font))
        return invalid_call;
    store_desc_head(desc, address(font), self);
    copy_dwords(desc + 0x18, address(font) + 0x1c, 8);
    return 0;
}
// Original 005feecd..005fefc6 (RET 8): D3DXFONT_DESCW. With the wide GDI
// entry points it copies LOGFONTW; otherwise it widens the ANSI face name
// with MultiByteToWideChar (result ignored) and terminates it at +0x56.
// Unwritten LOGFONT bytes are zero here, stale in the original.
u32 font_get_desc_w(u32 self, u32 desc) {
    if (!desc)
        return invalid_call;
    if (dat_0224f34c) {
        u32 font[23] = {};
        if (!GetObjectW(pointer(word(self + 0x14)), 0x5c, font))
            return invalid_call;
        store_desc_head(desc, address(font), self);
        copy_dwords(desc + 0x18, address(font) + 0x1c, 16);
        return 0;
    }
    u32 font[15] = {};
    if (!GetObjectA(pointer(word(self + 0x14)), 0x3c, font))
        return invalid_call;
    store_desc_head(desc, address(font), self);
    MultiByteToWideChar(0, 0, static_cast<const char*>(pointer(address(font) + 0x1c)), -1,
                        static_cast<uint16_t*>(pointer(desc + 0x18)), 0x20);
    put_half(desc + 0x56, 0);
    return 0;
}
// Original 005fefc6..005fefde (RET 8): GetTextMetricsA on the DC.
u32 font_get_text_metrics_a(u32 self, u32 metrics) {
    return static_cast<u32>(GetTextMetricsA(pointer(word(self + 0x10)), pointer(metrics)));
}
// Original 005fefde..005ff000 (RET 8): copies the cached TEXTMETRICW
// (15 dwords at +0x30); TRUE, or FALSE for a NULL output.
u32 font_get_text_metrics_w(u32 self, u32 metrics) {
    if (!metrics)
        return 0;
    copy_dwords(metrics, self + 0x30, 15);
    return 1;
}
// Original 005ff000..005ff00f (RET 4).
u32 font_get_dc(u32 self) {
    return word(self + 0x10);
}
// Original 005ff00f..005ff030 (RET 4): sprite OnLostDevice (vtable +0x30).
u32 font_on_lost_device(u32 self) {
    return forward_to_sprite(self, 0x30);
}
// Original 005ff030..005ff051 (RET 4): sprite OnResetDevice (vtable +0x34).
u32 font_on_reset_device(u32 self) {
    return forward_to_sprite(self, 0x34);
}
// Original 005ff051..005ff085 (thiscall, RET 4): whether the glyph index has
// an outline, through GetGlyphOutlineA(GGO_GLYPH_INDEX) with no buffer and
// the identity matrix. Callers read only EAX.
u32 font_has_glyph(u32 self, u32 glyph) {
    u32 metrics[5];
    return GetGlyphOutlineA(pointer(word(self + 0x10)), glyph, 0x80, metrics, 0, nullptr,
                            identity_glyph_matrix) != 0xffffffffu;
}
