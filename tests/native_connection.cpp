#include "connection.h"
#include "support/loopback.h"
#include "crypto/sha256.h"
#include <cstdio>

using namespace genesis::net;
namespace {
void expect(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        fixture::Endpoint login_server, world_server;
        Connection client;
        // Identity public exponent is deliberately limited to this isolated
        // fixture: the reply reveals the synthetic random input, so the peer
        // can independently derive the real HMAC/AES transport without secrets.
        std::array<std::uint8_t, 64> exponent = {1};
        std::array<std::uint8_t, 128> modulus;
        modulus.fill(255);
        Writer challenge;
        challenge.u8(0);
        challenge.u8(1);
        challenge.u8(128);
        challenge.append(exponent.data(), exponent.size());
        challenge.append(modulus.data(), modulus.size());
        login_server.send({0xa101, challenge.finish()});
        std::array<std::uint8_t, 32> material = {};
        std::array<std::uint8_t, 16> identity;
        identity.fill(0x42);
        bool authenticated = false, selected = false, world_connected = false;
        client.connect("127.0.0.1", login_server.port(), world_server.port());
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline) {
            client.poll(1);
            expect(client.phase() != Connection::Phase::failed, client.error().c_str());
            login_server.pump();
            world_server.pump();
            while (auto packet = login_server.wire.pop()) {
                if (packet->opcode == 0xa101) {
                    expect(packet->body.size() == 129, "Synthetic RSA reply framing");
                    hmac_sha256(material.data(), packet->body.data() + 1, 128, modulus.data(), 128);
                    login_server.wire.crypto.aes_material(material);
                } else if (packet->opcode == 0xa102) {
                    Reader fields(packet->body);
                    expect(fields.text(32) == "fixture" && fields.text(16) == "synthetic",
                           "Socket login credentials");
                    Writer result;
                    result.u8(0);
                    result.u32(42);
                    result.u8(11);
                    result.append(identity.data(), 16);
                    login_server.send({0xa102, result.finish()});
                    Writer list;
                    list.u8(1);
                    list.u8(1);
                    list.u8(0);
                    list.u16(0);
                    list.u16(100);
                    list.text("Genesis", 32);
                    login_server.send({0xa201, list.finish()});
                } else if (packet->opcode == 0xa202) {
                    expect(packet->body == Bytes({1, 0, 0x22, 0x12, 0x11}),
                           "Socket world selection");
                    login_server.send({0xa202, {0, 127, 0, 0, 1}});
                } else
                    throw std::runtime_error("Unexpected login fixture opcode");
            }
            while (auto packet = world_server.wire.pop()) {
                expect(packet->opcode == 0xa301, "World socket must begin with plaintext identity");
                Reader fields(packet->body);
                expect(fields.u32() == 42 && fields.fixed<16>() == identity,
                       "Identity token survived server transition");
                std::array<std::uint8_t, 32> digest;
                sha256_digest(digest.data(), material.data() + 16, 16);
                std::copy_n(digest.data(), 16, material.begin() + 16);
                world_server.wire.crypto.aes_material(material);
                Writer result;
                result.u8(0);
                result.u8(3);
                for (int i = 0; i < 16; ++i)
                    result.u8(0);
                world_server.send({0xa301, result.finish()});
                world_server.send({0x0101, {0, 0, 0, 0, 0}});
                world_server.send({0x0109, {1, 3}});
                world_connected = true;
            }
            if (auto login = client.login()) {
                if (login->state() == Login::State::ready && !authenticated) {
                    login->authenticate("fixture", "synthetic");
                    authenticated = true;
                }
                if (login->state() == Login::State::authenticated && !login->servers().empty() &&
                    !selected) {
                    login->select_server(1, 0x11122200);
                    selected = true;
                }
            }
            if (client.world() && client.world()->state() == World::State::characters &&
                client.world()->characters().count(0) && client.world()->faction() == 1)
                break;
        }
        expect(authenticated && selected && world_connected &&
                   client.phase() == Connection::Phase::world && !client.login() &&
                   client.world() && client.world()->characters().count(0) &&
                   client.world()->max_mode() == 3,
               "Native end-to-end login/world socket transition timed out");
        client.disconnect();
        expect(client.phase() == Connection::Phase::offline && !client.world() && !client.login(),
               "Disconnect released session ownership");
        std::puts("Native two-server session/socket transition passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
