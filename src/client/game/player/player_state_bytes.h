#pragma once
#include <stdint.h>

// Exact-width stores into the opaque player-state object. Many of its fields
// are unaligned, and the originals leave the bytes between them untouched,
// so every store names its offset and width.
namespace player_state_bytes {
inline void set8(void* base, uint32_t offset, uint8_t value) {
    static_cast<uint8_t*>(base)[offset] = value;
}
inline void set16(void* base, uint32_t offset, uint16_t value) {
    __builtin_memcpy(static_cast<uint8_t*>(base) + offset, &value, 2);
}
inline void set32(void* base, uint32_t offset, uint32_t value) {
    __builtin_memcpy(static_cast<uint8_t*>(base) + offset, &value, 4);
}
inline void set_pointer(void* base, uint32_t offset, const void* value) {
    set32(base, offset, reinterpret_cast<uint32_t>(value));
}
inline uint32_t get32(const void* base, uint32_t offset) {
    uint32_t value;
    __builtin_memcpy(&value, static_cast<const uint8_t*>(base) + offset, 4);
    return value;
}
inline void* at(void* base, uint32_t offset) {
    return static_cast<uint8_t*>(base) + offset;
}
// Clears count consecutive dwords (the originals' rep stosd).
inline void zero32(void* base, uint32_t offset, uint32_t count) {
    for (uint32_t index = 0; index != count; ++index)
        set32(base, offset + index * 4, 0);
}
} // namespace player_state_bytes
