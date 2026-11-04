#include "wire.h"
#include "crypto/transport.h"
#include <cstdio>
#include <cstring>

// The reference API retains its original global state. The native API below
// owns state per connection and is compared against this production reference.
extern "C" {
std::uint8_t dat_02251fe0[4096] = {};
std::uint32_t dat_02252fe4 = 0;
std::uint8_t dat_02252fe8 = 0, dat_02252fe9 = 0, dat_02252fea = 0;
}
using namespace genesis::net;
void expect(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
int main() {
    try {
        Wire sender, receiver;
        Packet first{0xa102, {1, 2, 3, 4, 5}}, second{0xa201, {7, 8, 9}};
        auto plain = sender.encode(first);
        expect(plain == Bytes({9, 0, 2, 0xa1, 1, 2, 3, 4, 5}), "Wire bytes changed");
        for (std::size_t split = 0; split <= plain.size(); ++split) {
            Wire fragmented;
            fragmented.feed(plain.data(), split);
            auto packet = fragmented.pop();
            expect(bool(packet) == (split == plain.size()),
                   "Fragmented header/payload decoded too early");
            fragmented.feed(plain.data() + split, plain.size() - split);
            if (!packet)
                packet = fragmented.pop();
            expect(packet && packet->opcode == first.opcode && packet->body == first.body &&
                       !fragmented.pop(),
                   "Fragmented frame was lost");
        }
        std::array<std::uint8_t, 32> material;
        for (std::size_t i = 0; i < material.size(); ++i)
            material[i] = static_cast<std::uint8_t>(i * 7);
        sender.crypto.aes_material(material);
        receiver.crypto.aes_material(material);
        auto encrypted = sender.encode(first), encrypted_second = sender.encode(second);
        ClientCryptoState reference = {};
        initialize_transport_aes_material(&reference, material.data());
        Bytes expected = plain;
        transform_outbound_payload(&reference, expected.data() + 2,
                                   static_cast<std::uint32_t>(expected.size() - 2));
        expect(expected == encrypted, "Native AES wire bytes differ from recovered sender");
        encrypted.insert(encrypted.end(), encrypted_second.begin(), encrypted_second.end());
        receiver.feed(encrypted.data(), encrypted.size());
        auto a = receiver.pop(), b = receiver.pop();
        expect(a && b && a->body == first.body && b->body == second.body && !receiver.pop(),
               "Coalesced encrypted packets");
        // Crypto enabled between packets already present in one received buffer.
        Wire initial, transition, after;
        after.crypto.aes_material(material);
        auto mixed = initial.encode({0xa101, {0}});
        auto cipher = after.encode(first);
        mixed.insert(mixed.end(), cipher.begin(), cipher.end());
        transition.feed(mixed.data(), mixed.size());
        expect(transition.pop()->opcode == 0xa101, "Initial plaintext handshake");
        transition.crypto.aes_material(material);
        expect(transition.pop()->body == first.body, "Per-packet crypto transition");
        // Independent receive/send directions and independent connections.
        auto reverse = receiver.encode(second);
        sender.feed(reverse.data(), reverse.size());
        expect(sender.pop()->body == second.body, "Independent AES direction counters");
        Wire unrelated;
        unrelated.crypto.aes_material(material);
        expect(unrelated.encode(first) == expected, "Crypto leaked between connections");
        Wire moved = std::move(sender);
        auto continued = moved.encode(first);
        receiver.feed(continued.data(), continued.size());
        expect(receiver.pop()->body == first.body, "Moving session lost stream position");
        Wire moved_receive = std::move(receiver);
        continued = moved.encode(second);
        moved_receive.feed(continued.data(), continued.size());
        expect(moved_receive.pop()->body == second.body, "Moving receive buffer lost its cursor");
        for (std::uint8_t length = 0; length < 4; ++length) {
            Wire malformed;
            Bytes header = {length, 0};
            malformed.feed(header.data(), header.size());
            try {
                (void)malformed.pop();
                throw std::logic_error("Accepted invalid frame length");
            } catch (const std::runtime_error&) {
            }
        }
        // Full 16-bit length, without inherited 4 KiB stack buffers.
        Wire large_sender, large_receiver;
        Packet large{0x102, Bytes(65531, 0x51)};
        auto large_frame = large_sender.encode(large);
        large_receiver.feed(large_frame.data(), large_frame.size());
        expect(large_receiver.pop()->body == large.body, "Maximum wire frame");
        large.body.push_back(0);
        try {
            (void)large_sender.encode(large);
            throw std::logic_error("Accepted overflowing frame");
        } catch (const std::invalid_argument&) {
        }
        // Recover known mode-two table bytes and reject its undefined spill.
        Crypto mode;
        std::array<std::uint8_t, 16> seed = {};
        mode.xor_seed(2, seed);
        mode.alternate_inbound(true);
        Bytes data(2048, 0);
        mode.inbound(data.data(), data.size());
        mode.inbound(data.data(), data.size());
        expect(data == Bytes(2048, 0), "Mode-two XOR symmetry");
        data.resize(2049);
        try {
            mode.inbound(data.data(), data.size());
            throw std::logic_error("Accepted undefined mode-two table spill");
        } catch (const std::runtime_error&) {
        }
        std::puts("Native wire checks passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
