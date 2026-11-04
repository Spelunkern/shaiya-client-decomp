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
} // namespace genesis
