#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace genesis {
struct AppearanceEntry {
    std::uint32_t model = 0, texture = 0, flags = 0;
};
struct AppearanceTable {
    std::vector<std::string> models, textures;
    std::vector<AppearanceEntry> appearances;
};
// ML2: two length-prefixed name catalogs followed by 12-byte appearance rows.
// Names/indices are authored data; flags are retained without guessed meaning.
AppearanceTable decode_appearance(const std::vector<std::uint8_t>& bytes);
} // namespace genesis
