#include "movement.h"
#include <cstring>

namespace genesis::net {
std::uint8_t movement_state_byte(std::uint8_t state, bool flag_40, bool flag_80) {
    if (flag_80)
        state |= 0x80;
    if (flag_40)
        state |= 0x40;
    return state;
}
std::optional<Packet> encode_move_packet(std::uint8_t state, std::uint16_t orientation,
                                         const std::array<float, 3>& position) {
    for (float value : position)
        if (!std::isfinite(value))
            return std::nullopt;
    Writer w;
    w.u8(state);
    w.u16(orientation);
    for (float value : position) {
        std::uint32_t bits;
        std::memcpy(&bits, &value, 4);
        w.u32(bits);
    }
    return Packet{0x0501, w.finish()};
}
Packet encode_move_target_packet(std::uint16_t opcode, std::uint32_t target) {
    if (opcode != 0x0502 && opcode != 0x0503)
        throw std::invalid_argument("Not a movement target opcode");
    Writer w;
    w.u32(target);
    return {opcode, w.finish()};
}
float movement_speed(std::uint8_t speed_class, bool mounted, std::uint32_t mount_kind, float base) {
    float speed = base;
    switch (speed_class) {
    case 0: speed = base - 2.f; break;
    case 1: speed = base - 1.f; break;
    case 3: speed = base + 1.f; break;
    case 4: speed = base + 2.f; break;
    case 0xff: speed = 0.f; break;
    default: break;
    }
    if (mounted)
        speed = mount_kind == 1 ? 8.f : 7.f;
    return speed;
}
} // namespace genesis::net
