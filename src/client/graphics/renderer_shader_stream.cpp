#include "runtime/allocations.h"
#include "runtime/shared_ascii_literals.h"
#include "runtime/stdio_format.h"
// The shader assembler's source stream and diagnostics. Stream: +0x00 cursor,
// +0x04 end, +0x14 line map count, +0x18 sorted line keys, +0x1c per-line
// flags, +0x2c and +0x30 owner fields. Message sink: +0x00 newest message
// (each a {next, text} block), +0x04 total text length, +0x08 error count.
// Only the original offsets are recovered.
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
u32 length(u32 text) {
    u32 end = text;
    while (byte(end))
        ++end;
    return end - text;
}
// Replaces the array at `field` (count `count` dwords) with a copy in a
// block of `capacity` dwords. False when the allocation fails.
bool regrow(u32 field, u32 count, u32 capacity) {
    const u32 block = address(fun_005cc96d(capacity << 2));
    if (!block)
        return false;
    copy_bytes(block, word(field), count << 2);
    fun_005cc620(pointer(word(field)));
    put(field, block);
    return true;
}
// One formatted piece of a diagnostic: a negative result consumes the rest.
template <typename... Args> u32 piece(u32 at, u32 left, const char* format, Args... args) {
    const int32_t written = fun_005cd214(static_cast<char*>(pointer(at)), left, format, args...);
    return written < 0 ? left : static_cast<u32>(written);
}
constexpr u32 e_fail = 0x80004005, e_outofmemory = 0x8007000e;
} // namespace
extern "C" {
u32 __attribute__((thiscall)) FUN_006066bd(void*, u32, u32);
u32 __attribute__((thiscall)) FUN_00606843(void*, u32);
u32 __attribute__((thiscall)) FUN_00606929(void*, u32, u32, u32, u32, u32, u32);
u32 __attribute__((thiscall)) FUN_00606993(void*);
u32 __attribute__((thiscall)) FUN_00606dde(void*, u32, u32);
u32 FUN_00606e48(void*, const void*, u32, const char*, ...);
// "error X%u: " and "(%u): " (renderer_shader_storage.cpp).
extern const char dat_006dfe8c[], dat_006dfe98[];
}
// Original 006066bd..006067fe (thiscall, RET 8): finds `key` in the sorted
// line map, inserting it with flags 1 when missing, and stores its index.
// The arrays grow to double (1 when empty) whenever the count is a power of
// two or zero. E_OUTOFMEMORY when a grow fails; the index is then not stored.
// The original always ends with a free(NULL), kept here.
extern "C" u32 __attribute__((thiscall)) FUN_006066bd(void* object, u32 key, u32 index_out) {
    const u32 self = address(object);
    u32 result = 0;
    const u32 count = word(self + 0x14);
    u32 low = 0, high = count, middle = count >> 1;
    bool found = false;
    while (low < high) {
        const u32 value = word(word(self + 0x18) + middle * 4);
        if (key > value) {
            low = middle + 1;
        } else if (key < value) {
            high = middle;
        } else {
            found = true;
            break;
        }
        middle = (high + low) >> 1;
    }
    if (!found) {
        if (count == (count & (0u - count))) {
            const u32 capacity = count ? count * 2 : 1;
            if (!regrow(self + 0x1c, word(self + 0x14), capacity)) {
                fun_005cc620(nullptr);
                return e_outofmemory;
            }
            const u32 again = word(self + 0x14);
            if (!regrow(self + 0x18, again, again ? again * 2 : 1)) {
                fun_005cc620(nullptr);
                return e_outofmemory;
            }
        }
        for (u32 slot = word(self + 0x14); slot > middle; --slot) {
            const u32 keys = word(self + 0x18) + slot * 4;
            put(keys, word(keys - 4));
            const u32 flags = word(self + 0x1c) + slot * 4;
            put(flags, word(flags - 4));
        }
        put(word(self + 0x18) + middle * 4, key);
        put(word(self + 0x1c) + middle * 4, 1);
        put(self + 0x14, word(self + 0x14) + 1);
    }
    if (index_out)
        put(index_out, middle);
    fun_005cc620(nullptr);
    return result;
}
// Original 00606843..0060689d (thiscall, RET 4): prepends a copy of `text`
// to the message list and adds its length to the total.
extern "C" u32 __attribute__((thiscall)) FUN_00606843(void* object, u32 text) {
    const u32 self = address(object);
    const u32 bytes = length(text) + 1;
    const u32 block = address(fun_005cc96d(bytes + 4));
    if (!block)
        return e_outofmemory;
    put(block, word(self));
    put(self + 4, word(self + 4) + bytes - 1);
    put(self, block);
    copy_bytes(block + 4, text, bytes);
    return 0;
}
// Original 00606929..00606991 (thiscall, RET 24): sets the stream over
// `bytes` of `text` (strlen for -1). E_FAIL without both owner fields or for
// NULL text with a length.
extern "C" u32 __attribute__((thiscall)) FUN_00606929(void* object, u32 text, u32 bytes, u32 a,
                                                      u32 b, u32 owner, u32 sink) {
    const u32 self = address(object);
    if (!owner || !sink)
        return e_fail;
    if (bytes == 0xffffffffu)
        bytes = text ? length(text) : 0;
    if (!text && bytes)
        return e_fail;
    put(self, text);
    put(self + 0x18, a);
    put(self + 0x1c, b);
    put(self + 0x2c, owner);
    put(self + 4, text + bytes);
    put(self + 0x30, sink);
    return 0;
}
// Original 00606993..006069ee (thiscall, RET): advances to the end of the
// line, joining backslash-newline and backslash-CR-newline continuations
// (each counted at +0x1c). True when it stops at a newline, false at the end.
extern "C" u32 __attribute__((thiscall)) FUN_00606993(void* object) {
    const u32 self = address(object);
    const u32 end = word(self + 4);
    if (word(self) >= end)
        return 0;
    do {
        u32 at = word(self);
        const u32 c = byte(at);
        if (c == 0x0a)
            return 1;
        if (c == 0x5c && at + 1 < end && byte(at + 1) == 0x0a) {
            at += 2;
            put(self + 0x1c, word(self + 0x1c) + 1);
        } else if (c == 0x5c && at + 2 < end && byte(at + 1) == 0x0d && byte(at + 2) == 0x0a) {
            at += 3;
            put(self + 0x1c, word(self + 0x1c) + 1);
        } else {
            at += 1;
        }
        put(self, at);
    } while (word(self) < word(self + 4));
    return 0;
}
// Original 00606dde..00606e46 (thiscall, RET 8): sets the flags of line
// `key`: 0xff keeps bit 5 and sets bit 0, 0x10 sets bit 4, anything else
// replaces the low four bits.
extern "C" u32 __attribute__((thiscall)) FUN_00606dde(void* object, u32 key, u32 flags) {
    const u32 self = address(object);
    u32 index = key;
    const u32 result = FUN_006066bd(object, key, address(&index));
    if (static_cast<int32_t>(result) < 0)
        return result;
    const u32 slot = word(self + 0x1c) + index * 4;
    if (flags == 0xff) {
        put(slot, word(slot) & 0x20);
        put(word(self + 0x1c) + index * 4, word(word(self + 0x1c) + index * 4) | 1);
    } else if (flags == 0x10) {
        put(slot, word(slot) | 0x10);
    } else {
        put(slot, word(slot) & 0xfffffff0u);
        put(word(self + 0x1c) + index * 4, word(word(self + 0x1c) + index * 4) | (flags & 0xf));
    }
    return result;
}
// Original 00606e48..00606f0f (cdecl, variadic): formats "file(line): error
// Xcode: message\n" into a 4 KiB frame buffer (file and line from
// `location` +0x10/+0x14, line 0 printed as 1; the code only when not
// zero), counts the error at +0x08 and appends the text to the sink.
extern "C" u32 FUN_00606e48(void* sink, const void* location, u32 code, const char* format, ...) {
    char buffer[0x1000];
    u32 at = address(buffer), left = 0xffe;
    if (location) {
        const u32 where = address(location);
        if (const u32 file = word(where + 0x10)) {
            const u32 n = piece(at, left, dat_006be170, file);
            at += n;
            left -= n;
        }
        const u32 line = word(where + 0x14);
        const u32 n = piece(at, left, dat_006dfe98, line ? line : 1);
        at += n;
        left -= n;
    }
    if (code) {
        const u32 n = piece(at, left, dat_006dfe8c, code);
        at += n;
        left -= n;
    }
    __builtin_va_list arguments;
    __builtin_va_start(arguments, format);
    const int32_t written = fun_005d139d(static_cast<char*>(pointer(at)), left, format, arguments);
    __builtin_va_end(arguments);
    at += written < 0 ? left : static_cast<u32>(written);
    put_byte(at, 0x0a);
    put_byte(at + 1, 0);
    const u32 self = address(sink);
    put(self + 8, word(self + 8) + 1);
    return FUN_00606843(sink, address(buffer));
}
