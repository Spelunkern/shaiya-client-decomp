#include "graphics/render_debug_mute.h"
#include "graphics/render_surface_resource.h"
// Methods of the sprite object whose vtable is dat_006df7dc. Only the field
// offsets the original reads are recovered; no class layout is inferred.
// The vtable and the asm still in renderer_font_instructions.cpp reference the
// undecorated original names, so every definition keeps its symbol label.
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
u32 address(const void* p) {
    return static_cast<u32>(reinterpret_cast<uintptr_t>(p));
}
// Slot `offset` of the COM vtable of `object`.
template <typename Method> Method method(u32 object, u32 offset) {
    return reinterpret_cast<Method>(word(word(object) + offset));
}
using Method0 = u32(__attribute__((stdcall))*)(u32);
using Method1 = u32(__attribute__((stdcall))*)(u32, u32);
using Method2 = u32(__attribute__((stdcall))*)(u32, u32, u32);
using Method6 = u32(__attribute__((stdcall))*)(u32, u32, u32, u32, u32, u32, u32);
void addref(u32 object) {
    method<Method0>(object, 4)(object);
}
void release(u32 object) {
    method<Method0>(object, 8)(object);
}
// REPE CMPSD over four dwords.
bool equal_guid(u32 left, u32 right) {
    for (u32 i = 0; i < 16; i += 4)
        if (word(left + i) != word(right + i))
            return false;
    return true;
}
// FLD m32 / FCOMP m32 / FNSTSW AX: AH of the status word. FCOMP (not FUCOMP)
// keeps the invalid-operation signal on a QNaN key.
u32 compare_status(u32 left, u32 right) {
    u32 status;
    __asm__ volatile("flds (%1)\n\tfcomps (%2)\n\tfnstsw %%ax"
                     : "=a"(status)
                     : "r"(left), "r"(right)
                     : "cc", "memory");
    return (status >> 8) & 0xff;
}
// TEST AH,mask / JP: the jump is taken on even parity of AH & mask.
bool parity_even(u32 bits) {
    return !__builtin_parity(bits);
}
// Shared tail of the three comparators: equal keys order by index, ascending.
int32_t by_index(u32 a, u32 b) {
    if (b > a)
        return -1;
    return b < a ? 1 : 0;
}
constexpr u32 invalid_call = 0x8876086c;
} // namespace
extern "C" {
// The interface id of the sprite, defined in renderer_font_storage.cpp.
extern const unsigned char sprite_iid[] __asm__("_dat_006efff4");
u32 __attribute__((stdcall)) sprite_query_interface(u32, u32, u32) __asm__("_FUN_005fd2fb");
u32 __attribute__((stdcall)) sprite_get_device(u32, u32) __asm__("_FUN_005fd341");
u32 __attribute__((stdcall)) sprite_get_transform(u32, u32) __asm__("_FUN_005fd36b");
u32 __attribute__((stdcall)) sprite_end(u32) __asm__("_FUN_005fd396");
u32 __attribute__((stdcall)) sprite_on_lost_device(u32) __asm__("_FUN_005fd444");
u32 __attribute__((stdcall)) sprite_on_reset_device(u32) __asm__("_FUN_005fd4ee");
int32_t __attribute__((stdcall)) sprite_order_by_word(u32, u32, u32) __asm__("_FUN_005fd572");
int32_t __attribute__((stdcall)) sprite_order_by_depth(u32, u32, u32) __asm__("_FUN_005fd5b6");
int32_t __attribute__((stdcall)) sprite_order_by_depth_reverse(u32, u32, u32)
    __asm__("_FUN_005fd602");
}
// Original 005fd2fb..005fd341 (RET 12). The output is written only on success.
u32 sprite_query_interface(u32 self, u32 iid, u32 output) {
    if (!equal_guid(iid, address(sprite_iid)) && !equal_guid(iid, address(dat_006f73f8)))
        return 0x80004002;
    put(output, self);
    addref(self);
    return 0;
}
// Original 005fd341..005fd36b (RET 8): device at +0x0c, read again after the store.
u32 sprite_get_device(u32 self, u32 output) {
    if (!output)
        return invalid_call;
    put(output, word(self + 0x0c));
    addref(word(self + 0x0c));
    return 0;
}
// Original 005fd36b..005fd393 (RET 8): REP MOVSD of the 4x4 matrix at *(+0x70).
u32 sprite_get_transform(u32 self, u32 output) {
    if (!output)
        return invalid_call;
    u32 from = word(self + 0x70), count = 16;
    __asm__ volatile("rep movsl" : "+D"(output), "+S"(from), "+c"(count)::"memory");
    return 0;
}
// Original 005fd396..005fd444 (RET 4). +0xac is the "inside Begin" flag and
// +0xb0 the Begin flags, re-read after each call as the original does. The
// result is the Flush result (vtable +0x28), or 0 when nothing is queued.
u32 sprite_end(u32 self) {
    if (!word(self + 0xac))
        return invalid_call;
    const u32 result = word(self + 0xc4) ? method<Method0>(self, 0x28)(self) : 0;
    FUN_005e9b1d(1);
    if (!(byte(self + 0xb0) & 3)) {
        if (const u32 block = word(self + 0x24))
            method<Method0>(block, 0x14)(block);
        const u32 device = word(self + 0x0c);
        method<Method1>(device, 0x1a0)(device, word(self + 0x3c));
    }
    u32 flags = word(self + 0xb0);
    if ((flags & 0x10) && !(flags & 1))
        if (const u32 block = word(self + 0x2c))
            method<Method0>(block, 0x14)(block);
    flags = word(self + 0xb0);
    if ((!(flags & 4) || (flags & 8)) && !(flags & 1))
        if (const u32 block = word(self + 0x34))
            method<Method0>(block, 0x14)(block);
    FUN_005e9b1d(0);
    put(self + 0xac, 0);
    return result;
}
// Original 005fd444..005fd4ee (RET 4): clear +0xac, then release and clear
// the nine device objects in the original order.
u32 sprite_on_lost_device(u32 self) {
    put(self + 0xac, 0);
    static const unsigned char fields[9] = {0x14, 0x18, 0x3c, 0x28, 0x24, 0x30, 0x2c, 0x38, 0x34};
    for (u32 i = 0; i < 9; ++i) {
        const u32 field = self + fields[i];
        if (const u32 object = word(field)) {
            release(object);
            put(field, 0);
        }
    }
    return 0;
}
// Original 005fd4ee..005fd572 (RET 4). +0x60 records whether the current
// render-target format supports post-pixel-shader blending: GetRenderTarget
// (0x98), GetDesc (0x30), GetDisplayMode (0x20), then CheckDeviceFormat (0x28)
// on the Direct3D object at +8 with adapter +0x44 and device type +0x40. The
// result is 0 on every path, including a failed GetRenderTarget. The original
// ignores the GetDesc and GetDisplayMode results; if they failed it would read
// stale frame bytes, here it reads the zeroed locals. GetRenderTarget writes
// into the original's own argument slot, so that slot starts out as `self`.
u32 sprite_on_reset_device(u32 self) {
    u32 desc[8] = {}, mode[4] = {}, target = self;
    const u32 device = word(self + 0x0c);
    put(self + 0xac, 0);
    put(self + 0x60, 1);
    if (static_cast<int32_t>(method<Method2>(device, 0x98)(device, 0, address(&target))) < 0)
        return 0;
    method<Method1>(target, 0x30)(target, address(desc));
    const u32 device2 = word(self + 0x0c);
    method<Method2>(device2, 0x20)(device2, 0, address(mode));
    const u32 d3d = word(self + 8);
    if (static_cast<int32_t>(method<Method6>(d3d, 0x28)(d3d, word(self + 0x44), word(self + 0x40),
                                                         mode[3], 0x80001, 1, desc[0])) < 0)
        put(self + 0x60, 0);
    if (target)
        release(target);
    return 0;
}
// Original 005fd572..005fd5b6 (RET 12): sort callback of 0x0065533c over the
// u32 keys at *(context + 0xb8); larger keys first.
int32_t sprite_order_by_word(u32 a, u32 b, u32 context) {
    const u32 keys = word(context + 0xb8);
    if (word(keys + a * 4) > word(keys + b * 4))
        return -1;
    const u32 again = word(context + 0xb8);
    if (word(again + a * 4) < word(again + b * 4))
        return 1;
    return by_index(a, b);
}
// Original 005fd5b6..005fd602 (RET 12): float keys at *(context + 0xbc),
// smaller first. Unordered keys fall through to the index order.
int32_t sprite_order_by_depth(u32 a, u32 b, u32 context) {
    const u32 keys = word(context + 0xbc);
    if (!parity_even(compare_status(keys + a * 4, keys + b * 4) & 5))
        return -1;
    const u32 again = word(context + 0xbc);
    if (!(compare_status(again + a * 4, again + b * 4) & 0x41))
        return 1;
    return by_index(a, b);
}
// Original 005fd602..005fd64e (RET 12): the same keys, larger first.
int32_t sprite_order_by_depth_reverse(u32 a, u32 b, u32 context) {
    const u32 keys = word(context + 0xbc);
    if (!(compare_status(keys + a * 4, keys + b * 4) & 0x41))
        return -1;
    const u32 again = word(context + 0xbc);
    if (!parity_even(compare_status(again + a * 4, again + b * 4) & 5))
        return 1;
    return by_index(a, b);
}
