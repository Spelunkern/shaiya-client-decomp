#pragma once
#include "graphics/render_types.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace genesis {
struct Bounds {
    std::array<float, 3> minimum{}, maximum{};
};
struct DungeonMesh {
    std::uint32_t texture = 0, lightmap = 0;
    std::vector<shaiya::graphics::native::LightmapVertex> vertices;
    std::vector<std::uint16_t> indices;
};
struct DungeonNode {
    std::array<float, 3> center{};
    Bounds view, collision;
    std::vector<DungeonMesh> meshes;
    std::vector<std::array<float, 3>> collision_vertices;
    std::vector<std::uint16_t> collision_indices;
    // Index into Dungeon::nodes; -1 denotes an absent octant.
    std::array<std::int32_t, 8> children{{-1, -1, -1, -1, -1, -1, -1, -1}};
};
struct Dungeon {
    Bounds bounds;
    std::vector<std::string> textures;
    std::uint32_t lightmaps = 0;
    std::vector<DungeonNode> nodes;
};
// DG files retain their octree, lightmap UVs and node-local collision indices.
Dungeon decode_dungeon(const std::vector<std::uint8_t>& bytes);
} // namespace genesis
