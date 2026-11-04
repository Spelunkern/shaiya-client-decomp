#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace genesis {
struct Action {
    std::string animation;
    // The client reads four bytes but tests only the low control byte. Upper
    // bytes in real assets include nonzero padding and must not be mode bits.
    std::uint32_t control = 0, next = 0;
    std::array<float, 4> parameters{};
    std::uint8_t mode() const {
        return std::uint8_t(control);
    }
};
struct ActionTable {
    std::vector<Action> slots;
};
// 00552120: AL3, sparse indexed animation names, then six authored dwords.
// Empty slots have only their zero name-length word; do not compact indices.
ActionTable decode_actions(const std::vector<std::uint8_t>& bytes);
} // namespace genesis
