#pragma once
#include "item.h"
#include <array>
#include <optional>
namespace genesis {
struct CharacterEquipment {
    std::array<std::uint8_t, 8> types{}, ids{};
};
struct CharacterAppearance {
    // Render table order: face, hair/helmet, upper, lower, hand, foot.
    std::array<std::uint32_t, 6> indices{};
    bool helmet = false;
};
struct CharacterCloak {
    std::uint8_t model = 0, variant = 0;
};
// 004145a8..00414617: cape slot seven; upper-body slot one selects PC variant.
std::optional<CharacterCloak> character_cloak(const ItemTable& items,
                                              const CharacterEquipment& equipment,
                                              bool show_cloak = true);
// 00417220 / 00416660: resolve clothing, independently of weapon resources.
CharacterAppearance character_appearance(const ItemTable& items, std::uint8_t face,
                                         std::uint8_t hair, const CharacterEquipment& equipment,
                                         bool show_helmet = true);
} // namespace genesis
