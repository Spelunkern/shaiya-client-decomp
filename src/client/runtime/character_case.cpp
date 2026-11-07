#include "character_case.h"
#include "thread_data.h"
#include "nls.h"
extern "C" uint32_t fun_005d08a1(OriginalLocaleStorage* locale, uint32_t character) {
    if (!locale->undecoded_004[4] || (locale->invariant_024 && character <= 127)) {
        if (int32_t(character) >= 65 && int32_t(character) <= 90)
            return character + 32;
        return character;
    }
    if (character < 256) {
        const uint32_t uppercase = int32_t(locale->invariant_028) <= 1
                                       ? uint8_t(locale->table_048[character]) & 1u
                                       : fun_005d0beb(locale, int32_t(character), 1);
        if (!uppercase)
            return character;
    }
    // Original reuses the stack locale operand for its one/two-byte input.
    // Preserve the untouched high bytes while setting the actual terminator.
    uint32_t input = reinterpret_cast<uint32_t>(locale);
    const uint8_t high = uint8_t(character >> 8);
    uint32_t length;
    if (locale->table_048[high] & 0x8000) {
        input = (input & 0xff000000u) | high | ((character & 255u) << 8);
        length = 2;
    } else {
        input = (input & 0xffff0000u) | (character & 255u);
        length = 1;
    }
    unsigned char converted[4];
    const uint32_t result = fun_005d9327(
        locale->undecoded_004[4], 0x100, reinterpret_cast<const char*>(&input), int32_t(length),
        reinterpret_cast<char*>(converted), 3, locale->undecoded_004[0], 1);
    if (!result)
        return character;
    if (result == 1)
        return converted[0];
    return (uint32_t(converted[0]) << 8) | converted[1];
}
extern "C" int32_t fun_005d0969(int32_t character) {
    auto* data = static_cast<OriginalPerThreadData*>(fun_005d2659());
    auto* locale = static_cast<OriginalLocaleStorage*>(data->locale_64);
    if (locale != dat_0070e67c)
        locale = fun_005d505a();
    return int32_t(fun_005d08a1(locale, uint32_t(character)));
}
// Original 5cf94c, the uppercase counterpart of 5d08a1: _LOWER (2) instead of
// _UPPER (1), LCMAP_UPPERCASE (0x200) instead of LCMAP_LOWERCASE (0x100).
extern "C" uint32_t FUN_005cf94c(OriginalLocaleStorage* locale, uint32_t character) {
    if (!locale->undecoded_004[4] || (locale->invariant_024 && character <= 127)) {
        if (int32_t(character) >= 97 && int32_t(character) <= 122)
            return character - 32;
        return character;
    }
    if (character < 256) {
        const uint32_t lowercase = int32_t(locale->invariant_028) <= 1
                                       ? uint8_t(locale->table_048[character]) & 2u
                                       : fun_005d0beb(locale, int32_t(character), 2);
        if (!lowercase)
            return character;
    }
    // Same reuse of the stack locale operand as 5d08a1: only the actual input
    // bytes and terminator are written, the remaining high bytes stay intact.
    uint32_t input = reinterpret_cast<uint32_t>(locale);
    const uint8_t high = uint8_t(character >> 8);
    uint32_t length;
    if (locale->table_048[high] & 0x8000) {
        input = (input & 0xff000000u) | high | ((character & 255u) << 8);
        length = 2;
    } else {
        input = (input & 0xffff0000u) | (character & 255u);
        length = 1;
    }
    unsigned char converted[4];
    const uint32_t result = fun_005d9327(
        locale->undecoded_004[4], 0x200, reinterpret_cast<const char*>(&input), int32_t(length),
        reinterpret_cast<char*>(converted), 3, locale->undecoded_004[0], 1);
    if (!result)
        return character;
    if (result == 1)
        return converted[0];
    return (uint32_t(converted[0]) << 8) | converted[1];
}
// Original 5cfa15: toupper through the thread's current locale.
extern "C" int32_t FUN_005cfa15(int32_t character) {
    auto* data = static_cast<OriginalPerThreadData*>(fun_005d2659());
    auto* locale = static_cast<OriginalLocaleStorage*>(data->locale_64);
    if (locale != dat_0070e67c)
        locale = fun_005d505a();
    return int32_t(FUN_005cf94c(locale, uint32_t(character)));
}
extern "C" int32_t fun_005d9235(int32_t character) {
    auto* data = static_cast<OriginalPerThreadData*>(fun_005d2659());
    auto* locale = static_cast<OriginalLocaleStorage*>(data->locale_64);
    if (locale != dat_0070e67c)
        locale = fun_005d505a();
    if (int32_t(locale->invariant_028) > 1)
        return int32_t(fun_005d0beb(locale, character, 4));
    return uint8_t(locale->table_048[character]) & 4;
}
extern "C" int32_t FUN_005d0969(int32_t) __attribute__((alias("fun_005d0969")));
