#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "graphics/render_types.h"
namespace genesis {
struct ClothVertex {
    std::array<float, 3> position{}, normal{};
    std::array<float, 2> uv{};
    // Disk offsets 12 and 16: copied by the original loader. These are NOT
    // skin palette indices. Keep bits until the simulation consumers use them.
    std::array<std::uint32_t, 2> attributes{};
};
struct ClothMesh {
    std::string embedded_name;
    std::vector<ClothVertex> vertices;
    std::vector<std::uint16_t> indices;
};
struct ClothMeshReference {
    std::uint32_t id = 0;
    std::string name;
};
struct ClothAnchor {
    std::uint32_t control = 0; // Only the low byte is tested against one.
    std::int32_t vertex = 0, bone = 0;
    std::array<float, 3> offset{};
    bool enabled() const {
        return (control & 255) == 1;
    }
};
struct ClothSetup {
    std::uint32_t dynamic_mesh = 0, texture = 0, mode = 0;
    std::int32_t static_mesh = -1;
    std::uint32_t columns = 0, rows = 0;
    std::array<std::uint32_t, 4> parameters{};
    std::uint32_t reserved = 0;
    std::array<ClothAnchor, 20> anchors{};
};
struct ClothCatalog {
    std::vector<ClothMeshReference> dynamic_meshes, static_meshes;
    std::vector<ClothSetup> simulation;
};
struct ClothPin {
    std::size_t vertex = 0;
    std::array<float, 3> position{};
};
// 00415e33 / 00416000: prefix of enabled anchors, skipping signed-negative
// vertex/bone values. Offsets are transformed by final bone matrices directly.
std::vector<ClothPin>
cloth_pins(const ClothSetup& setup,
           const std::vector<shaiya::graphics::native::Matrix>& skeleton_finals);
// 005825e0: dynamic cloth has no inverse-bind matrix catalog.
ClothMesh decode_cloth_mesh(const std::vector<std::uint8_t>& bytes);
// 00585770: three counted groups with widths 0x84 / 0x20c / 0x84.
ClothCatalog decode_cloth_catalog(const std::vector<std::uint8_t>& bytes);
// 00584d50/00584e80/00584fb0/005850e0: signed positive count, 256-byte names.
std::vector<std::string> decode_cloth_textures(const std::vector<std::uint8_t>& bytes);
struct ClothTexturePaths {
    std::string catalog, directory;
};
// 00414884's actual jump table: Human, Elf, Vile, DeathEater.
ClothTexturePaths cloth_texture_paths(std::uint8_t race);
} // namespace genesis
