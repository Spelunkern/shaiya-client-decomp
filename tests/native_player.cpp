#include "player.h"
#include "fixtures/native_player.h"
#include <cstdio>
using namespace genesis::net;
namespace {
void expect(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void rejects(PlayerState& player, const Packet& packet) {
    bool failed = false;
    try {
        (void)apply_player_packet(player, packet);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    expect(failed, "Malformed player packet accepted");
}
} // namespace
int main() {
    try {
        PlayerState player;
        player.id = 1234;
        player.map = 7;
        expect(apply_player_packet(player, {0x0105, player_fixture::details}),
               "Details not dispatched");
        const auto& d = *player.details;
        expect(d.stat_points == 321 && d.skill_points == 654 && d.orientation == 123,
               "Details word offsets");
        expect(d.current_resources == std::array<std::uint32_t, 3>{70000, 80000, 90000},
               "Details resource widths");
        expect(d.experience == std::array<std::uint32_t, 3>{111222, 333444, 555666} &&
                   d.money == 777888,
               "Details experience/money offsets");
        expect(d.position == std::array<float, 3>{12.5f, -3.25f, 987.75f} &&
                   d.combat_counts[3] == 44 && d.guild_name == "G\xe9nesis",
               "Details position/counters/tail");
        for (std::size_t size = 0; size < 62; ++size) {
            rejects(player, {0x0105, Bytes(player_fixture::details.begin(),
                                           player_fixture::details.begin() + size)});
            expect(player.money == 777888 && player.position[1] == -3.25f,
                   "Truncated details mutated player");
        }
        for (std::size_t size = 62; size <= 87; ++size) {
            auto decoded = decode_character_details(
                Bytes(player_fixture::details.begin(), player_fixture::details.begin() + size));
            expect(decoded.guild_name.size() == std::min<std::size_t>(size - 62, 7),
                   "Short details tail");
        }
        auto malformed = player_fixture::details;
        malformed.push_back(0);
        rejects(player, {0x0105, malformed});
        malformed = player_fixture::details;
        malformed[34] = 0;
        malformed[35] = 0;
        malformed[36] = 0x80;
        malformed[37] = 0x7f;
        rejects(player, {0x0105, malformed});
        Writer stats;
        for (unsigned i = 1; i <= 12; ++i)
            stats.u32(100000 + i);
        expect(apply_player_packet(player, {0x0526, stats.finish()}) &&
                   player.additional_stats[11] == 100012 && player.has_additional_stats,
               "Additional stats");
        Writer max;
        max.u32(1234);
        for (auto value : {100000, 110000, 120000})
            max.u32(value);
        auto maxima = max.finish();
        expect(apply_player_packet(player, {0x051f, maxima}) &&
                   player.maximum_resources[2] == 120000,
               "Maximum resources");
        maxima[0] = 0;
        expect(!apply_player_packet(player, {0x051f, maxima}) &&
                   player.maximum_resources[2] == 120000,
               "Another entity's maxima consumed");
        Writer current;
        for (auto value : {130000, 140000, 150000})
            current.u32(value);
        expect(apply_player_packet(player, {0x0521, current.finish()}) &&
                   player.current_resources[1] == 140000,
               "Current resources");
        expect(apply_player_packet(player, {0x0213, {1, 2, 3, 4}}) && player.money == 0x04030201,
               "Money update");
        expect(apply_player_packet(player, {0x020e, {2, 4, 3, 2, 1}}) &&
                   player.combat_counts[2] == 0x01020304,
               "Combat counter");
        rejects(player, {0x020e, {4, 0, 0, 0, 0}});
        rejects(player, {0x0521, {0, 0, 0, 0}});
        Bytes teleport = {0xd2, 4, 0, 0, 9, 0};
        teleport.insert(teleport.end(), player_fixture::details.begin() + 34,
                        player_fixture::details.begin() + 46);
        expect(apply_player_packet(player, {0x020b, teleport}) && player.map == 9 &&
                   player.position[2] == 987.75f,
               "Map/position update");
        teleport[0] = 0;
        expect(!apply_player_packet(player, {0x020b, teleport}),
               "Another entity's teleport consumed");
        expect(!apply_player_packet(player, {0xffff, {}}), "Unknown opcode consumed");
        std::puts("Native player state follows recovered client packet contracts");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
