#include "login.h"
#include "world.h"
#include "fixtures/native_a101.h"
#include <cstdio>

using namespace genesis::net;
namespace {
void expect(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
Packet challenge() {
    Writer w;
    w.u8(0);
    w.u8(3);
    w.u8(128);
    std::array<std::uint8_t, 64> exponent = {1, 0, 1};
    w.append(exponent.data(), exponent.size());
    w.append(a101_fixture::modulus.data(), 128);
    return {0xa101, w.finish()};
}
void handshake(Login& client, Wire& server) {
    Wire plain;
    auto frame = plain.encode(challenge());
    client.receive(frame.data(), frame.size());
    expect(client.state() == Login::State::ready, "Handshake did not enable authentication");
    auto response = client.take_outgoing();
    plain.feed(response.data(), response.size());
    auto packet = plain.pop();
    expect(packet && packet->opcode == 0xa101 && packet->body.size() == 129 &&
               packet->body[0] == 128,
           "A101 response layout");
    expect(std::equal(a101_fixture::reply.begin(), a101_fixture::reply.end(),
                      packet->body.begin() + 1),
           "RSA differs from independent Python pow fixture");
    server.crypto.aes_material(a101_fixture::material);
}
} // namespace
int main() {
    try {
        expect(login_request(std::string(32, 'u'), std::string(16, 'p')).body.size() == 48,
               "Full-width credential fields");
        Login client([] { return 0x12345678u; });
        Wire server;
        handshake(client, server);
        client.authenticate("genesis-test", "synthetic");
        auto frame = client.take_outgoing();
        server.feed(frame.data(), frame.size());
        auto request = server.pop();
        expect(request && request->opcode == 0xa102 && request->body.size() == 48,
               "A102 recovered frame size");
        Reader credentials(request->body);
        expect(credentials.text(32) == "genesis-test" && credentials.text(16) == "synthetic",
               "A102 account/password offsets or derived HMAC material");
        auto failure = server.encode({0xa102, {1}});
        client.receive(failure.data(), failure.size());
        expect(client.state() == Login::State::rejected && client.status() == 1,
               "Rejected login transition");
        client.authenticate("genesis-test", "retry");
        frame = client.take_outgoing();
        server.feed(frame.data(), frame.size());
        expect(server.pop()->opcode == 0xa102, "Retry lost stream position");
        Writer success;
        success.u8(0);
        success.u32(0x12345678);
        success.u8(11);
        std::array<std::uint8_t, 16> identity;
        identity.fill(0x77);
        success.append(identity.data(), 16);
        Writer list;
        list.u8(2);
        for (unsigned id = 1; id <= 2; ++id) {
            list.u8(id);
            list.u8(0);
            list.u16(7);
            list.u16(100);
            list.text(id == 1 ? "Genesis" : "Second", 32);
        }
        auto incoming = server.encode({0xa102, success.finish()}),
             servers = server.encode({0xa201, list.finish()});
        incoming.insert(incoming.end(), servers.begin(), servers.end());
        for (auto byte : incoming)
            client.receive(&byte, 1);
        expect(client.state() == Login::State::authenticated && client.account().id == 0x12345678 &&
                   client.account().identity == identity && client.account().auth_status == 11,
               "Typed authentication result");
        expect(client.servers().size() == 2 && client.servers()[0].name == "Genesis" &&
                   client.servers()[1].id == 2,
               "Server-list records");
        try {
            client.select_server(9, 0);
            throw std::runtime_error("Selected absent server");
        } catch (const std::invalid_argument&) {
        }
        client.select_server(2, 0x11122200);
        frame = client.take_outgoing();
        server.feed(frame.data(), frame.size());
        auto selected = server.pop();
        expect(selected && selected->opcode == 0xa202 &&
                   selected->body == Bytes({2, 0, 0x22, 0x12, 0x11}),
               "A202 version and world wire bytes");
        incoming = server.encode({0xa202, {0, 127, 0, 0, 1}});
        client.receive(incoming.data(), incoming.size());
        expect(client.state() == Login::State::selected &&
                   client.world_address() == std::array<std::uint8_t, 4>{127, 0, 0, 1},
               "World-address selection");
        auto world = client.enter_world();
        expect(client.state() == Login::State::transferred && world.take_outgoing().size() == 24,
               "Login-to-world ownership transfer");
        try {
            (void)client.enter_world();
            throw std::runtime_error("Reused a transferred login session");
        } catch (const std::logic_error&) {
        }
        for (std::size_t size = 0; size < 195; ++size) {
            Login malformed([] { return 1; });
            Wire plain;
            auto partial = challenge();
            partial.body.resize(size);
            auto truncated = plain.encode(partial);
            try {
                malformed.receive(truncated.data(), truncated.size());
                throw std::logic_error("Accepted truncated A101");
            } catch (const std::runtime_error&) {
            }
        }
        auto invalid = challenge();
        invalid.body[1] = 65;
        Login malformed;
        Wire plain;
        frame = plain.encode(invalid);
        try {
            malformed.receive(frame.data(), frame.size());
            throw std::logic_error("Accepted oversized RSA exponent");
        } catch (const std::runtime_error&) {
        }
        // The alternate recovered LCG algorithm uses the same RSA public reply
        // and HMAC material, but independent 8-word stream states.
        auto lcg_challenge = challenge();
        lcg_challenge.body[0] = 1;
        Wire left, right;
        auto left_reply = left.crypto.challenge_reply(lcg_challenge.body, 0x12345678);
        auto right_reply = right.crypto.challenge_reply(lcg_challenge.body, 0x12345678);
        expect(left_reply == right_reply &&
                   std::equal(a101_fixture::reply.begin(), a101_fixture::reply.end(),
                              left_reply.begin() + 1),
               "LCG RSA public reply");
        left.crypto.enable();
        right.crypto.enable();
        auto lcg_frame = left.encode({0x1234, {1, 2, 3, 4, 5, 6, 7}});
        right.feed(lcg_frame.data(), lcg_frame.size());
        expect(right.pop()->body == Bytes({1, 2, 3, 4, 5, 6, 7}), "LCG connection transport");
        left.crypto.reset_nonce();
        right.crypto.reset_nonce();
        lcg_frame = right.encode({0x1234, {9, 8, 7}});
        left.feed(lcg_frame.data(), lcg_frame.size());
        expect(left.pop()->body == Bytes({9, 8, 7}), "LCG world reconnect nonce reset");
        std::puts("Native A101/A102/A201/A202 login checks passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
