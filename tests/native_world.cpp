#include "world.h"
#include "fixtures/native_a101.h"
#include "fixtures/native_player.h"
#include "crypto/sha256.h"
#include <cstdio>

using namespace genesis::net;
namespace {
void expect(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
Packet character(std::uint32_t id = 1234) {
    Writer w;
    w.u8(2);
    w.u32(id);
    if (!id)
        return {0x0101, w.finish()};
    w.u32(0);
    w.u16(42);
    for (auto byte : {1, 2, 3, 4, 5, 6, 1})
        w.u8(byte);
    w.u16(7);
    for (unsigned i = 0; i < 6; ++i)
        w.u16(100 + i);
    w.u16(1000);
    w.u16(2000);
    w.u16(3000);
    std::array<std::uint8_t, 8> types = {1, 2, 3, 4, 5, 6, 7, 8}, ids = {8, 7, 6, 5, 4, 3, 2, 1};
    w.append(types.data(), 8);
    w.append(ids.data(), 8);
    w.text("Genesis", 19);
    w.u8(0);
    w.u8(1);
    for (unsigned i = 0; i < 6; ++i)
        w.u8(50 + i);
    return {0x0101, w.finish()};
}
void deliver(World& client, Wire& server, const Packet& packet) {
    auto frame = server.encode(packet);
    client.receive(frame.data(), frame.size());
}
} // namespace
int main() {
    if (genesis::net::world_map_path(0) != "data/world/0.wld" ||
        genesis::net::world_map_path(65535) != "data/world/65535.wld")
        return 1;
    try {
        Wire transport;
        transport.crypto.aes_material(a101_fixture::material);
        LoginAccount account;
        account.id = 0x12345678;
        account.identity.fill(0x77);
        World client(std::move(transport), account);
        auto frame = client.take_outgoing();
        Wire plain;
        plain.feed(frame.data(), frame.size());
        auto opening = plain.pop();
        expect(opening && opening->opcode == 0xa301 &&
                   opening->body == world_handshake(account).body && frame.size() == 24,
               "Plaintext world identity");
        auto world_material = a101_fixture::material;
        std::array<std::uint8_t, 32> digest;
        sha256_digest(digest.data(), world_material.data() + 16, 16);
        std::copy_n(digest.data(), 16, world_material.begin() + 16);
        Wire server;
        server.crypto.aes_material(world_material);
        Writer accept;
        accept.u8(0);
        accept.u8(2);
        for (unsigned i = 1; i <= 16; ++i)
            accept.u8(i);
        auto incoming = server.encode({0xa301, accept.finish()});
        auto slot_frame = server.encode(character());
        incoming.insert(incoming.end(), slot_frame.begin(), slot_frame.end());
        auto empty = server.encode({0x0101, {0, 0, 0, 0, 0}});
        incoming.insert(incoming.end(), empty.begin(), empty.end());
        auto faction = server.encode({0x0109, {1, 3}});
        incoming.insert(incoming.end(), faction.begin(), faction.end());
        for (auto byte : incoming)
            client.receive(&byte, 1);
        expect(client.state() == World::State::characters && client.faction() == 1 &&
                   client.max_mode() == 3,
               "World handshake/faction state");
        const auto& slot = client.characters().at(2);
        expect(slot.id == 1234 && slot.level == 42 && slot.race == 1 && slot.mode == 2 &&
                   slot.hair == 3 && slot.face == 4 && slot.height == 5 && slot.job == 6 &&
                   slot.gender == 1 && slot.map == 7,
               "Character scalar fields");
        expect(slot.stats[5] == 105 && slot.resources[2] == 3000 && slot.name == "Genesis" &&
                   slot.rename_pending == 1 && slot.equipment_types[7] == 8 &&
                   slot.extra_equipment[5] == 55,
               "Character arrays/name/flags");
        expect(client.characters().at(0).id == 0, "Empty character slot");
        client.select_character(1234);
        frame = client.take_outgoing();
        server.feed(frame.data(), frame.size());
        auto selected = server.pop();
        expect(selected && selected->opcode == 0x0104 && selected->body == Bytes({0xd2, 4, 0, 0}),
               "Character selection request");
        Writer fail;
        fail.u8(1);
        fail.u32(1234);
        deliver(client, server, {0x0104, fail.finish()});
        expect(client.state() == World::State::characters && client.status() == 1,
               "Rejected character selection");
        client.select_character(1234);
        frame = client.take_outgoing();
        server.feed(frame.data(), frame.size());
        (void)server.pop();
        Writer success;
        success.u8(0);
        success.u32(1234);
        deliver(client, server, {0x0104, success.finish()});
        expect(client.state() == World::State::loading, "Accepted character selection");
        expect(client.player() && client.player()->id == 1234 && client.player()->map == 7,
               "Selected player ownership");
        expect(client.selected_map_path() && *client.selected_map_path() == "data/world/7.wld",
               "Selected player map resource path");
        deliver(client, server, {0x0105, player_fixture::details});
        expect(client.player()->details && client.player()->position[2] == 987.75f &&
                   client.take_events().empty(),
               "Typed details integrated into World");
        Writer bar;
        bar.u8(1);
        for (auto b : {0, 1, 2, 3, 4})
            bar.u8(b);
        incoming = server.encode({0x010b, bar.finish()});
        // Independent hashlib-generated XOR fixtures for seed bytes 1..16.
        // The following two packets must not advance the AES receive counter.
        const Bytes map_xor = {8, 0, 0x8a, 0xf1, 0x35, 0x01, 0x20, 0xc2};
        const Bytes logout_xor = {4, 0, 0xac, 0xef};
        incoming.insert(incoming.end(), map_xor.begin(), map_xor.end());
        incoming.insert(incoming.end(), logout_xor.begin(), logout_xor.end());
        auto resumed = server.encode({0x0109, {0, 2}});
        incoming.insert(incoming.end(), resumed.begin(), resumed.end());
        client.receive(incoming.data(), incoming.size());
        auto events = client.take_events();
        expect(events.size() == 2 && events[0].opcode == 0x0201 &&
                   events[0].body == Bytes({9, 8, 7, 6}) && events[1].opcode == 0x0107,
               "AES/XOR/AES transition in a coalesced read");
        expect(client.state() == World::State::characters && client.faction() == 0 &&
                   client.quick_slots().size() == 1 && client.quick_slots()[0][4] == 4,
               "World logout and quick-slot state");
        expect(!client.player(), "Logout retained the selected player state");
        expect(!client.selected_map_path(), "Logout retained the selected map path");
        auto occupied = character().body;
        for (std::size_t count = 0; count < 54; ++count) {
            try {
                (void)decode_character_slot(Bytes(occupied.begin(), occupied.begin() + count));
                throw std::logic_error("Accepted incomplete character geometry/state fields");
            } catch (const std::runtime_error&) {
            }
        }
        for (std::size_t count = 54; count < occupied.size(); ++count) {
            try {
                (void)decode_character_slot(Bytes(occupied.begin(), occupied.begin() + count));
                throw std::logic_error("Accepted missing mandatory equipment extension");
            } catch (const std::runtime_error&) {
            }
        }
        auto short_name = occupied;
        short_name[45] = 0;
        short_name.resize(59);
        expect(decode_character_slot(short_name).name == "Genes",
               "Legacy short character-name tail");
        std::puts("Native world/character/cipher-transition checks passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
