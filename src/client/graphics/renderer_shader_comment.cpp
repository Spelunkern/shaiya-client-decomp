#include "runtime/allocations.h"
// The shader compiler's comment block builder: a tagged list of byte blobs
// serialized behind a D3DSIO_COMMENT token. Builder: +0x00 tag, +0x04 total
// bytes, +0x08 first entry, +0x0c address of the last entry's link. Entry
// (0x14 bytes): +0x00 data, +0x04 size, +0x08 flags (1 caller-owned, 2 shared,
// 4 unaligned, 8 free caller data when shared), +0x0c offset, +0x10 next.
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
void* pointer(u32 p) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(p));
}
// REP MOVSD / REP MOVSB of `bytes` bytes.
void copy_bytes(u32 to, u32 from, u32 bytes) {
    u32 dwords = bytes >> 2, rest = bytes & 3;
    __asm__ volatile("rep movsl\n\tmovl %3,%%ecx\n\trep movsb"
                     : "+D"(to), "+S"(from), "+c"(dwords)
                     : "r"(rest)
                     : "memory");
}
// REP STOSD / REP STOSB of `bytes` bytes of 0xab.
void fill_ab(u32 to, u32 bytes) {
    u32 dwords = bytes >> 2, rest = bytes & 3;
    __asm__ volatile("rep stosl\n\tmovl %2,%%ecx\n\trep stosb"
                     : "+D"(to), "+c"(dwords)
                     : "r"(rest), "a"(0xababababu)
                     : "memory");
}
// REPE CMPSB over `bytes` bytes: equal when every byte matches (or none).
bool equal_bytes(u32 left, u32 right, u32 bytes) {
    for (u32 i = 0; i < bytes; ++i)
        if (byte(left + i) != byte(right + i))
            return false;
    return true;
}
constexpr u32 e_fail = 0x80004005, e_outofmemory = 0x8007000e;
} // namespace
extern "C" {
void* __attribute__((thiscall)) FUN_006532a8(void*, u32);
void __attribute__((thiscall)) FUN_006532c5(void*);
u32 __attribute__((thiscall)) FUN_006532fd(void*, u32, u32, u32, u32);
u32 __attribute__((thiscall)) FUN_00653436(void*);
u32 __attribute__((thiscall)) FUN_00653442(void*, u32, u32);
u32 __attribute__((stdcall)) shader_value_extent(u32, u32, u32) __asm__("_FUN_00653545");
}
// Original 006532a8..006532c5 (thiscall, RET 4): empty builder with `tag`.
extern "C" void* __attribute__((thiscall)) FUN_006532a8(void* object, u32 tag) {
    const u32 self = address(object);
    put(self + 4, 0);
    put(self, tag);
    put(self + 8, 0);
    put(self + 0x0c, self + 8);
    return object;
}
// Original 006532c5..006532fd (thiscall, RET): frees every entry; the data
// too unless it is caller-owned (flag 1) without flag 8. Callers ignore EAX.
extern "C" void __attribute__((thiscall)) FUN_006532c5(void* object) {
    const u32 self = address(object);
    while (const u32 entry = word(self + 8)) {
        put(self + 8, word(entry + 0x10));
        const u32 flags = word(entry + 8);
        if ((flags & 8) || !(flags & 1))
            fun_005cc620(pointer(word(entry)));
        fun_005cc620(pointer(entry));
    }
}
// Original 006532fd..00653436 (thiscall, RET 16): appends `size` bytes
// (strlen + 1 for size -1) and stores their offset through `offset_out`.
// Data the builder copies is shared by default: an equal shared entry is
// reused (and caller data with flags 1|8 freed). Entries are 4-byte aligned
// unless flag 4 is set. E_FAIL for data NULL with a size, E_OUTOFMEMORY when
// an allocation fails.
extern "C" u32 __attribute__((thiscall)) FUN_006532fd(void* object, u32 data, u32 size, u32 flags,
                                                      u32 offset_out) {
    const u32 self = address(object);
    if (size && !data)
        return e_fail;
    if (size == 0xffffffffu) {
        u32 end = data;
        while (byte(end))
            ++end;
        size = end - data + 1;
    }
    if (!(flags & 1))
        flags |= 2;
    if (flags & 2)
        for (u32 entry = word(self + 8); entry; entry = word(entry + 0x10))
            if ((word(entry + 8) & 2) && size == word(entry + 4) &&
                equal_bytes(data, word(entry), size)) {
                if (offset_out)
                    put(offset_out, word(entry + 0x0c));
                if ((flags & 1) && (flags & 8))
                    fun_005cc620(pointer(data));
                return 0;
            }
    const u32 entry = address(fun_005cc96d(0x14));
    if (!entry)
        return e_outofmemory;
    if (!(flags & 1)) {
        const u32 copy = address(fun_005cc96d(size));
        put(entry, copy);
        if (!copy) {
            fun_005cc620(pointer(entry));
            return e_outofmemory;
        }
        copy_bytes(copy, data, size);
    } else {
        put(entry, data);
    }
    if (!(flags & 4))
        put(self + 4, (word(self + 4) + 3) & ~3u);
    const u32 offset = word(self + 4);
    put(entry + 8, flags);
    put(entry + 0x10, 0);
    put(entry + 0x0c, offset);
    put(entry + 4, size);
    put(self + 4, word(self + 4) + size);
    put(word(self + 0x0c), entry);
    put(self + 0x0c, entry + 0x10);
    if (offset_out)
        put(offset_out, word(entry + 0x0c));
    return 0;
}
// Original 00653436..00653442 (thiscall, RET): serialized size in dwords,
// the comment token and tag included.
extern "C" u32 __attribute__((thiscall)) FUN_00653436(void* object) {
    return ((word(address(object) + 4) + 3) >> 2) + 2;
}
// Original 00653442..0065352d (thiscall, RET 8): writes the comment token,
// the tag and the entries into `dwords` dwords at `out` (the needed size
// for -1), padding with 0xab. E_FAIL when it does not fit or exceeds 0x8000.
extern "C" u32 __attribute__((thiscall)) FUN_00653442(void* object, u32 out, u32 dwords) {
    const u32 self = address(object);
    const u32 needed = ((word(self + 4) + 3) >> 2) + 2;
    if (dwords == 0xffffffffu)
        dwords = needed;
    else if (dwords < needed)
        return e_fail;
    if (dwords > 0x8000)
        return e_fail;
    put(out, (((dwords - 1) & 0x7fff) << 16) | 0xfffe);
    put(out + 4, word(self));
    u32 at = out + 8, offset = 0;
    for (u32 entry = word(self + 8); entry; entry = word(entry + 0x10)) {
        if (!(word(entry + 8) & 4)) {
            const u32 pad = ((offset + 3) & ~3u) - offset;
            fill_ab(at, pad);
            at += pad;
            offset += pad;
        }
        copy_bytes(at, word(entry), word(entry + 4));
        const u32 size = word(entry + 4);
        at += size;
        offset += size;
    }
    fill_ab(at, dwords * 4 - 8 - offset);
    return 0;
}
// Original 00653545..00653608 (RET 12): the extent of a value node: kind 9
// by class (scalar and 0 class: 1 x +0x1c, 2: +0x1c x +0x18, 3: 1 x 1), kind
// 8 the element's extents with the first one times +0x14, kind 1 the sum of the members' first
// extents and the largest second one. E_FAIL for any other kind or class.
u32 shader_value_extent(u32 node, u32 first, u32 second) {
    u32 scratch = node;
    const u32 out = second ? second : address(&scratch);
    switch (word(node + 4)) {
    case 9: {
        const u32 kind = word(node + 0x10);
        if (kind == 1 || kind == 0) {
            put(first, 1);
            put(out, word(node + 0x1c));
        } else if (kind == 2) {
            put(first, word(node + 0x1c));
            put(out, word(node + 0x18));
        } else if (kind == 3) {
            put(first, 1);
            put(out, 1);
        } else {
            return e_fail;
        }
        return 0;
    }
    case 8: {
        const u32 result = shader_value_extent(word(node + 0x10), first, out);
        if (static_cast<int32_t>(result) < 0)
            return result;
        put(first, word(node + 0x14) * word(first));
        return 0;
    }
    case 1: {
        put(first, 0);
        put(out, 0);
        // The original reuses its own argument slots for the member extents.
        u32 member_first = second, member_second = first;
        for (u32 member = node; member; member = word(member + 0x0c)) {
            const u32 result = shader_value_extent(word(word(word(member + 8) + 0x18) + 0x20),
                                                   address(&member_first), address(&member_second));
            if (static_cast<int32_t>(result) < 0)
                return result;
            put(first, word(first) + member_first);
            if (member_second > word(out))
                put(out, member_second);
        }
        return 0;
    }
    default:
        return e_fail;
    }
}
