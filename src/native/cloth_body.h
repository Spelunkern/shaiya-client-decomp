#pragma once
#include "graphics/render_types.h"
#include <array>
#include <cstdint>
#include <vector>
namespace genesis {
struct ClothSphere {
    std::array<float, 3> center{};
    float radius = 0;
};
using ClothBody = std::array<ClothSphere, 4>;
// 004161d3..00416267: clear translation, transform local +Z, normalize,
// negate. Caller supplies the actor's composed orientation matrix.
std::array<float, 3>
cloth_correction_direction(const shaiya::graphics::native::Matrix& orientation);
// Bone records are 0x118 bytes, final matrix at 0xc0: 0x520 is bone four.
std::array<float, 3>
cloth_correction_direction(const std::vector<shaiya::graphics::native::Matrix>& skeleton_finals,
                           const shaiya::graphics::native::Matrix& world);
// 00583370 shape preparation: final bone transforms precede the actor/world
// transform. Radius zero is retained, not removed from the sphere sequence.
ClothBody cloth_body(std::uint8_t kind,
                     const std::vector<shaiya::graphics::native::Matrix>& skeleton_finals,
                     const shaiya::graphics::native::Matrix& world);
} // namespace genesis
