#pragma once
#include "graphics/render_types.h"
#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace genesis {
struct StaticMesh {
    std::string texture;
    std::vector<shaiya::graphics::native::MeshVertex> vertices;
    std::vector<std::uint16_t> indices;
};
struct StaticModel {
    std::array<float, 4> sphere;
    std::array<float, 3> minimum, maximum;
    std::vector<StaticMesh> meshes;
    std::vector<std::uint8_t> trailing_data;
};
// Replaces the pointer/offset owners around FUN_005aea00 and FUN_005ae780.
// Serialized floats/indices are little endian; runtime vectors are host-native.
StaticModel decode_smod(const std::vector<std::uint8_t>& bytes);
// The collision section that follows the meshes (`005aebf0`): a 24-byte bounds
// box, a flag word and, when the flag is non-zero, a vertex count with 12-byte
// vertices and a triangle count with three 16-bit indices each. Returns an
// empty mesh when the model has no collision data; throws on truncation.
struct ModelCollision {
    std::array<float, 3> minimum{}, maximum{};
    std::vector<std::array<float, 3>> vertices;
    std::vector<std::uint16_t> indices;
};
ModelCollision decode_model_collision(const StaticModel& model);
} // namespace genesis
