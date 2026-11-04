#pragma once
#include "wire.h"
#include <functional>

namespace genesis::net {
class World;
struct LoginAccount {
    std::uint32_t id = 0;
    std::uint8_t auth_status = 0;
    std::array<std::uint8_t, 16> identity = {};
};
struct LoginServer {
    std::uint8_t id = 0, status = 0;
    std::uint16_t users = 0, capacity = 0;
    std::string name;
};
class Login {
  public:
    enum class State {
        await_challenge,
        ready,
        authenticating,
        authenticated,
        selecting,
        selected,
        rejected,
        transferred
    };
    explicit Login(std::function<std::uint32_t()> seed = {});
    void receive(const void* bytes, std::size_t count);
    Bytes take_outgoing();
    void authenticate(std::string_view username, std::string_view password);
    void select_server(std::uint8_t id, std::uint32_t version);
    World enter_world();
    State state() const {
        return state_;
    }
    std::uint8_t status() const {
        return status_;
    }
    const LoginAccount& account() const {
        return account_;
    }
    const std::vector<LoginServer>& servers() const {
        return servers_;
    }
    const std::array<std::uint8_t, 4>& world_address() const {
        return world_address_;
    }

  private:
    void dispatch(const Packet& packet);
    void send(const Packet& packet);
    Wire wire_;
    std::function<std::uint32_t()> seed_;
    State state_ = State::await_challenge;
    std::uint8_t status_ = 0;
    LoginAccount account_;
    std::vector<LoginServer> servers_;
    std::array<std::uint8_t, 4> world_address_ = {};
    Bytes outgoing_;
};
// Recovered FUN_005992f0's standard branch: 32-byte account and 16-byte password.
Packet login_request(std::string_view username, std::string_view password);
// Recovered FUN_00599440: one world ID byte and a little-endian version DWORD.
Packet server_selection(std::uint8_t id, std::uint32_t version);
} // namespace genesis::net
