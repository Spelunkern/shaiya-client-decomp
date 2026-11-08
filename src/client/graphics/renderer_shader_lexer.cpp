#include "runtime/character_case.h"
#include "runtime/character_classify.h"
// Token scanners of the shader assembler's lexer. They work on the source
// stream of renderer_shader_stream.cpp (+0x00 cursor, +0x04 end, +0x08 the
// location reported with errors, +0x1c line, +0x28 lexer flags, +0x2c
// arena, +0x30 message sink). Each returns the length it consumed, 0 when
// the text at `start` does not match.
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
// MOVSX of the byte at `p`.
int32_t sbyte(u32 p) {
    int32_t result;
    __asm__ volatile("movsbl (%1),%0" : "=r"(result) : "r"(p) : "memory");
    return result;
}
void put_byte(u32 p, u32 v) {
    __asm__ volatile("movb %b1,(%0)" ::"r"(p), "q"(v) : "memory");
}
void* pointer(u32 p) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(p));
}
u32 address(const void* p) {
    return static_cast<u32>(reinterpret_cast<uintptr_t>(p));
}
// REP MOVSD / REP MOVSB of `bytes` bytes.
void copy_bytes(u32 to, u32 from, u32 bytes) {
    u32 dwords = bytes >> 2, rest = bytes & 3;
    __asm__ volatile("rep movsl\n\tmovl %3,%%ecx\n\trep movsb"
                     : "+D"(to), "+S"(from), "+c"(dwords)
                     : "r"(rest)
                     : "memory");
}
} // namespace
extern "C" {
u32 FUN_00606e48(void*, const void*, u32, const char*, ...);
u32 __attribute__((thiscall)) FUN_00606993(void*);
void* __attribute__((thiscall)) FUN_00651c2f(void*, u32, u32);
// Lexer warnings (renderer_shader_storage.cpp).
extern const char dat_006dff78[], dat_006dff9c[], dat_006dffbc[], dat_006dffdc[];
u32 __attribute__((thiscall)) FUN_00606b79(void*, u32, u32);
u32 __attribute__((thiscall)) FUN_00606d0b(void*, u32, u32);
u32 __attribute__((thiscall)) FUN_00606d55(void*, u32, u32);
u32 __attribute__((thiscall)) FUN_006071b1(void*);
u32 __attribute__((thiscall)) FUN_006072da(void*, u32, u32);
u32 __attribute__((thiscall)) FUN_0060737e(void*, u32, u32);
u32 __attribute__((thiscall)) FUN_006073fc(void*, u32, u32);
}
namespace {
void warn(u32 self, u32 code, const char* message) {
    FUN_00606e48(pointer(word(self + 0x30)), pointer(self + 8), code, message);
}
} // namespace
// Original 00606b79..00606bfb (thiscall, RET 8): an identifier ([A-Za-z_]
// then [A-Za-z0-9_] by the locale tables), copied NUL-terminated into the
// arena at +0x2c and stored through `out`; 0 when the copy cannot be
// allocated.
extern "C" u32 __attribute__((thiscall)) FUN_00606b79(void* object, u32 start, u32 out) {
    const u32 self = address(object);
    if (start >= word(self + 4))
        return 0;
    if (!FUN_005d91f6(sbyte(start)) && sbyte(start) != 0x5f)
        return 0;
    u32 at = start + 1;
    while (at < word(self + 4) && (FUN_005d92e8(sbyte(at)) || sbyte(at) == 0x5f))
        ++at;
    const u32 bytes = at - start;
    const u32 copy = address(FUN_00651c2f(pointer(word(self + 0x2c)), bytes + 1, 1));
    if (!copy)
        return 0;
    copy_bytes(copy, start, bytes);
    put_byte(copy + bytes, 0);
    put(out, copy);
    return bytes;
}
// Original 00606d0b..00606d55 (thiscall, RET 8): float suffix: 'f' gives
// type 7, 'h' type 6, none type 5 (stored when `out` is set). At the end of
// the text nothing is stored.
extern "C" u32 __attribute__((thiscall)) FUN_00606d0b(void* object, u32 start, u32 out) {
    const u32 self = address(object);
    if (start >= word(self + 4))
        return 0;
    u32 type = 5, at = start;
    const int32_t c = fun_005d0969(sbyte(start));
    if (c == 0x66) {
        type = 7;
        at = start + 1;
    } else if (c == 0x68) {
        type = 6;
        at = start + 1;
    }
    if (out)
        put(out, type);
    return at - start;
}
// Original 00606d55..00606ddc (thiscall, RET 8): integer suffixes 'u' and
// 'l' in any order, each at most once: type 4 for unsigned, else 3 for long;
// `out` is left unchanged without a suffix.
extern "C" u32 __attribute__((thiscall)) FUN_00606d55(void* object, u32 start, u32 out) {
    const u32 self = address(object);
    bool is_unsigned = false, is_long = false;
    u32 at = start;
    if (at < word(self + 4)) {
        for (;;) {
            if (!is_unsigned && fun_005d0969(sbyte(at)) == 0x75) {
                is_unsigned = true;
            } else {
                if (is_long || fun_005d0969(sbyte(at)) != 0x6c)
                    break;
                is_long = true;
            }
            if (++at >= word(self + 4))
                break;
        }
    }
    if (out) {
        if (is_unsigned)
            put(out, 4);
        else if (is_long)
            put(out, 3);
    }
    return at - start;
}
// Original 006071b1..006072da (thiscall): skips blanks (space and 9..13),
// "//" comments (and ';' ones with lexer flag 2) to the end of the line,
// "/* */" comments and line continuations, counting lines at +0x1c. True
// when a newline was consumed. An unterminated block comment is reported
// (0x3e9) and scanning goes on at the end of the text.
extern "C" u32 __attribute__((thiscall)) FUN_006071b1(void* object) {
    const u32 self = address(object);
    u32 newline = 0;
    if (word(self) >= word(self + 4))
        return 0;
    for (;;) {
        u32 at = word(self);
        const int32_t c = sbyte(at);
        bool store = true;
        if (c == 0x0a) {
            ++at;
            put(self + 0x1c, word(self + 0x1c) + 1);
            newline = 1;
        } else if (c == 0x20 || (c >= 9 && c <= 0x0d)) {
            ++at;
        } else {
            bool line_comment = false;
            bool handled = false;
            if (c == 0x2f) {
                if (at + 1 < word(self + 4) && sbyte(at + 1) == c) {
                    line_comment = true;
                } else {
                    const u32 end = word(self + 4);
                    if (at + 1 < end && sbyte(at + 1) == 0x2a) {
                        at += 2;
                        put(self, at);
                        if (at < end) {
                            do {
                                at = word(self);
                                const int32_t d = sbyte(at);
                                if (d == 0x2a && at + 1 < end && sbyte(at + 1) == 0x2f)
                                    break;
                                if (d == 0x0a)
                                    put(self + 0x1c, word(self + 0x1c) + 1);
                                ++at;
                                put(self, at);
                            } while (at < word(self + 4));
                        }
                        at = word(self);
                        if (at < end) {
                            at += 2;
                        } else {
                            warn(self, 0x3e9, dat_006dff78);
                            store = false;
                        }
                        handled = true;
                    }
                }
            }
            if (!handled && !line_comment && (word(self + 0x28) & 2) && c == 0x3b)
                line_comment = true;
            if (line_comment) {
                FUN_00606993(object);
                store = false;
            } else if (!handled) {
                if (c != 0x5c)
                    break;
                if (at + 1 < word(self + 4) && sbyte(at + 1) == 0x0a) {
                    at += 2;
                } else if (at + 2 < word(self + 4) && sbyte(at + 1) == 0x0d && sbyte(at + 2) == 0x0a) {
                    at += 3;
                } else {
                    break;
                }
                put(self + 0x1c, word(self + 0x1c) + 1);
            }
        }
        if (store)
            put(self, at);
        if (word(self) >= word(self + 4))
            break;
    }
    return newline;
}
// Original 006072da..0060737c (thiscall, RET 8): "0x" hexadecimal literal
// (at least one digit), value through `out` modulo 2^32; more than ten
// characters are reported (0x3ea) but still consumed.
extern "C" u32 __attribute__((thiscall)) FUN_006072da(void* object, u32 start, u32 out) {
    const u32 self = address(object);
    u32 at = start + 2;
    if (at >= word(self + 4) || sbyte(start) != 0x30 || sbyte(start + 1) != 0x78 ||
        !fun_005d926f(sbyte(at)))
        return 0;
    u32 value = 0;
    while (at < word(self + 4) && fun_005d926f(sbyte(at))) {
        const int32_t c = sbyte(at);
        value <<= 4;
        if (c >= 0x61)
            value += static_cast<u32>(c) - 0x57;
        else if (c >= 0x41)
            value += static_cast<u32>(c) - 0x37;
        else
            value += static_cast<u32>(c) - 0x30;
        ++at;
    }
    if (out)
        put(out, value);
    const u32 bytes = at - start;
    if (static_cast<int32_t>(bytes) > 10)
        warn(self, 0x3ea, dat_006dff9c);
    return bytes;
}
// Original 0060737e..006073fa (thiscall, RET 8): '0'-prefixed octal literal
// (the prefix alone is 0), value through `out` modulo 2^32; a digit shifted
// past 32 bits is reported (0x3eb).
extern "C" u32 __attribute__((thiscall)) FUN_0060737e(void* object, u32 start, u32 out) {
    const u32 self = address(object);
    const u32 end = word(self + 4);
    if (start >= end || sbyte(start) != 0x30)
        return 0;
    u32 value = 0, overflow = 0, at = start + 1;
    while (at < end) {
        const int32_t c = sbyte(at);
        if (c < 0x30 || c > 0x37)
            break;
        if (value & 0xe0000000u)
            overflow = 1;
        value = static_cast<u32>(c) + value * 8 - 0x30;
        ++at;
    }
    if (out)
        put(out, value);
    if (overflow)
        warn(self, 0x3eb, dat_006dffbc);
    return at - start;
}
// Original 006073fc..0060749d (thiscall, RET 8): decimal literal by the
// locale digit test, value through `out` modulo 2^32; overflow is reported
// (0x3ec).
extern "C" u32 __attribute__((thiscall)) FUN_006073fc(void* object, u32 start, u32 out) {
    const u32 self = address(object);
    if (start >= word(self + 4) || !fun_005d9235(sbyte(start)))
        return 0;
    u32 value = 0, overflow = 0, at = start;
    while (at < word(self + 4) && fun_005d9235(sbyte(at))) {
        if (value > 0x19999999u)
            overflow = 1;
        const u32 times_ten = value * 10;
        value = static_cast<u32>(sbyte(at)) + times_ten - 0x30;
        if (value < times_ten)
            overflow = 1;
        ++at;
    }
    if (out)
        put(out, value);
    if (overflow)
        warn(self, 0x3ec, dat_006dffdc);
    return at - start;
}
