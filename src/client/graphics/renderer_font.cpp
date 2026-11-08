#include "graphics/render_surface_resource.h"
#include "runtime/allocations.h"
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
// REP STOSD / REP STOSB of `bytes` zero bytes.
void zero_bytes(u32 to, u32 bytes) {
    u32 dwords = bytes >> 2, rest = bytes & 3;
    __asm__ volatile("rep stosl\n\tmovl %2,%%ecx\n\trep stosb"
                     : "+D"(to), "+c"(dwords)
                     : "r"(rest), "a"(0)
                     : "memory");
}
// REP MOVSD / REP MOVSB of `bytes` bytes.
void copy_bytes(u32 to, u32 from, u32 bytes) {
    u32 dwords = bytes >> 2, rest = bytes & 3;
    __asm__ volatile("rep movsl\n\tmovl %3,%%ecx\n\trep movsb"
                     : "+D"(to), "+S"(from), "+c"(dwords)
                     : "r"(rest)
                     : "memory");
}
// Shared body of the six growable-array reserves (thiscall, RET 8) over a
// {data, capacity} pair: nothing to do when `wanted` fits; otherwise the
// capacity becomes double the old one (16 when empty), doubled again until
// it fits, all modulo 2^32 like the original, which therefore spins forever
// once the doubling wraps to zero. The old elements are copied unless
// `discard` is set, the old block is freed and E_OUTOFMEMORY is returned if
// the allocation fails.
u32 reserve(u32 self, u32 wanted, u32 discard, u32 element) {
    if (wanted <= word(self + 4))
        return 0;
    const u32 capacity = word(self + 4);
    u32 grown = capacity ? capacity * 2 : 16;
    while (grown < wanted) {
        grown *= 2;
        __asm__ volatile("" ::: "memory");  // keeps the original endless loop observable
    }
    const u32 block = address(fun_005cc96d(grown * element));
    if (!block)
        return 0x8007000e;
    if (!discard)
        copy_bytes(block, word(self), word(self + 4) * element);
    fun_005cc620(pointer(word(self)));
    put(self, block);
    put(self + 4, grown);
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
__declspec(dllimport) void* __attribute__((stdcall)) SelectObject(void*, void*);
__declspec(dllimport) int32_t __attribute__((stdcall)) DeleteObject(void*);
__declspec(dllimport) int32_t __attribute__((stdcall)) DeleteDC(void*);
__declspec(dllimport) void* __attribute__((stdcall))
CreateDIBSection(void*, const void*, uint32_t, void*, void*, uint32_t);
__declspec(dllimport) int32_t __attribute__((stdcall)) GetVersionExA(void*);
__declspec(dllimport) int32_t __attribute__((stdcall))
MultiByteToWideChar(uint32_t, uint32_t, const char*, int32_t, uint16_t*, int32_t);
// Nonzero when the wide GDI entry points are usable (renderer_font_storage.cpp).
extern uint32_t dat_0224f34c;
// Set once dat_0224f34c has been computed (renderer_font_storage.cpp).
extern uint32_t dat_0224f350;
// ScriptFreeCache, resolved from usp10.dll at runtime (renderer_font_storage.cpp).
extern uint32_t dat_0224f390;
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
u32 __attribute__((stdcall)) font_release(u32) __asm__("_FUN_00600706");
extern const unsigned char font_vtable[] __asm__("_dat_006df828");
void* __attribute__((thiscall)) FUN_005e8bbe(void*, u32);
void __attribute__((thiscall)) FUN_005ff43b(void*);
void* __attribute__((thiscall)) FUN_005ff20b(void*, u32);
void __attribute__((thiscall)) FUN_006005ce(void*);
void* __attribute__((thiscall)) FUN_00600603(void*);
uint32_t FUN_005e8c00();
void* __attribute__((thiscall)) FUN_005ff085(void*, const void*, void*);
u32 __attribute__((thiscall)) FUN_005ff10b(void*, u32, u32);
u32 __attribute__((thiscall)) FUN_005ff18b(void*, u32, u32);
u32 __attribute__((thiscall)) FUN_005ff22c(void*, u32, u32);
u32 __attribute__((thiscall)) FUN_005ff2ab(void*, u32, u32);
u32 __attribute__((thiscall)) FUN_005ff328(void*, u32, u32);
u32 __attribute__((thiscall)) FUN_005ff3a8(void*, u32, u32);
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
// Original 00600706..00600721 (RET 4): the new count, or 0 after the
// scalar deleting destructor ran on the last reference.
u32 font_release(u32 self) {
    put(self + 4, word(self + 4) - 1);
    const u32 count = word(self + 4);
    if (count)
        return count;
    FUN_005e8bbe(pointer(self), 1);
    return 0;
}
// Original 005e8bbe..005e8bdd (thiscall, RET 4): scalar deleting destructor.
// Both callers ignore EAX.
extern "C" void* __attribute__((thiscall)) FUN_005e8bbe(void* object, u32 flags) {
    FUN_005ff43b(object);
    if (flags & 1)
        fun_005cc620(object);
    return object;
}
// Original 005ff43b..005ff600 (thiscall, RET): destructor body. Installs the
// vtable, releases +8 and the sprite at +0x0c, restores the objects the DC
// had selected (+0x18, +0x20) before deleting the fonts +0x14 and +0x1c and
// the DC, frees the +0x8c-counted arrays at +0x90, releases the non-NULL
// entries of the +0x498-counted table at +0x49c, frees the Uniscribe cache at
// +0x70, the buffer +0x4f8, the tree at +0x490, sixteen buffers from +0x57c
// down to +0x500 and the table itself. Counts are re-read on every iteration,
// as in the original. The only caller overwrites EAX.
extern "C" void __attribute__((thiscall)) FUN_005ff43b(void* object) {
    const u32 self = address(object);
    put(self, address(font_vtable));
    for (u32 field = self + 8; field <= self + 0x0c; field += 4)
        if (const u32 com = word(field)) {
            method<Method0>(com, 8)(com);
            put(field, 0);
        }
    if (word(self + 0x10) && word(self + 0x18))
        SelectObject(pointer(word(self + 0x10)), pointer(word(self + 0x18)));
    if (const u32 font = word(self + 0x14))
        DeleteObject(pointer(font));
    if (word(self + 0x10) && word(self + 0x20))
        SelectObject(pointer(word(self + 0x10)), pointer(word(self + 0x20)));
    if (const u32 font = word(self + 0x1c))
        DeleteObject(pointer(font));
    if (const u32 dc = word(self + 0x10))
        DeleteDC(pointer(dc));
    for (u32 i = 0; i < word(self + 0x8c); ++i)
        fun_005cc620(pointer(word(self + 0x90 + 4 * i)));
    for (u32 i = 0; i < word(self + 0x498); ++i)
        if (const u32 entry = word(word(self + 0x49c) + 4 * i))
            method<Method0>(entry, 8)(entry);
    if (word(self + 0x70))
        reinterpret_cast<u32(__attribute__((stdcall))*)(u32)>(dat_0224f390)(self + 0x70);
    fun_005cc620(pointer(word(self + 0x4f8)));
    if (const u32 tree = word(self + 0x490))
        FUN_005ff20b(pointer(tree), 1);
    fun_005cc620(pointer(word(self + 0x57c)));
    for (u32 offset = 0x570; offset >= 0x500; offset -= 8)
        fun_005cc620(pointer(word(self + offset)));
    fun_005cc620(pointer(word(self + 0x49c)));
}
// Original 005ff20b..005ff22a (thiscall, RET 4): scalar deleting destructor
// of a tree node. Both callers ignore EAX.
extern "C" void* __attribute__((thiscall)) FUN_005ff20b(void* node, u32 flags) {
    FUN_006005ce(node);
    if (flags & 1)
        fun_005cc620(node);
    return node;
}
// Original 006005ce..00600603 (thiscall, RET): tree node destructor body:
// the children at +4 and +8, then the buffers at +0x10 and +0x14.
extern "C" void __attribute__((thiscall)) FUN_006005ce(void* node) {
    const u32 self = address(node);
    if (const u32 left = word(self + 4))
        FUN_005ff20b(pointer(left), 1);
    if (const u32 right = word(self + 8))
        FUN_005ff20b(pointer(right), 1);
    fun_005cc620(pointer(word(self + 0x10)));
    fun_005cc620(pointer(word(self + 0x14)));
}
// Original 00600603..00600706 (thiscall, RET): constructor. Returns the
// object; callers read nothing else.
extern "C" void* __attribute__((thiscall)) FUN_00600603(void* object) {
    const u32 self = address(object);
    put(self, address(font_vtable));
    put(self + 0x49c, 0);
    put(self + 0x4a0, 0);
    for (u32 offset = 0x500; offset <= 0x570; offset += 8) {
        put(self + offset, 0);
        put(self + offset + 4, 0);
    }
    put(self + 0x57c, 0);
    put(self + 0x580, 0);
    put(self + 4, 1);
    for (u32 offset = 8; offset <= 0x20; offset += 4)
        put(self + offset, 0);
    static const unsigned short zeroed[6] = {0x70, 0x578, 0x8c, 0x498, 0x4f8, 0x490};
    for (u32 i = 0; i < 6; ++i)
        put(self + zeroed[i], 0);
    return object;
}
// Original 005e8c00..005e8c4f (cdecl): whether GetVersionExA reports
// VER_PLATFORM_WIN32_NT, computed once. The flag dat_0224f350 is set before
// the call. The original reads a stale frame if GetVersionExA fails; here
// the platform field is zero.
extern "C" uint32_t FUN_005e8c00() {
    if (dat_0224f350)
        return dat_0224f34c;
    u32 info[37];
    zero_bytes(address(info), sizeof info);  // no implicit memset call
    dat_0224f350 = 1;
    info[0] = 0x94;
    GetVersionExA(info);
    dat_0224f34c = info[4] == 2;
    return dat_0224f34c;
}
// Original 005ff085..005ff10b (thiscall, RET 8): a top-down 32-bit DIB
// section of the {width, height} at `size` on the DC at +0x10. The
// BITMAPINFO block is header plus width * height * 4 bytes (modulo 2^32),
// zero-filled; NULL when it cannot be allocated.
extern "C" void* __attribute__((thiscall)) FUN_005ff085(void* object, const void* size,
                                                        void* bits) {
    const u32 dims = address(size);
    const u32 bytes = word(dims) * word(dims + 4) * 4 + 0x28;
    const u32 info = address(fun_005cc96d(bytes));
    if (!info)
        return nullptr;
    zero_bytes(info, bytes);
    put(info, 0x28);
    put(info + 4, word(dims));
    put(info + 8, 0u - word(dims + 4));
    put(info + 0x10, 0);
    put_half(info + 0x0c, 1);
    put_half(info + 0x0e, 0x20);
    void* const bitmap =
        CreateDIBSection(pointer(word(address(object) + 0x10)), pointer(info), 0, bits, nullptr, 0);
    fun_005cc620(pointer(info));
    return bitmap;
}
// Originals 005ff10b, 005ff18b, 005ff22c, 005ff2ab, 005ff328 and 005ff3a8:
// the reserve above for 8-, 16-, 2-, 1-, 4- and 12-byte elements.
extern "C" u32 __attribute__((thiscall)) FUN_005ff10b(void* self, u32 wanted, u32 discard) {
    return reserve(address(self), wanted, discard, 8);
}
extern "C" u32 __attribute__((thiscall)) FUN_005ff18b(void* self, u32 wanted, u32 discard) {
    return reserve(address(self), wanted, discard, 16);
}
extern "C" u32 __attribute__((thiscall)) FUN_005ff22c(void* self, u32 wanted, u32 discard) {
    return reserve(address(self), wanted, discard, 2);
}
extern "C" u32 __attribute__((thiscall)) FUN_005ff2ab(void* self, u32 wanted, u32 discard) {
    return reserve(address(self), wanted, discard, 1);
}
extern "C" u32 __attribute__((thiscall)) FUN_005ff328(void* self, u32 wanted, u32 discard) {
    return reserve(address(self), wanted, discard, 4);
}
extern "C" u32 __attribute__((thiscall)) FUN_005ff3a8(void* self, u32 wanted, u32 discard) {
    return reserve(address(self), wanted, discard, 12);
}
