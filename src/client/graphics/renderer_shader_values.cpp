#include <stdint.h>
// Value nodes of the shader compiler. Every node starts with a vtable whose
// slots are Equals(other) (thiscall, RET 4), Clone() (thiscall) and a pure
// call, then +0x04 the kind, +0x08 and +0x0c two words the base clears (the
// list kind uses them as value and next). Only the offsets the original
// reads are recovered. The vtables in renderer_shader_storage.cpp and the
// asm still in other units reference the undecorated original names.
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
// REP MOVSD of `dwords` dwords.
void copy_dwords(u32 to, u32 from, u32 dwords) {
    __asm__ volatile("rep movsl" : "+D"(to), "+S"(from), "+c"(dwords)::"memory");
}
// REP MOVSD / REP MOVSB of `bytes` bytes.
void copy_bytes(u32 to, u32 from, u32 bytes) {
    u32 dwords = bytes >> 2, rest = bytes & 3;
    __asm__ volatile("rep movsl\n\tmovl %3,%%ecx\n\trep movsb"
                     : "+D"(to), "+S"(from), "+c"(dwords)
                     : "r"(rest)
                     : "memory");
}
void zero_dwords(u32 to, u32 dwords) {
    for (u32 i = 0; i < dwords; ++i)
        put(to + 4 * i, 0);
}
// REPE CMPSB over `bytes` bytes: equal when every byte matches (or none).
bool equal_bytes(u32 left, u32 right, u32 bytes) {
    for (u32 i = 0; i < bytes; ++i)
        if (byte(left + i) != byte(right + i))
            return false;
    return true;
}
// Byte-wise compare of two NUL-terminated strings for equality.
bool equal_strings(u32 left, u32 right) {
    for (;; ++left, ++right) {
        const u32 c = byte(left);
        if (c != byte(right))
            return false;
        if (!c)
            return true;
    }
}
// FLD m64 (right) / FLD m64 (left) / FUCOMPP / FNSTSW AX, then TEST AH,0x44 /
// JP: equal only when C3 alone is set. The compare is quiet.
bool equal_doubles(u32 left, u32 right) {
    u32 status;
    __asm__ volatile("fldl (%1)\n\tfldl (%2)\n\tfucompp\n\tfnstsw %%ax"
                     : "=a"(status)
                     : "r"(right), "r"(left)
                     : "cc", "memory");
    return __builtin_parity((status >> 8) & 0x44);
}
using Equals = u32(__attribute__((thiscall))*)(u32, u32);
using Clone = u32(__attribute__((thiscall))*)(u32);
u32 call_equals(u32 node, u32 other) {
    return reinterpret_cast<Equals>(word(word(node)))(node, other);
}
u32 call_clone(u32 node) {
    return reinterpret_cast<Clone>(word(word(node) + 4))(node);
}
} // namespace
extern "C" {
extern const unsigned char vt_value[] __asm__("_dat_006f0270");
extern const unsigned char vt_list[] __asm__("_dat_006f027c");
extern const unsigned char vt_constant[] __asm__("_dat_006f0294");
extern const unsigned char vt_kind9[] __asm__("_dat_006f02dc");
extern const unsigned char vt_kind11[] __asm__("_dat_006f02f4");
extern const unsigned char vt_kind14[] __asm__("_dat_006f0318");
extern const unsigned char vt_kind18[] __asm__("_dat_006f04ac");
extern const unsigned char vt_kind19[] __asm__("_dat_006f04b8");
// The shared shader arena (renderer_shader_arena.cpp).
extern uint32_t dat_02251784;
void* __attribute__((stdcall)) FUN_00604310(u32) __asm__("_FUN_00604310");
void* __attribute__((thiscall)) FUN_00651c2f(void*, u32, u32);
u32 shader_value_init(u32 self, u32 kind);
u32 shader_value_same_kind(u32 self, u32 other);
u32 shader_constant_equal(u32 left, u32 right);
u32 __attribute__((thiscall)) FUN_00651d19(u32, u32);
u32 __attribute__((thiscall)) FUN_00651d38(u32, u32);
void FUN_00651ef1();
u32 __attribute__((stdcall)) shader_values_equal(u32, u32) __asm__("_FUN_00651d53");
u32 __attribute__((stdcall)) shader_value_clone(u32) __asm__("_FUN_00651d74");
u32 __attribute__((stdcall)) shader_list_append(u32, u32) __asm__("_FUN_00651d8d");
u32 __attribute__((thiscall)) FUN_00651db6(u32);
u32 __attribute__((thiscall)) FUN_00651dcc(u32, u32, u32, u32);
u32 __attribute__((thiscall)) FUN_00651df8(u32, u32);
u32 __attribute__((thiscall)) FUN_00651fc1(u32);
u32 __attribute__((thiscall)) FUN_00651fd3(u32, u32);
u32 __attribute__((thiscall)) FUN_00651ffc(u32, u32);
u32 __attribute__((thiscall)) FUN_0065235e(u32);
u32 __attribute__((thiscall)) FUN_00652385(u32, u32, u32, u32, u32, u32);
u32 __attribute__((thiscall)) FUN_006523bd(u32, u32);
u32 __attribute__((thiscall)) FUN_006524fa(u32);
u32 __attribute__((thiscall)) FUN_00652532(u32, u32);
u32 __attribute__((thiscall)) FUN_0065277d(u32);
u32 __attribute__((thiscall)) FUN_006527fc(u32, u32, u32, u32);
u32 __attribute__((thiscall)) FUN_00652866(u32, u32, u32, u32, u32);
u32 __attribute__((thiscall)) FUN_006528a1(u32, u32);
u32 __attribute__((thiscall)) FUN_00652a9d(u32);
u32 __attribute__((thiscall)) FUN_00652abd(u32);
u32 __attribute__((thiscall)) FUN_00652b91(u32);
u32 __attribute__((thiscall)) FUN_00652db2(u32);
u32 __attribute__((thiscall)) FUN_00652ed0(u32);
u32 __attribute__((thiscall)) FUN_00653142(u32);
u32 __attribute__((thiscall)) FUN_00653b68(u32);
u32 __attribute__((thiscall)) FUN_00653ba6(u32, u32, u32, u32, u32);
u32 __attribute__((thiscall)) FUN_00653bf8(u32);
u32 __attribute__((thiscall)) FUN_00653c2d(u32, u32, u32, u32, u32, u32);
u32 __attribute__((thiscall)) FUN_00653c75(u32);
u32 __attribute__((thiscall)) FUN_00653d39(u32);
}
namespace {
// `bytes` from the shared arena, as the nodes' operator new.
u32 allocate(u32 bytes) {
    return address(FUN_00604310(bytes));
}
} // namespace
// Original 00651d19..00651d38 (thiscall, RET 4): base constructor. 33 asm
// call sites keep the object in EDX across the call, so the entry preserves
// EDX; ECX ends as the kind, as in the original.
extern "C" u32 shader_value_init(u32 self, u32 kind) {
    put(self + 8, 0);
    put(self + 0x0c, 0);
    put(self, address(vt_value));
    put(self + 4, kind);
    return self;
}
extern "C" __attribute__((naked)) u32 __attribute__((thiscall)) FUN_00651d19(u32, u32) {
    __asm__ volatile("pushl %edx\n\tpushl 8(%esp)\n\tpushl %ecx\n\tcalll _shader_value_init\n\t"
                     "addl $8,%esp\n\tpopl %edx\n\tmovl 4(%esp),%ecx\n\tretl $4");
}
// Original 00651d38..00651d53 (thiscall, RET 4): base Equals, true when
// `other` is not NULL and has the same kind. Asm callers keep values in ECX
// and EDX across it, so the entry preserves both.
extern "C" u32 shader_value_same_kind(u32 self, u32 other) {
    return other && word(other + 4) == word(self + 4);
}
extern "C" __attribute__((naked)) u32 __attribute__((thiscall)) FUN_00651d38(u32, u32) {
    __asm__ volatile("pushl %ecx\n\tpushl %edx\n\tpushl 12(%esp)\n\tpushl %ecx\n\t"
                     "calll _shader_value_same_kind\n\taddl $8,%esp\n\tpopl %edx\n\tpopl %ecx\n\t"
                     "retl $4");
}
// Original 00651d53..00651d74 (RET 8): `a`'s Equals on `b`, or whether `b` is
// NULL too.
u32 shader_values_equal(u32 a, u32 b) {
    return a ? call_equals(a, b) : b == 0;
}
// Original 00651d74..00651d8d (RET 4): `a`'s Clone, or NULL.
u32 shader_value_clone(u32 a) {
    return a ? call_clone(a) : 0;
}
// Original 00651d8d..00651db6 (RET 8): appends `item` through the +0x0c
// links and returns the list head (`item` for an empty list).
u32 shader_list_append(u32 list, u32 item) {
    if (!list)
        return item;
    u32 link = list + 0x0c;
    while (word(link))
        link = word(link) + 0x0c;
    put(link, item);
    return list;
}
// Original 00651db6..00651dcc (thiscall): empty list node (kind 1).
extern "C" u32 __attribute__((thiscall)) FUN_00651db6(u32 self) {
    shader_value_init(self, 1);
    put(self + 0x10, 0);
    put(self, address(vt_list));
    return self;
}
// Original 00651dcc..00651df8 (thiscall, RET 12): list node with its value,
// next node and tag.
extern "C" u32 __attribute__((thiscall)) FUN_00651dcc(u32 self, u32 value, u32 next, u32 tag) {
    shader_value_init(self, 1);
    put(self + 8, value);
    put(self + 0x0c, next);
    put(self + 0x10, tag);
    put(self, address(vt_list));
    return self;
}
// Original 00651df8..00651e53 (thiscall, RET 4): list Equals. Walks both
// lists while this one has list nodes, comparing their values; a non-list
// node is compared whole. As in the original, the other list may be longer.
extern "C" u32 __attribute__((thiscall)) FUN_00651df8(u32 self, u32 other) {
    if (!shader_value_same_kind(self, other))
        return 0;
    for (u32 node = self; node;) {
        if (word(node + 4) != 1)
            return shader_values_equal(node, other) ? 1 : 0;
        if (!other || word(other + 4) != 1)
            return 0;
        if (!shader_values_equal(word(node + 8), word(other + 8)))
            return 0;
        node = word(node + 0x0c);
        other = word(other + 0x0c);
    }
    return 1;
}
// Original 00651ef1..00651fc1 (ECX = left, EAX = right; RET): whether two
// typed constants {type, ?, payload at +8} are equal. Types 0 and 2..4
// compare the payload dword, 1 an inline string, 5..8 a double (quiet
// compare), 9 and 10 a string through the payload pointer; negative types and
// types above 10 are always equal.
extern "C" u32 shader_constant_equal(u32 left, u32 right) {
    const int32_t type = static_cast<int32_t>(word(left));
    if (static_cast<u32>(type) != word(right))
        return 0;
    if (type > 8) {
        if (type == 9 || type == 10)
            return equal_strings(word(left + 8), word(right + 8));
        return 1;
    }
    if (type >= 5)
        return equal_doubles(left + 8, right + 8);
    if (type == 1)
        return equal_strings(left + 8, right + 8);
    if (type < 0)
        return 1;
    return word(left + 8) == word(right + 8);
}
extern "C" __attribute__((naked)) void FUN_00651ef1() {
    __asm__ volatile("pushl %eax\n\tpushl %ecx\n\tcalll _shader_constant_equal\n\t"
                     "addl $8,%esp\n\tretl");
}
// Original 00651fc1..00651fd3 (thiscall): empty constant node (kind 3).
extern "C" u32 __attribute__((thiscall)) FUN_00651fc1(u32 self) {
    shader_value_init(self, 3);
    put(self, address(vt_constant));
    return self;
}
// Original 00651fd3..00651ffc (thiscall, RET 4): constant node holding a
// copy of the eight-dword constant at `value`.
extern "C" u32 __attribute__((thiscall)) FUN_00651fd3(u32 self, u32 value) {
    shader_value_init(self, 3);
    put(self, address(vt_constant));
    copy_dwords(self + 0x10, value, 8);
    return self;
}
// Original 00651ffc..00652021 (thiscall, RET 4): constant Equals.
extern "C" u32 __attribute__((thiscall)) FUN_00651ffc(u32 self, u32 other) {
    if (!shader_value_same_kind(self, other))
        return 0;
    return shader_constant_equal(self + 0x10, other + 0x10) ? 1 : 0;
}
// Original 0065235e..00652385 (thiscall): empty kind-9 node; +0x14 starts as 9.
extern "C" u32 __attribute__((thiscall)) FUN_0065235e(u32 self) {
    shader_value_init(self, 9);
    put(self + 0x10, 0);
    put(self + 0x18, 0);
    put(self + 0x1c, 0);
    put(self + 0x20, 0);
    put(self, address(vt_kind9));
    put(self + 0x14, 9);
    return self;
}
// Original 00652385..006523bd (thiscall, RET 20): kind-9 node with its five fields.
extern "C" u32 __attribute__((thiscall)) FUN_00652385(u32 self, u32 a, u32 b, u32 c, u32 d,
                                                      u32 e) {
    shader_value_init(self, 9);
    put(self + 0x10, a);
    put(self + 0x14, b);
    put(self + 0x18, c);
    put(self + 0x1c, d);
    put(self + 0x20, e);
    put(self, address(vt_kind9));
    return self;
}
// Original 006523bd..00652400 (thiscall, RET 4): kind-9 Equals, field by field.
extern "C" u32 __attribute__((thiscall)) FUN_006523bd(u32 self, u32 other) {
    if (!shader_value_same_kind(self, other))
        return 0;
    for (u32 offset = 0x10; offset <= 0x20; offset += 4)
        if (word(self + offset) != word(other + offset))
            return 0;
    return 1;
}
// Original 006524fa..00652532 (thiscall): empty kind-11 node.
extern "C" u32 __attribute__((thiscall)) FUN_006524fa(u32 self) {
    shader_value_init(self, 0x0b);
    zero_dwords(self + 0x10, 12);
    put(self, address(vt_kind11));
    return self;
}
// Original 00652532..006525ea (thiscall, RET 4): kind-11 Equals: the plain
// fields +0x10..+0x1c and the count +0x34, the five child values +0x20..+0x30,
// then the count * 4 bytes at +0x38.
extern "C" u32 __attribute__((thiscall)) FUN_00652532(u32 self, u32 other) {
    if (!shader_value_same_kind(self, other))
        return 0;
    static const unsigned char plain[5] = {0x10, 0x14, 0x18, 0x1c, 0x34};
    for (u32 i = 0; i < 5; ++i)
        if (word(self + plain[i]) != word(other + plain[i]))
            return 0;
    for (u32 offset = 0x20; offset <= 0x30; offset += 4)
        if (!shader_values_equal(word(self + offset), word(other + offset)))
            return 0;
    if (const u32 count = word(self + 0x34))
        if (!equal_bytes(word(self + 0x38), word(other + 0x38), count << 2))
            return 0;
    return 1;
}
// Original 0065277d..006527b0 (thiscall): empty kind-14 node; +0x10 starts as 3.
extern "C" u32 __attribute__((thiscall)) FUN_0065277d(u32 self) {
    shader_value_init(self, 0x0e);
    put(self, address(vt_kind14));
    zero_dwords(self + 0x10, 4);
    zero_dwords(self + 0x20, 8);
    put(self + 0x10, 3);
    return self;
}
// Original 006527fc..0065282f (thiscall, RET 12): kind-14 node; +0x14 and
// +0x1c are left as they were.
extern "C" u32 __attribute__((thiscall)) FUN_006527fc(u32 self, u32 a, u32 b, u32 value) {
    shader_value_init(self, 0x0e);
    put(self + 0x10, a);
    put(self + 0x18, b);
    put(self, address(vt_kind14));
    copy_dwords(self + 0x20, value, 8);
    return self;
}
// Original 00652866..0065289f (thiscall, RET 16): kind-14 node; +0x14 is
// left as it was.
extern "C" u32 __attribute__((thiscall)) FUN_00652866(u32 self, u32 a, u32 b, u32 c, u32 value) {
    shader_value_init(self, 0x0e);
    put(self + 0x10, a);
    put(self + 0x18, b);
    put(self + 0x1c, c);
    put(self, address(vt_kind14));
    copy_dwords(self + 0x20, value, 8);
    return self;
}
// Original 006528a1..006528f0 (thiscall, RET 4): kind-14 Equals: the four
// dwords at +0x10, or, for form 4, the same form and equal child values at
// +0x18.
extern "C" u32 __attribute__((thiscall)) FUN_006528a1(u32 self, u32 other) {
    if (!shader_value_same_kind(self, other))
        return 0;
    const u32 form = word(self + 0x10);
    if (form != word(other + 0x10))
        return 0;
    bool same = true;
    for (u32 offset = 0x10; offset <= 0x1c; offset += 4)
        if (word(self + offset) != word(other + offset)) {
            same = false;
            break;
        }
    if (same)
        return 1;
    if (form == 4 && shader_values_equal(word(self + 0x18), word(other + 0x18)))
        return 1;
    return 0;
}
// Original 00652a9d..00652abd (thiscall): base Clone, a bare node of the same kind.
extern "C" u32 __attribute__((thiscall)) FUN_00652a9d(u32 self) {
    const u32 copy = allocate(0x10);
    return copy ? shader_value_init(copy, word(self + 4)) : 0;
}
// Original 00652abd..00652b36 (thiscall): list Clone. Copies list nodes with
// their tags and cloned values; the first non-list node is cloned whole.
// NULL when any allocation or child clone fails.
extern "C" u32 __attribute__((thiscall)) FUN_00652abd(u32 self) {
    u32 result = 0;
    u32 slot = address(&result);
    for (u32 node = self; node; node = word(node + 0x0c)) {
        if (word(node + 4) != 1) {
            put(slot, call_clone(node));
            break;
        }
        const u32 memory = allocate(0x14);
        const u32 copy = memory ? FUN_00651db6(memory) : 0;
        put(slot, copy);
        if (!copy)
            break;
        put(copy + 0x10, word(node + 0x10));
        if (word(node + 8)) {
            put(word(slot) + 8, call_clone(word(node + 8)));
            if (!word(word(slot) + 8)) {
                put(slot, 0);
                break;
            }
        }
        slot = word(slot) + 0x0c;
    }
    return result;
}
// Original 00652b91..00652bc1 (thiscall): constant Clone.
extern "C" u32 __attribute__((thiscall)) FUN_00652b91(u32 self) {
    const u32 memory = allocate(0x30);
    const u32 copy = memory ? FUN_00651fc1(memory) : 0;
    if (!copy)
        return 0;
    copy_dwords(copy + 0x10, self + 0x10, 8);
    return copy;
}
// Original 00652db2..00652df3 (thiscall): kind-9 Clone.
extern "C" u32 __attribute__((thiscall)) FUN_00652db2(u32 self) {
    const u32 memory = allocate(0x24);
    const u32 copy = memory ? FUN_0065235e(memory) : 0;
    if (!copy)
        return 0;
    for (u32 offset = 0x10; offset <= 0x20; offset += 4)
        put(copy + offset, word(self + offset));
    return copy;
}
// Original 00652ed0..00653014 (thiscall): kind-11 Clone: plain fields, the
// five child values (a failed child clone is stored, then NULL is returned)
// and, when the count is not zero, arena copies of the count * 4 bytes at
// +0x38 and of the +0x14 * 4 bytes at +0x3c.
extern "C" u32 __attribute__((thiscall)) FUN_00652ed0(u32 self) {
    const u32 memory = allocate(0x40);
    const u32 copy = memory ? FUN_006524fa(memory) : 0;
    if (!copy)
        return 0;
    static const unsigned char plain[5] = {0x10, 0x14, 0x18, 0x1c, 0x34};
    for (u32 i = 0; i < 5; ++i)
        put(copy + plain[i], word(self + plain[i]));
    for (u32 offset = 0x20; offset <= 0x30; offset += 4)
        if (word(self + offset)) {
            const u32 child = call_clone(word(self + offset));
            put(copy + offset, child);
            if (!child)
                return 0;
        }
    if (const u32 count = word(self + 0x34)) {
        void* const arena = pointer(dat_02251784);
        const u32 first = address(FUN_00651c2f(arena, count << 2, 0x10));
        put(copy + 0x38, first);
        if (!first)
            return 0;
        const u32 second = address(FUN_00651c2f(pointer(dat_02251784), word(self + 0x14) << 2, 0x10));
        put(copy + 0x3c, second);
        if (!second)
            return 0;
        copy_bytes(word(copy + 0x38), word(self + 0x38), word(self + 0x34) << 2);
        copy_bytes(word(copy + 0x3c), word(self + 0x3c), word(self + 0x14) << 2);
    }
    return copy;
}
// Original 00653142..006531ab (thiscall): kind-14 Clone. For form 4 the
// child at +0x18 is cloned, but the original stores the clone back into the
// SOURCE node's +0x18, leaving the copy sharing the old child; this lift
// keeps that behavior. NULL when the child clone fails.
extern "C" u32 __attribute__((thiscall)) FUN_00653142(u32 self) {
    const u32 memory = allocate(0x40);
    const u32 copy = memory ? FUN_0065277d(memory) : 0;
    if (!copy)
        return 0;
    copy_dwords(copy + 0x10, self + 0x10, 4);
    copy_dwords(copy + 0x20, self + 0x20, 8);
    if (word(self + 0x10) == 4 && word(self + 0x18)) {
        const u32 child = shader_value_clone(word(self + 0x18));
        put(self + 0x18, child);
        if (!child)
            return 0;
    }
    return copy;
}
// Original 00653b68..00653ba6 (thiscall): empty kind-18 node.
extern "C" u32 __attribute__((thiscall)) FUN_00653b68(u32 self) {
    shader_value_init(self, 0x12);
    static const unsigned char zeroed[7] = {0x30, 0x34, 0x38, 0x3c, 0x40, 0x54, 0x58};
    for (u32 i = 0; i < 7; ++i)
        put(self + zeroed[i], 0);
    put(self, address(vt_kind18));
    zero_dwords(self + 0x10, 8);
    zero_dwords(self + 0x44, 4);
    return self;
}
// Original 00653ba6..00653bf8 (thiscall, RET 16): kind-18 node with the
// eight dwords at `value` and the fields +0x30..+0x38.
extern "C" u32 __attribute__((thiscall)) FUN_00653ba6(u32 self, u32 value, u32 a, u32 b, u32 c) {
    shader_value_init(self, 0x12);
    put(self, address(vt_kind18));
    copy_dwords(self + 0x10, value, 8);
    put(self + 0x30, a);
    put(self + 0x34, b);
    put(self + 0x38, c);
    static const unsigned char zeroed[4] = {0x3c, 0x40, 0x54, 0x58};
    for (u32 i = 0; i < 4; ++i)
        put(self + zeroed[i], 0);
    zero_dwords(self + 0x44, 4);
    return self;
}
// Original 00653bf8..00653c2d (thiscall): empty kind-19 node.
extern "C" u32 __attribute__((thiscall)) FUN_00653bf8(u32 self) {
    shader_value_init(self, 0x13);
    static const unsigned char zeroed[5] = {0x10, 0x14, 0x18, 0x1c, 0x28};
    for (u32 i = 0; i < 5; ++i)
        put(self + zeroed[i], 0);
    put(self, address(vt_kind19));
    put(self + 0x20, 0xf0000);
    put(self + 0x24, 0xe40000);
    return self;
}
// Original 00653c2d..00653c75 (thiscall, RET 20): kind-19 node with its fields.
extern "C" u32 __attribute__((thiscall)) FUN_00653c2d(u32 self, u32 a, u32 b, u32 c, u32 d,
                                                      u32 child) {
    shader_value_init(self, 0x13);
    put(self + 0x10, a);
    put(self + 0x14, b);
    put(self + 0x18, c);
    put(self + 0x1c, d);
    put(self + 0x28, child);
    put(self, address(vt_kind19));
    put(self + 0x20, 0xf0000);
    put(self + 0x24, 0xe40000);
    return self;
}
// Original 00653c75..00653d39 (thiscall): kind-18 Clone: plain fields, then
// the children +0x3c, +0x40 and +0x44..+0x50; NULL when one fails (the
// failed result is stored first).
extern "C" u32 __attribute__((thiscall)) FUN_00653c75(u32 self) {
    const u32 memory = allocate(0x60);
    const u32 copy = memory ? FUN_00653b68(memory) : 0;
    if (!copy)
        return 0;
    copy_dwords(copy + 0x10, self + 0x10, 8);
    static const unsigned char plain[5] = {0x30, 0x34, 0x38, 0x54, 0x58};
    for (u32 i = 0; i < 5; ++i)
        put(copy + plain[i], word(self + plain[i]));
    static const unsigned char children[6] = {0x3c, 0x40, 0x44, 0x48, 0x4c, 0x50};
    for (u32 i = 0; i < 6; ++i)
        if (word(self + children[i])) {
            const u32 child = call_clone(word(self + children[i]));
            put(copy + children[i], child);
            if (!child)
                return 0;
        }
    return copy;
}
// Original 00653d39..00653d9d (thiscall): kind-19 Clone.
extern "C" u32 __attribute__((thiscall)) FUN_00653d39(u32 self) {
    const u32 memory = allocate(0x2c);
    const u32 copy = memory ? FUN_00653bf8(memory) : 0;
    if (!copy)
        return 0;
    for (u32 offset = 0x10; offset <= 0x24; offset += 4)
        put(copy + offset, word(self + offset));
    if (word(self + 0x28)) {
        const u32 child = call_clone(word(self + 0x28));
        put(copy + 0x28, child);
        if (!child)
            return 0;
    }
    return copy;
}
