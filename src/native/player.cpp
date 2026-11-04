#include "player.h"
#include <cmath>
#include <cstring>
#include <limits>

namespace genesis::net {
namespace {
float coordinate(Reader& r) {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
    auto bits = r.u32();
    float value;
    std::memcpy(&value, &bits, 4);
    if (!std::isfinite(value))
        throw std::runtime_error("Non-finite player coordinate");
    return value;
}
std::array<float, 3> position(Reader& r) {
    return {coordinate(r), coordinate(r), coordinate(r)};
}
void end(Reader& r) {
    if (r.remaining())
        throw std::runtime_error("Unexpected player packet fields");
}
} // namespace
CharacterDetails decode_character_details(const Bytes& body) {
    Reader r(body);
    CharacterDetails value;
    value.stat_points = r.u16();
    value.skill_points = r.u16();
    for (auto& resource : value.current_resources)
        resource = r.u32();
    value.orientation = r.u16();
    for (auto& experience : value.experience)
        experience = r.u32();
    value.money = r.u32();
    value.position = position(r);
    for (auto& count : value.combat_counts)
        count = r.u32();
    if (r.remaining() > 25)
        throw std::runtime_error("Character-details name exceeds 25 bytes");
    // The original available-byte read permits a short name tail. Native
    // storage has an explicit length instead of relying on stack termination.
    value.guild_name = r.text(r.remaining());
    return value;
}
bool apply_player_packet(PlayerState& player, const Packet& packet) {
    Reader r(packet.body);
    switch (packet.opcode) {
    case 0x0105: {
        auto details = decode_character_details(packet.body);
        player.current_resources = details.current_resources;
        player.position = details.position;
        player.money = details.money;
        player.combat_counts = details.combat_counts;
        player.details = std::move(details);
        return true;
    }
    case 0x0526: {
        std::array<std::uint32_t, 12> stats;
        for (auto& stat : stats)
            stat = r.u32();
        end(r);
        player.additional_stats = stats;
        player.has_additional_stats = true;
        return true;
    }
    case 0x051f: {
        auto id = r.u32();
        std::array<std::uint32_t, 3> values;
        for (auto& value : values)
            value = r.u32();
        end(r);
        if (id != player.id)
            return false;
        player.maximum_resources = values;
        player.has_maximum_resources = true;
        return true;
    }
    case 0x0521: {
        std::array<std::uint32_t, 3> values;
        for (auto& value : values)
            value = r.u32();
        end(r);
        player.current_resources = values;
        return true;
    }
    case 0x0213: {
        auto money = r.u32();
        end(r);
        player.money = money;
        return true;
    }
    case 0x020e: {
        auto index = r.u8();
        auto value = r.u32();
        end(r);
        if (index >= player.combat_counts.size())
            throw std::runtime_error("Invalid combat-counter index");
        player.combat_counts[index] = value;
        return true;
    }
    case 0x020b: {
        auto id = r.u32();
        auto map = r.u16();
        auto coordinates = position(r);
        end(r);
        if (id != player.id)
            return false;
        player.map = map;
        player.position = coordinates;
        return true;
    }
    default:
        return false;
    }
}
} // namespace genesis::net
