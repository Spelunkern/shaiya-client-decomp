#include "character_classify.h"
#include "thread_data.h"
#include "nls.h"
extern "C" int32_t fun_005d926f(int32_t character) {
    auto* data = static_cast<OriginalPerThreadData*>(fun_005d2659());
    auto* locale = static_cast<OriginalLocaleStorage*>(data->locale_64);
    if (locale != dat_0070e67c)
        locale = fun_005d505a();
    if (int32_t(locale->invariant_028) > 1)
        return int32_t(fun_005d0beb(locale, character, 0x80));
    return uint8_t(locale->table_048[character]) & 0x80;
}
extern "C" int32_t fun_005d92ae(int32_t character) {
    auto* data = static_cast<OriginalPerThreadData*>(fun_005d2659());
    auto* locale = static_cast<OriginalLocaleStorage*>(data->locale_64);
    if (locale != dat_0070e67c)
        locale = fun_005d505a();
    if (int32_t(locale->invariant_028) > 1)
        return int32_t(fun_005d0beb(locale, character, 8));
    return uint8_t(locale->table_048[character]) & 8;
}
// Originals 005d91f6 and 005d92e8: the same locale-aware classification for
// the alphabetic (0x103) and alphanumeric (0x107) masks, reading the whole
// 16-bit table entry.
extern "C" int32_t FUN_005d91f6(int32_t character) {
    auto* data = static_cast<OriginalPerThreadData*>(fun_005d2659());
    auto* locale = static_cast<OriginalLocaleStorage*>(data->locale_64);
    if (locale != dat_0070e67c)
        locale = fun_005d505a();
    if (int32_t(locale->invariant_028) > 1)
        return int32_t(fun_005d0beb(locale, character, 0x103));
    return locale->table_048[character] & 0x103;
}
extern "C" int32_t FUN_005d92e8(int32_t character) {
    auto* data = static_cast<OriginalPerThreadData*>(fun_005d2659());
    auto* locale = static_cast<OriginalLocaleStorage*>(data->locale_64);
    if (locale != dat_0070e67c)
        locale = fun_005d505a();
    if (int32_t(locale->invariant_028) > 1)
        return int32_t(fun_005d0beb(locale, character, 0x107));
    return locale->table_048[character] & 0x107;
}
