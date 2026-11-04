#pragma once
#include "graphics/render_types.h"
#include <array>
#include <cstdint>
#include <vector>
namespace genesis {
template <std::size_t N> struct AnimationKey {
    std::int32_t frame = 0;
    std::array<float, N> value{};
};
struct AnimationBone {
    std::int32_t parent = -1;
    shaiya::graphics::native::Matrix authored{};
    std::vector<AnimationKey<4>> rotations;
    std::vector<AnimationKey<3>> translations;
};
struct Animation {
    std::int32_t start = 0, end = 0;
    std::vector<AnimationBone> bones;
};
// Retains authored global matrices, hierarchy and independent key channels.
// The client's 00552910 consumes 20-byte quaternion and 16-byte position keys.
Animation decode_animation(const std::vector<std::uint8_t>& bytes);
} // namespace genesis
