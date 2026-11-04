#include "movement.h"
#include <cstdio>
#include <limits>
#include <stdexcept>
using namespace genesis::net;
static void expect(bool ok, const char* what) {
    if (!ok)
        throw std::runtime_error(what);
}
int main() {
    try {
        expect(movement_state_byte(3, false, false) == 3, "plain state");
        expect(movement_state_byte(3, true, false) == 0x43, "0x40 flag");
        expect(movement_state_byte(3, false, true) == 0x83, "0x80 flag");
        expect(movement_state_byte(1, true, true) == 0xc1, "both flags");
        auto packet = encode_move_packet(0x41, 0x0123, {1.f, -2.f, 3.5f});
        expect(packet && packet->opcode == 0x0501 && packet->body.size() == 15, "move packet size");
        const Bytes expected = {0x41, 0x23, 0x01, 0, 0, 0x80, 0x3f, 0, 0, 0, 0xc0, 0, 0, 0x60, 0x40};
        expect(packet->body == expected, "move packet bytes");
        expect(!encode_move_packet(0, 0, {std::numeric_limits<float>::quiet_NaN(), 0, 0}),
               "NaN position was sent");
        expect(!encode_move_packet(0, 0, {0, std::numeric_limits<float>::infinity(), 0}),
               "infinite position was sent");
        auto target = encode_move_target_packet(0x0502, 0x01020304);
        expect(target.body == Bytes({4, 3, 2, 1}), "target packet bytes");
        bool rejected = false;
        try {
            encode_move_target_packet(0x0504, 1);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        expect(rejected, "wrong target opcode accepted");
        expect(movement_speed(2, false, 0) == 5.f && movement_speed(0, false, 0) == 3.f &&
                   movement_speed(1, false, 0) == 4.f && movement_speed(3, false, 0) == 6.f &&
                   movement_speed(4, false, 0) == 7.f && movement_speed(0xff, false, 0) == 0.f,
               "speed classes");
        expect(movement_speed(2, true, 0) == 7.f && movement_speed(2, true, 1) == 8.f &&
                   movement_speed(0xff, true, 5) == 7.f,
               "mounted speeds");
        std::puts("Native movement packet and speed contract passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
