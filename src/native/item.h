#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace genesis {
struct ItemRecord {
    std::string name, description; // Original byte strings; encoding belongs to the UI.
    std::array<std::uint8_t, 81>
        payload{}; // Disk bytes, NOT the x86 92-byte pointer-bearing object.
    std::uint8_t model() const {
        return payload[2];
    }
    std::uint8_t icon() const {
        return payload[3];
    }
    // 00461141..00461145 stores disk 28..29 at runtime 0x22.
    // Two earlier disk bytes overwrite runtime 8, so runtime-8 is incorrect.
    std::uint8_t cloth_variant() const {
        return payload[28];
    }
    std::uint16_t minimum_level() const {
        return payload[4] | std::uint16_t(payload[5]) << 8;
    }
};
struct ItemTable {
    std::vector<std::vector<ItemRecord>> groups;
    // One-based indexes, matching the recovered item lookup. No pointer/layout assumptions.
    const ItemRecord* find(std::uint8_t type, std::uint8_t id) const;
};
ItemTable decode_items(const std::vector<std::uint8_t>& plaintext);
} // namespace genesis
