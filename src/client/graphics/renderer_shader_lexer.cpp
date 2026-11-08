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
extern const char dat_006e0000[], dat_006e0028[], dat_006e004c[];
u32 __attribute__((thiscall)) FUN_0060749f(void*, u32, u32);
u32 __attribute__((thiscall)) FUN_0060773d(void*, u32, u32);
u32 __attribute__((thiscall)) FUN_00607780(void*, u32, u32);
// The client's atof (still an instruction listing); the result is in ST0.
double FUN_005cf914(const char*);
u32 __attribute__((thiscall)) FUN_006069ee(void*, u32, u32);
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
// Original 006069ee..00606b77 (thiscall, RET 8): floating literal: digits
// with an optional fraction, or '.' and digits, then an optional exponent
// ('e' digits or 'e-' digits). Plain digits without '.' or exponent are not
// a float (0). With `out`, the text is copied NUL-terminated into a stack
// block ((length + 4) & ~3 bytes, through the stack probe) and converted with
// the client's atof; the double is stored through `out`.
extern "C" u32 __attribute__((thiscall)) FUN_006069ee(void* object, u32 start, u32 out) {
    const u32 self = address(object);
    u32 at = start;
    bool integer_only = false;
    if (at < word(self + 4) && fun_005d9235(sbyte(at))) {
        do
            ++at;
        while (at < word(self + 4) && fun_005d9235(sbyte(at)));
        const u32 end = word(self + 4);
        if (at < end && sbyte(at) == 0x2e) {
            ++at;
            while (at < end && fun_005d9235(sbyte(at))) {
                ++at;
                if (at >= word(self + 4))
                    break;
            }
        } else {
            integer_only = true;
        }
    } else {
        if (at + 1 >= word(self + 4) || sbyte(at) != 0x2e || !fun_005d9235(sbyte(at + 1)))
            return 0;
        at += 2;
        while (at < word(self + 4) && fun_005d9235(sbyte(at)))
            ++at;
    }
    bool exponent = false;
    if (at + 1 < word(self + 4) && fun_005d0969(sbyte(at)) == 0x65 && fun_005d9235(sbyte(at + 1))) {
        at += 2;
        exponent = true;
    } else if (at + 2 < word(self + 4) && fun_005d0969(sbyte(at)) == 0x65 && sbyte(at + 1) == 0x2d &&
               fun_005d9235(sbyte(at + 2))) {
        at += 3;
        exponent = true;
    }
    if (exponent) {
        while (at < word(self + 4) && fun_005d9235(sbyte(at)))
            ++at;
    } else if (integer_only) {
        return 0;
    }
    if (out) {
        const u32 bytes = at - start;
        char* const text = static_cast<char*>(__builtin_alloca((bytes + 4) & ~3u));
        copy_bytes(address(text), start, bytes);
        put_byte(address(text) + bytes, 0);
        const double value = FUN_005cf914(text);
        __builtin_memcpy(pointer(out), &value, 8);
    }
    return at - start;
}
// Original 0060749f..00607636 (thiscall, RET 8): one character of a
// character or string literal, its value (sign-extended) through `out`.
// Unless lexer flag 4 is set, a backslash starts an escape: \a \b \f \n \r
// \t \v, up to three octal digits, \x and hexadecimal digits (modulo 2^32),
// or the next character itself. A backslash at the end is reported (0x3ef)
// and, as in the original, the byte after it is still read.
extern "C" u32 __attribute__((thiscall)) FUN_0060749f(void* object, u32 start, u32 out) {
    const u32 self = address(object);
    const u32 end = word(self + 4);
    if (start >= end)
        return 0;
    if (sbyte(start) != 0x5c || (word(self + 0x28) & 4)) {
        put(out, static_cast<u32>(sbyte(start)));
        return 1;
    }
    u32 at = start + 1;
    if (at >= end)
        warn(self, 0x3ef, dat_006e0000);
    const int32_t c = sbyte(at);
    static const unsigned char letters[7] = {0x61, 0x62, 0x66, 0x6e, 0x72, 0x74, 0x76};
    static const unsigned char values[7] = {7, 8, 0x0c, 0x0a, 0x0d, 9, 0x0b};
    for (u32 i = 0; i < 7; ++i)
        if (c == letters[i]) {
            put(out, values[i]);
            return at + 1 - start;
        }
    if (c >= 0x30 && c <= 0x37) {
        u32 limit = word(self + 4);
        if (at + 3 < limit)
            limit = at + 3;
        u32 value = 0;
        while (at < limit) {
            const int32_t d = sbyte(at);
            if (d < 0x30 || d > 0x37)
                break;
            value = static_cast<u32>(d) + value * 8 - 0x30;
            ++at;
        }
        put(out, value);
        return at - start;
    }
    if (c == 0x78) {
        const u32 digits = at + 1;
        if (digits < word(self + 4) && fun_005d926f(sbyte(digits))) {
            at = digits;
            u32 value = 0;
            while (at < word(self + 4) && fun_005d926f(sbyte(at))) {
                const int32_t d = sbyte(at);
                value <<= 4;
                if (d >= 0x61)
                    value += static_cast<u32>(d) - 0x57;
                else if (d >= 0x41)
                    value += static_cast<u32>(d) - 0x37;
                else
                    value += static_cast<u32>(d) - 0x30;
                ++at;
            }
            put(out, value);
            return at - start;
        }
    }
    put(out, static_cast<u32>(sbyte(at)));
    return at + 1 - start;
}
// Original 0060773d..0060777e (thiscall, RET 8): a 'c' character literal.
extern "C" u32 __attribute__((thiscall)) FUN_0060773d(void* object, u32 start, u32 out) {
    const u32 self = address(object);
    if (start >= word(self + 4) || sbyte(start) != 0x27)
        return 0;
    const u32 inner = start + 1;
    const u32 bytes = FUN_0060749f(object, inner, out);
    if (!bytes)
        return 0;
    const u32 close = inner + bytes;
    if (close >= word(self + 4) || sbyte(close) != 0x27)
        return 0;
    return close - start + 1;
}
// Original 00607780..006078c8 (thiscall, RET 8): a "..." string literal, or
// <...> with lexer flag 8. The first pass finds the closing character (or a
// newline), counting escaped line ends at +0x1c; running into the end of the
// text (0x3ee, the length is then cut there) or a newline (0x3ed) is
// reported. The decoded text is then copied NUL-terminated into the arena,
// skipping escaped line ends; the result is the length up to the closing
// character, inclusive.
extern "C" u32 __attribute__((thiscall)) FUN_00607780(void* object, u32 start, u32 out) {
    const u32 self = address(object);
    if (start >= word(self + 4))
        return 0;
    const int32_t open = sbyte(start);
    int32_t close;
    if (open == 0x22)
        close = open;
    else if (open == 0x3c && (word(self + 0x28) & 8))
        close = 0x3e;
    else
        return 0;
    const u32 content = start + 1;
    u32 at = content;
    if (at < word(self + 4)) {
        do {
            const int32_t c = sbyte(at);
            if (c == close || c == 0x0a)
                break;
            if (c == 0x5c && !(word(self + 0x28) & 4)) {
                const u32 end = word(self + 4);
                ++at;
                if (at < end) {
                    const int32_t d = sbyte(at);
                    if (d == 0x0a) {
                        put(self + 0x1c, word(self + 0x1c) + 1);
                    } else if (d == 0x0d && at + 1 < end && sbyte(at + 1) == 0x0a) {
                        ++at;
                        put(self + 0x1c, word(self + 0x1c) + 1);
                    }
                }
            }
            ++at;
        } while (at < word(self + 4));
    }
    if (at >= word(self + 4)) {
        at = word(self + 4);
        warn(self, 0x3ee, dat_006e004c);
    } else if (sbyte(at) == 0x0a) {
        warn(self, 0x3ed, dat_006e0028);
    }
    const u32 stop = at, bytes = at - start;
    u32 text = address(FUN_00651c2f(pointer(word(self + 0x2c)), bytes, 1));
    if (!text)
        return 0;
    put(out, text);
    u32 cursor = content;
    u32 decoded = out;
    for (;;) {
        if (cursor + 1 < stop) {
            u32 next = cursor + 1;
            while (sbyte(cursor) == 0x5c && !(word(self + 0x28) & 4)) {
                const int32_t d = sbyte(next);
                if (d == 0x0a) {
                    cursor += 2;
                } else if (d == 0x0d && cursor + 2 < stop && sbyte(cursor + 2) == 0x0a) {
                    cursor += 3;
                } else {
                    break;
                }
                next = cursor + 1;
                if (next >= stop)
                    break;
            }
        }
        if (cursor >= stop)
            break;
        // The original decodes into its own `out` argument slot.
        cursor += FUN_0060749f(object, cursor, address(&decoded));
        put_byte(text, decoded);
        ++text;
    }
    put_byte(text, 0);
    return bytes + 1;
}
