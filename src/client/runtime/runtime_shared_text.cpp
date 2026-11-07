#include "runtime_shared_text.h"
namespace {
// Both originals build a 256-bit set of the control bytes on the stack; the
// terminating NUL is never added, so it always ends the scan.
struct ByteSet {
    uint32_t bits[8];
};
ByteSet control_set(const char* control) {
    ByteSet set = {};
    for (auto* cursor = reinterpret_cast<const volatile uint8_t*>(control); *cursor; ++cursor) {
        const uint8_t byte = *cursor;
        set.bits[byte >> 5] |= 1u << (byte & 31u);
    }
    return set;
}
bool contains(const ByteSet& set, uint8_t byte) {
    return (set.bits[byte >> 5] >> (byte & 31u)) & 1u;
}
} // namespace

// Original 5d1d30 (strcspn): length of the leading run without control bytes.
extern "C" uint32_t FUN_005d1d30(const char* string, const char* control) {
    const ByteSet set = control_set(control);
    uint32_t length = 0;
    for (auto* cursor = reinterpret_cast<const volatile uint8_t*>(string);; ++cursor, ++length) {
        const uint8_t byte = *cursor;
        if (!byte || contains(set, byte))
            return length;
    }
}

// Original 5de920 (strpbrk): first byte found in control, or null at the NUL.
extern "C" char* FUN_005de920(const char* string, const char* control) {
    const ByteSet set = control_set(control);
    for (auto* cursor = reinterpret_cast<const volatile uint8_t*>(string);; ++cursor) {
        const uint8_t byte = *cursor;
        if (!byte)
            return nullptr;
        if (contains(set, byte))
            return const_cast<char*>(reinterpret_cast<const volatile char*>(cursor));
    }
}
