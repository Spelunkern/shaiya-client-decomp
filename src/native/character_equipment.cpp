#include "character_equipment.h"
namespace genesis {
std::optional<CharacterCloak>
character_cloak(const ItemTable& items, const CharacterEquipment& equipment, bool show_cloak) {
    if (!show_cloak || !equipment.types[7])
        return std::nullopt;
    CharacterCloak result;
    if (auto item = items.find(equipment.types[7], equipment.ids[7]))
        result.model = item->model();
    if (auto upper = items.find(equipment.types[1], equipment.ids[1])) {
        auto variant = std::uint8_t(upper->cloth_variant() - 1);
        if (variant <= 7)
            result.variant = variant;
    }
    return result;
}
CharacterAppearance character_appearance(const ItemTable& items, std::uint8_t face,
                                         std::uint8_t hair, const CharacterEquipment& equipment,
                                         bool show_helmet) {
    CharacterAppearance result;
    result.indices = {face, hair, 0, 0, 0, 0};
    // Actor 0x18c/0x199 are equipment type/id slot zero; 0x4c is hair/helmet.
    // The original switches to the helmet table on TYPE alone, even if lookup
    // fails, and then retains the hair index. Hidden helmets keep normal hair.
    result.helmet = show_helmet && equipment.types[0] != 0;
    if (result.helmet)
        if (auto item = items.find(equipment.types[0], equipment.ids[0]))
            result.indices[1] = item->model();
    // 004174e5..00417591 initializes each body appearance to zero and replaces
    // it with runtime item+0x0a (disk byte 2). No subtract-one or class remapping.
    for (unsigned slot = 1; slot <= 4; ++slot)
        if (auto item = items.find(equipment.types[slot], equipment.ids[slot]))
            result.indices[slot + 1] = item->model();
    return result;
}
} // namespace genesis
