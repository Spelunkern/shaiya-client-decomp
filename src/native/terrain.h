#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <string>
#include "graphics/render_types.h"
namespace genesis {
// 005aef30 stores the scales and their single-precision reciprocals.
struct TerrainScale {
    float horizontal, vertical, offset, inverse_horizontal, inverse_vertical;
};
TerrainScale terrain_scale(float horizontal, float vertical, float offset);
// Startup 004443fb initializes the outdoor world with these three values.
TerrainScale default_terrain_scale();
// 005aefb0: truncate the world-size/spacing product, then add one.
std::uint32_t terrain_dimension(std::int32_t world_size, float spacing);
struct TerrainFilePrefix {
    std::uint32_t field = 0;
    std::size_t bytes_consumed = 8;
};
// 00451999..004519da: FLD marker followed by one little-endian word.
TerrainFilePrefix decode_terrain_prefix(const std::vector<std::uint8_t>& bytes);
// 005b3250 reads N*N little-endian 16-bit heights followed by N*N bytes
// of additional terrain data. Preserve that second block for later consumers.
struct TerrainGridData {
    std::vector<std::uint16_t> heights;
    std::vector<std::uint8_t> attributes;
    std::size_t bytes_consumed = 0;
};
TerrainGridData decode_terrain_grid(std::uint32_t extent, const std::vector<std::uint8_t>& bytes);
// 005a2d50: sample the byte grid in local coordinates; outside returns zero.
std::uint8_t terrain_attribute(const TerrainGridData& grid, std::uint32_t extent, float x, float z);
struct TerrainMaterialRecord {
    std::string texture, sound;
    std::uint32_t parameter_bits = 0;
};
std::vector<TerrainMaterialRecord> decode_terrain_materials(const std::vector<std::uint8_t>& bytes,
                                                            std::size_t offset);
const TerrainMaterialRecord* terrain_surface_at(const TerrainGridData& grid, std::uint32_t extent,
                                                const std::vector<TerrainMaterialRecord>& records,
                                                float x, float z, float inverse_horizontal);
struct TerrainFieldData {
    TerrainFilePrefix prefix;
    std::uint32_t dimension = 0;
    TerrainGridData grid;
    std::vector<TerrainMaterialRecord> materials;
};
// FLD stream: the grid read follows the eight-byte prefix for the field branch.
TerrainFieldData decode_terrain_field(const std::vector<std::uint8_t>& bytes, float spacing);
// 005b3360: inclusive coordinate clamp, but row pitch is extent itself.
// Keep the original address rule; do not silently change to extent+1.
class TerrainHeights {
  public:
    TerrainHeights(std::uint32_t extent, std::vector<std::uint16_t> samples);
    static TerrainHeights from_grid(std::uint32_t extent, const TerrainGridData& grid);
    static TerrainHeights from_grid(std::uint32_t extent, TerrainGridData&& grid);
    std::uint32_t extent() const {
        return extent_;
    }
    float sample(std::int32_t x, std::int32_t z) const;
    // 005b3720: four-triangle fan in each 4x4 cell. Outside is already
    // converted to the client's integer sample units by the caller.
    float height(float x, float z, std::int32_t outside = 0) const;
    // 005a2c30: X/Z are stored after scaling; height remains extended until
    // the vertical scale and base offset have been applied.
    float world_height(float x, float z, float horizontal_scale, float vertical_scale,
                       float vertical_offset, float inverse_vertical_scale) const;

  private:
    long double height_extended(float x, float z, std::int32_t outside) const;
    std::uint32_t extent_;
    std::vector<std::uint16_t> samples_;
};
struct TerrainCellMesh {
    std::array<shaiya::graphics::native::WorldVertex, 5> vertices{};
    std::array<std::uint16_t, 12> indices{};
};
// Geometry for one 4x4 height-query cell, before terrain materials are loaded.
TerrainCellMesh terrain_cell_mesh(const TerrainHeights& heights, std::uint32_t cell_x,
                                  std::uint32_t cell_z, const TerrainScale& scale);
// 005b3640: plane height at X/Z. Caller selects the containing triangle.
float terrain_triangle_height(const std::array<float, 3>& a, const std::array<float, 3>& b,
                              const std::array<float, 3>& c, float x, float z);
} // namespace genesis
