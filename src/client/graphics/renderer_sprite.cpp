#include "graphics/render_debug_mute.h"
#include "graphics/render_dispatch.h"
#include "graphics/render_matrix_pair.h"
#include "graphics/render_packed_trig_quaternions.h"
#include "graphics/render_project_unproject.h"
#include "graphics/render_surface_resource.h"
#include "graphics/render_transform_arrays_dispatch.h"
#include "runtime/allocations.h"
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
// FLD constant / FLD m32 / FUCOMPP / FNSTSW AX, then TEST AH,0x44 / JP:
// only the equal outcome (C3 alone) has odd parity. The compare is quiet.
bool quiet_equal(u32 value, const uint32_t* constant) {
    u32 status;
    __asm__ volatile("flds (%1)\n\tflds (%2)\n\tfucompp\n\tfnstsw %%ax"
                     : "=a"(status)
                     : "r"(constant), "r"(value)
                     : "cc", "memory");
    return __builtin_parity((status >> 8) & 0x44);
}
// FLD m32 / FCHS / FSTP m32: a signaling NaN comes out quiet, as in the original.
void negate_float(u32 from, u32 to) {
    __asm__ volatile("flds (%0)\n\tfchs\n\tfstps (%1)" ::"r"(from), "r"(to) : "memory");
}
void copy_matrix(u32 to, u32 from) {
    u32 count = 16;
    __asm__ volatile("rep movsl" : "+D"(to), "+S"(from), "+c"(count)::"memory");
}
void* pointer(u32 p) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(p));
}
// Shared head of SetWorldViewRH/LH: the view matrix at *(+0x80) becomes
// world * view, the one that is not NULL, or identity (stored in the
// original order: the twelve zeros, then the four ones).
void store_world_view(u32 self, u32 world, u32 view) {
    if (world && view) {
        FUN_005e4db8(static_cast<D3DMATRIX*>(pointer(word(self + 0x80))),
                     static_cast<const D3DMATRIX*>(pointer(world)),
                     static_cast<const D3DMATRIX*>(pointer(view)));
    } else if (world || view) {
        copy_matrix(word(self + 0x80), world ? world : view);
    } else {
        const u32 out = word(self + 0x80);
        static const unsigned char zeros[12] = {0x38, 0x34, 0x30, 0x2c, 0x24, 0x20,
                                                0x1c, 0x18, 0x10, 0x0c, 0x08, 0x04};
        static const unsigned char ones[4] = {0x3c, 0x28, 0x14, 0x00};
        for (u32 i = 0; i < 12; ++i)
            put(out + zeros[i], 0);
        for (u32 i = 0; i < 4; ++i)
            put(out + ones[i], 0x3f800000);
    }
}
constexpr u32 invalid_call = 0x8876086c;
} // namespace
extern "C" {
// The interface id and the vtable of the sprite, defined in renderer_font_storage.cpp.
extern const unsigned char sprite_iid[] __asm__("_dat_006efff4");
extern const unsigned char sprite_vtable[] __asm__("_dat_006df7dc");
void __attribute__((thiscall)) FUN_005fd64e(void*);
void* __attribute__((thiscall)) FUN_005e8b9d(void*, u32);
u32 __attribute__((stdcall)) sprite_release(u32) __asm__("_FUN_005fede9");
u32 __attribute__((stdcall)) sprite_query_interface(u32, u32, u32) __asm__("_FUN_005fd2fb");
u32 __attribute__((stdcall)) sprite_get_device(u32, u32) __asm__("_FUN_005fd341");
u32 __attribute__((stdcall)) sprite_get_transform(u32, u32) __asm__("_FUN_005fd36b");
u32 __attribute__((stdcall)) sprite_set_transform(u32, u32) __asm__("_FUN_005fd9e2");
u32 __attribute__((stdcall)) sprite_set_world_view_rh(u32, u32, u32) __asm__("_FUN_005fdb87");
u32 __attribute__((stdcall)) sprite_set_world_view_lh(u32, u32, u32) __asm__("_FUN_005fdc69");
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
    copy_matrix(output, word(self + 0x70));
    return 0;
}
// Original 005fd9e2..005fdb87 (RET 8). Inside Begin, the vertices queued
// since the last transform (+0xcc up to +0xc4, four 0x18-byte vertices per
// sprite at +0xc0) are first transformed by the old matrix, unless +0x64
// says it is identity or Begin flag 8 is set. Then the matrix is copied to
// *(+0x70) and +0x64 records whether the copy is exactly identity.
u32 sprite_set_transform(u32 self, u32 matrix) {
    if (!matrix)
        return invalid_call;
    if (word(self + 0xac)) {
        if (!word(self + 0x64) && !(byte(self + 0xb0) & 8)) {
            const u32 done = word(self + 0xcc);
            const u32 vertices = done * 96 + word(self + 0xc0);
            const u32 count = (word(self + 0xc4) - done) << 2;
            FUN_005e44ea(pointer(vertices), 0x18, pointer(vertices), 0x18,
                         pointer(word(self + 0x70)), count);
        }
        put(self + 0xcc, word(self + 0xc4));
    }
    copy_matrix(word(self + 0x70), matrix);
    const u32 copy = word(self + 0x70);
    u32 identity = 1;
    for (u32 i = 0; i < 16; ++i)
        if (!quiet_equal(copy + 4 * i, i % 5 ? dat_006bb5fc : dat_006bc4dc)) {
            identity = 0;
            break;
        }
    put(self + 0x68, 0);
    put(self + 0x64, identity);
    return 0;
}
// Original 005fdb87..005fdc69 (RET 12). Refused inside Begin. The view
// direction at +0x94 is the negated third column, then normalized in place.
u32 sprite_set_world_view_rh(u32 self, u32 world, u32 view) {
    if (word(self + 0xac))
        return invalid_call;
    store_world_view(self, world, view);
    const u32 matrix = word(self + 0x80);
    negate_float(matrix + 0x08, self + 0x94);
    negate_float(matrix + 0x18, self + 0x98);
    negate_float(matrix + 0x28, self + 0x9c);
    float* direction = static_cast<float*>(pointer(self + 0x94));
    FUN_005e40eb(direction, direction);
    put(self + 0x68, 0);
    return 0;
}
// Original 005fdc69..005fdd45 (RET 12): as RH, with the column copied as
// plain dwords.
u32 sprite_set_world_view_lh(u32 self, u32 world, u32 view) {
    if (word(self + 0xac))
        return invalid_call;
    store_world_view(self, world, view);
    const u32 matrix = word(self + 0x80);
    put(self + 0x94, word(matrix + 0x08));
    put(self + 0x98, word(matrix + 0x18));
    put(self + 0x9c, word(matrix + 0x28));
    float* direction = static_cast<float*>(pointer(self + 0x94));
    FUN_005e40eb(direction, direction);
    put(self + 0x68, 0);
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
// Original 005fede9..005fee06 (RET 4): the new count, or 0 after the
// scalar deleting destructor ran on the last reference.
u32 sprite_release(u32 self) {
    put(self + 4, word(self + 4) - 1);
    const u32 count = word(self + 4);
    if (count)
        return count;
    FUN_005e8b9d(pointer(self), 1);
    return 0;
}
// Original 005e8b9d..005e8bbe (thiscall, RET 4): scalar deleting destructor.
extern "C" void* __attribute__((thiscall)) FUN_005e8b9d(void* object, u32 flags) {
    FUN_005fd64e(object);
    if (flags & 1)
        fun_005cc620(object);
    return object;
}
// Original 005fd64e..005fd762 (thiscall, RET): destructor body. Installs the
// vtable, releases and clears the twelve COM fields in the original order,
// frees the aligned matrix block at +0x70 (its offset byte sits just below
// the matrix) and the four arrays +0xb4..+0xc0. Every caller ignores EAX.
extern "C" void __attribute__((thiscall)) FUN_005fd64e(void* object) {
    const u32 self = address(object);
    put(self, address(sprite_vtable));
    static const unsigned char fields[12] = {0x08, 0x0c, 0x10, 0x14, 0x18, 0x3c,
                                             0x28, 0x24, 0x30, 0x2c, 0x38, 0x34};
    for (u32 i = 0; i < 12; ++i) {
        const u32 field = self + fields[i];
        if (const u32 com = word(field)) {
            release(com);
            put(field, 0);
        }
    }
    if (const u32 matrix = word(self + 0x70))
        fun_005cc620(pointer(matrix - byte(matrix - 1)));
    for (u32 offset = 0xb4; offset <= 0xc0; offset += 4)
        fun_005cc620(pointer(word(self + offset)));
}
