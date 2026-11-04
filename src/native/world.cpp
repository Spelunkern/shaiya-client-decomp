#include "world.h"
#include <algorithm>

namespace genesis::net {
std::string world_map_path(std::uint16_t map) {
    return "data/world/" + std::to_string(map) + ".wld";
}
CharacterSlot decode_character_slot(const Bytes& body) {
    Reader r(body);
    CharacterSlot slot;
    slot.slot = r.u8();
    slot.id = r.u32();
    if (!slot.id) {
        return slot;
    }
    slot.delete_time = r.u32();
    slot.level = r.u16();
    slot.race = r.u8();
    slot.mode = r.u8();
    slot.hair = r.u8();
    slot.face = r.u8();
    slot.height = r.u8();
    slot.job = r.u8();
    slot.gender = r.u8();
    slot.map = r.u16();
    for (auto& stat : slot.stats)
        stat = r.u16();
    for (auto& resource : slot.resources)
        resource = r.u16();
    slot.equipment_types = r.fixed<8>();
    slot.equipment_ids = r.fixed<8>();
    // The legacy bounded name read permits a short tail. Define missing bytes
    // as zero in the native representation instead of inheriting stack bytes.
    Bytes name_flags(21, 0);
    auto count = std::min<std::size_t>(r.remaining(), name_flags.size());
    if (count)
        std::copy_n(r.take(count), count, name_flags.begin());
    Reader text(name_flags);
    slot.name = text.text(19);
    slot.pending_delete = text.u8();
    slot.rename_pending = text.u8();
    // Extended equipment is requested by the eighth authored equipment type.
    if (slot.equipment_types[7] || r.remaining()) {
        slot.extra_equipment = r.fixed<6>();
        if (r.remaining())
            throw std::runtime_error("Unexpected extended character-slot fields");
    }
    return slot;
}
Packet world_handshake(const LoginAccount& account) {
    Writer w;
    w.u32(account.id);
    w.append(account.identity.data(), account.identity.size());
    return {0xa301, w.finish()};
}
World::World(Wire&& wire, LoginAccount account) : wire_(std::move(wire)) {
    wire_.reset(); // clears the old socket's frame buffer without copying keys
    wire_.crypto.reset_nonce();
    outgoing_ = wire_.encode(world_handshake(account)); // unencrypted A301 identity
    wire_.crypto.enable();
}
Bytes World::take_outgoing() {
    Bytes bytes;
    bytes.swap(outgoing_);
    return bytes;
}
std::vector<Packet> World::take_events() {
    std::vector<Packet> packets;
    packets.swap(events_);
    event_bytes_ = 0;
    return packets;
}
void World::select_character(std::uint32_t id) {
    if (state_ != State::characters)
        throw std::logic_error("Character selection is not ready");
    if (!id || std::none_of(characters_.begin(), characters_.end(), [&](const auto& entry) {
            return entry.second.id == id && !entry.second.pending_delete;
        }))
        throw std::invalid_argument("Character is missing or pending deletion");
    Writer w;
    w.u32(id);
    auto bytes = wire_.encode({0x0104, w.finish()});
    outgoing_.insert(outgoing_.end(), bytes.begin(), bytes.end());
    selected_ = id;
    state_ = State::selecting_character;
}
void World::send_packet(const Packet& packet) {
    if (state_ != State::loading)
        throw std::logic_error("Gameplay packets require a selected character");
    auto bytes = wire_.encode(packet);
    outgoing_.insert(outgoing_.end(), bytes.begin(), bytes.end());
}
void World::receive(const void* bytes, std::size_t count) {
    wire_.feed(bytes, count);
    while (auto packet = wire_.pop())
        dispatch(std::move(*packet));
}
void World::dispatch(Packet packet) {
    Reader r(packet.body);
    if (packet.opcode == 0xa301) {
        if (state_ != State::await_handshake)
            throw std::runtime_error("Repeated world handshake");
        auto status = r.u8();
        if (status) {
            status_ = status;
            state_ = State::rejected;
            return;
        }
        auto mode = r.u8();
        auto seed = r.fixed<16>();
        if (r.remaining())
            throw std::runtime_error("Invalid world handshake length");
        wire_.crypto.xor_seed(mode, seed);
        state_ = State::characters;
        return;
    }
    if (state_ == State::await_handshake || state_ == State::rejected)
        throw std::runtime_error("World packet before accepted handshake");
    switch (packet.opcode) {
    case 0x0101: {
        auto slot = decode_character_slot(packet.body);
        characters_[slot.slot] = std::move(slot);
        return;
    }
    case 0x0109: {
        auto faction = r.u8(), mode = r.u8();
        if (r.remaining())
            throw std::runtime_error("Invalid account-faction length");
        faction_ = faction;
        max_mode_ = mode;
        return;
    }
    case 0x0104: {
        if (state_ != State::selecting_character)
            throw std::runtime_error("Unexpected character-selection response");
        auto status = r.u8();
        auto id = r.u32();
        if (r.remaining() || id != selected_)
            throw std::runtime_error("Character-selection ID/length mismatch");
        status_ = status;
        state_ = status ? State::characters : State::loading;
        if(!status) {
            PlayerState player;player.id=selected_;
            for(const auto& entry:characters_) if(entry.second.id==selected_) {player.map=entry.second.map;break;}
            player_=std::move(player);
        }
        return;
    }
    case 0x010b: {
        auto count = r.u8();
        if (r.remaining() != std::size_t(count) * 5)
            throw std::runtime_error("Invalid quick-slot packet length");
        std::vector<std::array<std::uint8_t, 5>> slots;
        slots.reserve(count);
        for (unsigned i = 0; i < count; ++i)
            slots.push_back(r.fixed<5>());
        wire_.crypto.enter_world_mode();
        quick_slots_ = std::move(slots);
        return;
    }
    case 0x0107:
        wire_.crypto.leave_world_mode();
        state_ = State::characters;
        selected_ = 0;
        player_.reset();
        break;
    default:
        break;
    }
    if(player_ && apply_player_packet(*player_,packet)) return;
    // Preserve unported gameplay packets for the application dispatcher. A
    // successful handshake is not evidence that their behavior is implemented.
    if (packet.body.size() + 4 > 1024 * 1024 - event_bytes_)
        throw std::runtime_error("Unconsumed world-event queue limit exceeded");
    event_bytes_ += packet.body.size() + 4;
    events_.push_back(std::move(packet));
}
} // namespace genesis::net
