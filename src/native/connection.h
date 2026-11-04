#pragma once
#include "login.h"
#include "world.h"
#include "tcp.h"
#include <memory>

namespace genesis::net {
// UI-facing session owner. One poll per application tick; no Win32 message
// window, inherited callback table, suspended stack frame or blocking connect.
class Connection {
  public:
    enum class Phase { offline, login, world, closed, failed };
    void connect(const std::string& host, std::uint16_t login_port, std::uint16_t world_port);
    void disconnect();
    void poll(int timeout_ms = 0);
    Phase phase() const {
        return phase_;
    }
    const std::string& error() const {
        return error_;
    }
    Login* login() {
        return login_.get();
    }
    World* world() {
        return world_.get();
    }

  private:
    Tcp socket_;
    std::unique_ptr<Login> login_;
    std::unique_ptr<World> world_;
    Phase phase_ = Phase::offline;
    std::uint16_t world_port_ = 0;
    std::string error_;
};
} // namespace genesis::net
