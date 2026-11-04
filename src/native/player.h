#pragma once
#include "packet.h"
#include <optional>

namespace genesis::net {
struct CharacterDetails {
    std::uint16_t stat_points = 0, skill_points = 0, orientation = 0;
    std::array<std::uint32_t, 3> current_resources{}; // HP, MP, SP
    // Keep all three recovered experience words in wire order.
    std::array<std::uint32_t, 3> experience{};
    std::uint32_t money = 0;
    std::array<float, 3> position{};
    std::array<std::uint32_t, 4> combat_counts{};
    std::string guild_name;
};
struct PlayerState {
    std::uint32_t id = 0;
    std::uint16_t map = 0;
    std::optional<CharacterDetails> details;
    std::array<std::uint32_t, 3> current_resources{}, maximum_resources{};
    std::array<std::uint32_t, 12> additional_stats{};
    std::array<std::uint32_t, 4> combat_counts{};
    std::array<float, 3> position{};
    std::uint32_t money = 0;
    bool has_additional_stats = false, has_maximum_resources = false;
};
// 0058dbd0 -> session vtable +0x28 / 0055ff60. This is the recovered
// client's contract, including the word before three experience dwords.
CharacterDetails decode_character_details(const Bytes& body);
// Decode atomically; packets for another entity remain available to its owner.
bool apply_player_packet(PlayerState& player, const Packet& packet);
} // namespace genesis::net
