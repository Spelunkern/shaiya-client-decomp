#pragma once
#include "login.h"
#include "player.h"
#include <map>

namespace genesis::net {
struct CharacterSlot {
    std::uint8_t slot = 0;
    std::uint32_t id = 0, delete_time = 0;
    std::uint16_t level = 0, map = 0;
    std::uint8_t race = 0, mode = 0, hair = 0, face = 0, height = 0, job = 0, gender = 0;
    std::array<std::uint16_t, 6> stats = {};
    std::array<std::uint16_t, 3> resources = {};
    std::array<std::uint8_t, 8> equipment_types = {}, equipment_ids = {};
    std::string name;
    std::uint8_t pending_delete = 0, rename_pending = 0;
    std::array<std::uint8_t, 6> extra_equipment = {};
};
// 0058d860: slot/id prefix, conditional occupied-slot fields, 21-byte name/flags.
CharacterSlot decode_character_slot(const Bytes& body);
Packet world_handshake(const LoginAccount& account);
// 00452xxx world loading formats the numeric map ID as "%d.wld".
std::string world_map_path(std::uint16_t map);

class World {
  public:
    enum class State { await_handshake, characters, selecting_character, loading, rejected };
    World(Wire&& wire, LoginAccount account);
    void receive(const void* bytes, std::size_t count);
    Bytes take_outgoing();
    std::vector<Packet> take_events();
    void select_character(std::uint32_t id);
    State state() const {
        return state_;
    }
    std::uint8_t status() const {
        return status_;
    }
    std::uint32_t selected_character() const {
        return selected_;
    }
    std::optional<std::string> selected_map_path() const {
        if (!player_)
            return std::nullopt;
        return world_map_path(player_->map);
    }
    const std::map<std::uint8_t, CharacterSlot>& characters() const {
        return characters_;
    }
    std::uint8_t faction() const {
        return faction_;
    }
    std::uint8_t max_mode() const {
        return max_mode_;
    }
    const auto& quick_slots() const {
        return quick_slots_;
    }
    const PlayerState* player() const {
        return player_ ? &*player_ : nullptr;
    }

  private:
    void dispatch(Packet packet);
    Wire wire_;
    State state_ = State::await_handshake;
    std::uint8_t status_ = 0, faction_ = 0xff, max_mode_ = 0;
    std::uint32_t selected_ = 0;
    std::optional<PlayerState> player_;
    std::map<std::uint8_t, CharacterSlot> characters_;
    std::vector<std::array<std::uint8_t, 5>> quick_slots_;
    Bytes outgoing_;
    std::vector<Packet> events_;
    std::size_t event_bytes_ = 0;
};
} // namespace genesis::net
