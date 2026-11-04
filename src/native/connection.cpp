#include "connection.h"

namespace genesis::net {
void Connection::connect(const std::string& host, std::uint16_t login_port,
                         std::uint16_t world_port) {
    if (!world_port)
        throw std::invalid_argument("Invalid world port");
    disconnect();
    world_port_ = world_port;
    login_ = std::make_unique<Login>();
    try {
        socket_.connect_ipv4(host, login_port);
        phase_ = Phase::login;
    } catch (const std::exception& e) {
        phase_ = Phase::failed;
        error_ = e.what();
    }
}
void Connection::disconnect() {
    socket_.close();
    world_.reset();
    login_.reset();
    phase_ = Phase::offline;
    error_.clear();
}
void Connection::poll(int timeout_ms) {
    if (phase_ != Phase::login && phase_ != Phase::world)
        return;
    try {
        auto received = socket_.poll(timeout_ms);
        if (phase_ == Phase::login) {
            if (!received.empty())
                login_->receive(received.data(), received.size());
            auto outgoing = login_->take_outgoing();
            if (!outgoing.empty())
                socket_.queue(outgoing);
            if (login_->state() == Login::State::selected && !socket_.queued_bytes()) {
                const auto address = login_->world_address();
                std::string host = std::to_string(address[0]) + "." + std::to_string(address[1]) +
                                   "." + std::to_string(address[2]) + "." +
                                   std::to_string(address[3]);
                world_ = std::make_unique<World>(login_->enter_world());
                login_.reset();
                socket_.connect_ipv4(host, world_port_);
                phase_ = Phase::world;
                socket_.queue(world_->take_outgoing());
            }
        } else {
            if (!received.empty())
                world_->receive(received.data(), received.size());
            auto outgoing = world_->take_outgoing();
            if (!outgoing.empty())
                socket_.queue(outgoing);
        }
        if (socket_.state() == Tcp::State::peer_closed)
            phase_ = Phase::closed;
    } catch (const std::exception& e) {
        socket_.close();
        phase_ = Phase::failed;
        error_ = e.what();
    }
}
} // namespace genesis::net
