#include "login.h"
#include "world.h"
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <algorithm>

namespace genesis::net {
namespace {
std::uint32_t random_seed() {
    std::uint32_t seed = 0;
    if (RAND_bytes(reinterpret_cast<unsigned char*>(&seed), sizeof(seed)) != 1)
        throw std::runtime_error("Cannot seed client handshake");
    return seed;
}
} // namespace
Packet login_request(std::string_view username, std::string_view password) {
    if (username.empty() || password.empty())
        throw std::invalid_argument("Account and password cannot be empty");
    Writer w;
    w.text(username, 32);
    w.text(password, 16);
    return {0xa102, w.finish()};
}
Packet server_selection(std::uint8_t id, std::uint32_t version) {
    Writer w;
    w.u8(id);
    w.u32(version);
    return {0xa202, w.finish()};
}
Login::Login(std::function<std::uint32_t()> seed) : seed_(seed ? std::move(seed) : random_seed) {}
void Login::send(const Packet& packet) {
    auto frame = wire_.encode(packet);
    outgoing_.insert(outgoing_.end(), frame.begin(), frame.end());
}
Bytes Login::take_outgoing() {
    Bytes result;
    result.swap(outgoing_);
    return result;
}
void Login::authenticate(std::string_view username, std::string_view password) {
    if (state_ != State::ready && state_ != State::rejected)
        throw std::logic_error("Login handshake is not ready");
    auto packet = login_request(username, password);
    send(packet);
    OPENSSL_cleanse(packet.body.data(), packet.body.size());
    state_ = State::authenticating;
    status_ = 0;
}
void Login::select_server(std::uint8_t id, std::uint32_t version) {
    if (state_ != State::authenticated)
        throw std::logic_error("Not authenticated");
    if (std::none_of(servers_.begin(), servers_.end(),
                     [&](const LoginServer& server) { return server.id == id; }))
        throw std::invalid_argument("Selected server is not in the received list");
    send(server_selection(id, version));
    state_ = State::selecting;
}
void Login::receive(const void* bytes, std::size_t count) {
    if (state_ == State::transferred)
        throw std::logic_error("Login session already transferred to world");
    wire_.feed(bytes, count);
    while (auto packet = wire_.pop())
        dispatch(*packet);
}
World Login::enter_world() {
    if (state_ != State::selected || !outgoing_.empty())
        throw std::logic_error("World selection is not complete");
    World world(std::move(wire_), account_);
    state_ = State::transferred;
    return world;
}
void Login::dispatch(const Packet& packet) {
    Reader r(packet.body);
    switch (packet.opcode) {
    case 0xa101: {
        if (state_ != State::await_challenge)
            throw std::runtime_error("Unexpected repeated A101 challenge");
        auto reply = wire_.crypto.challenge_reply(packet.body, seed_());
        send({0xa101, std::move(reply)}); // the reply itself is plaintext
        wire_.crypto.enable();
        state_ = State::ready;
        return;
    }
    case 0xa102: {
        if (state_ != State::authenticating)
            throw std::runtime_error("Unexpected login result");
        auto status = r.u8();
        if (status) {
            status_ = status;
            state_ = State::rejected;
            return;
        }
        LoginAccount account;
        account.id = r.u32();
        account.auth_status = r.u8();
        account.identity = r.fixed<16>();
        if (r.remaining())
            throw std::runtime_error("Unexpected login-result length");
        account_ = account;
        status_ = 0;
        state_ = State::authenticated;
        return;
    }
    case 0xa201: {
        if (state_ != State::authenticated && state_ != State::selecting)
            throw std::runtime_error("Server list before authentication");
        auto count = r.u8();
        if (r.remaining() != std::size_t(count) * 38)
            throw std::runtime_error("Invalid server-list length");
        std::vector<LoginServer> servers;
        servers.reserve(count);
        for (unsigned i = 0; i < count; ++i) {
            LoginServer server;
            server.id = r.u8();
            server.status = r.u8();
            server.users = r.u16();
            server.capacity = r.u16();
            server.name = r.text(32);
            servers.push_back(std::move(server));
        }
        servers_ = std::move(servers);
        return;
    }
    case 0xa202: {
        if (state_ != State::selecting)
            throw std::runtime_error("Unexpected server-selection result");
        auto status = r.u8();
        if (status) {
            status_ = status;
            state_ = State::authenticated;
            return;
        }
        auto address = r.fixed<4>();
        if (r.remaining())
            throw std::runtime_error("Unexpected world-address length");
        world_address_ = address;
        status_ = 0;
        state_ = State::selected;
        return;
    }
    default:
        throw std::runtime_error("Unported login packet opcode " + std::to_string(packet.opcode));
    }
}
} // namespace genesis::net
