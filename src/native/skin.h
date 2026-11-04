#pragma once
#include "graphics/render_types.h"
#include <cstdint>
#include <vector>
namespace genesis {
struct SkinnedModel {
    std::uint32_t version = 0;
    std::vector<shaiya::graphics::native::Matrix> inverse_bind;
    std::vector<shaiya::graphics::native::SkinVertex> vertices;
    std::vector<std::uint16_t> indices;
    std::size_t referenced_bones = 0;
};
// Standard 40-byte/two-bone character mesh. Cloaks and headerless monster
// meshes have distinct contracts and must not be guessed from this decoder.
SkinnedModel decode_skin(const std::vector<std::uint8_t>& bytes);
// 00550fb0 / 005514a0: inverse bind is multiplied by the animated final
// transform before the two influences are blended, in row-vector order.
std::vector<shaiya::graphics::native::Matrix>
skin_palette(const SkinnedModel& model,
             const std::vector<shaiya::graphics::native::Matrix>& skeleton_finals);
} // namespace genesis
