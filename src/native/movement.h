#pragma once
#include "packet.h"
#include <array>
#include <cmath>
#include <optional>
#include <cstdint>

namespace genesis::net {
// 0058d760: the state byte is the caller's value, with 0x80 added when the
// fourth argument is non-zero and 0x40 when the third is.
std::uint8_t movement_state_byte(std::uint8_t state, bool flag_40, bool flag_80);
// 00598980: opcode 0x0501, body = state byte, 16-bit orientation, x, y, z
// (little-endian floats; 15 body bytes, 17 with the opcode). The original
// sends nothing when any coordinate is not finite; that is reported here with
// std::nullopt rather than an exception.
std::optional<Packet> encode_move_packet(std::uint8_t state, std::uint16_t orientation,
                                         const std::array<float, 3>& position);
// 00598ab0 / 00598b00: opcodes 0x0502 and 0x0503 carry a 32-bit target id.
Packet encode_move_target_packet(std::uint16_t opcode, std::uint32_t target);
// 0043bfa0: forward movement speed in world units per second. The base
// (`00719880`) is initialised to 5.0 by `004092a7`. The speed class byte at
// player +0x281 shifts it by -2, -1, 0, +1, +2 for classes 0..4 and zeroes it
// for 0xff; a mounted actor (+900 non-zero) uses 7.0, or 8.0 when its mount
// kind (+0x388) is 1.
float movement_speed(std::uint8_t speed_class, bool mounted, std::uint32_t mount_kind,
                     float base = 5.f);
} // namespace genesis::net
