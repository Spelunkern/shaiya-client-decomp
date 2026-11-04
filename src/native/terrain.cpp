#include "terrain.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
namespace genesis {
const TerrainMaterialRecord* terrain_surface_at(const TerrainGridData& grid, std::uint32_t extent,
                                                const std::vector<TerrainMaterialRecord>& records,
                                                float x, float z, float inverse_horizontal) {
    if (!std::isfinite(inverse_horizontal))
        throw std::invalid_argument("Invalid terrain horizontal scale");
    auto index =
        terrain_attribute(grid, extent, float(static_cast<long double>(x) * inverse_horizontal),
                          float(static_cast<long double>(z) * inverse_horizontal));
    return index < records.size() ? &records[index] : nullptr;
}
std::vector<TerrainMaterialRecord> decode_terrain_materials(const std::vector<std::uint8_t>& bytes,
                                                            std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4)
        throw std::invalid_argument("Truncated terrain material count");
    auto word = [&](std::size_t p) {
        return std::uint32_t(bytes[p]) | std::uint32_t(bytes[p + 1]) << 8 |
               std::uint32_t(bytes[p + 2]) << 16 | std::uint32_t(bytes[p + 3]) << 24;
    };
    auto count = word(offset);
    offset += 4;
    if (count > (bytes.size() - offset) / 516)
        throw std::invalid_argument("Truncated terrain material records");
    auto name = [&](std::size_t p) {
        auto first = bytes.begin() + p;
        auto end = std::find(first, first + 256, 0);
        return std::string(first, end);
    };
    std::vector<TerrainMaterialRecord> result;
    result.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i, offset += 516)
        result.push_back({name(offset), name(offset + 260), word(offset + 256)});
    return result;
}
std::uint32_t terrain_dimension(std::int32_t world_size, float spacing) {
    if (world_size <= 0 || !std::isfinite(spacing) || spacing <= 0)
        throw std::invalid_argument("Invalid terrain dimension inputs");
    auto scaled = (1.L / spacing) * world_size;
    if (scaled >= std::numeric_limits<std::int32_t>::max())
        throw std::invalid_argument("Terrain dimension exceeds original range");
    return std::uint32_t(std::int32_t(scaled) + 1);
}
TerrainFilePrefix decode_terrain_prefix(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < 8 || bytes[0] != 'F' || bytes[1] != 'L' || bytes[2] != 'D' || bytes[3])
        throw std::invalid_argument("Invalid or truncated FLD prefix");
    TerrainFilePrefix result;
    result.field = std::uint32_t(bytes[4]) | std::uint32_t(bytes[5]) << 8 |
                   std::uint32_t(bytes[6]) << 16 | std::uint32_t(bytes[7]) << 24;
    return result;
}
WorldKind decode_world_kind(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() >= 4 && !bytes[3]) {
        if (bytes[0] == 'F' && bytes[1] == 'L' && bytes[2] == 'D')
            return WorldKind::field;
        if (bytes[0] == 'D' && bytes[1] == 'U' && bytes[2] == 'N')
            return WorldKind::dungeon;
    }
    throw std::invalid_argument("Unknown world descriptor tag");
}
DungeonDescriptor decode_dungeon_descriptor(const std::vector<std::uint8_t>& bytes) {
    if (decode_world_kind(bytes) != WorldKind::dungeon)
        throw std::invalid_argument("World descriptor is not a dungeon");
    DungeonDescriptor result;
    if (bytes.size() < result.bytes_consumed)
        throw std::invalid_argument("Truncated dungeon name");
    auto first = bytes.begin() + 4;
    auto end = std::find(first, first + 0x100, 0);
    if (end == first + 0x100 || end == first)
        throw std::invalid_argument("Unterminated or empty dungeon name");
    result.name.assign(first, end);
    result.path = "data/world/dungeon/" + result.name;
    return result;
}
static TerrainGridData decode_grid_at(std::uint32_t extent, const std::vector<std::uint8_t>& bytes,
                                      std::size_t offset) {
    const auto count64 = std::uint64_t(extent) * extent;
    if (!extent || offset > bytes.size() || count64 > (bytes.size() - offset) / 3)
        throw std::invalid_argument("Truncated terrain grid");
    auto count = std::size_t(count64);
    TerrainGridData result;
    result.heights.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        result.heights.push_back(std::uint16_t(bytes[offset + 2 * i]) |
                                 std::uint16_t(bytes[offset + 2 * i + 1]) << 8);
    result.attributes.assign(bytes.begin() + offset + 2 * count,
                             bytes.begin() + offset + 3 * count);
    result.bytes_consumed = 3 * count;
    return result;
}
TerrainGridData decode_terrain_grid(std::uint32_t extent, const std::vector<std::uint8_t>& bytes) {
    return decode_grid_at(extent, bytes, 0);
}
std::uint8_t terrain_attribute(const TerrainGridData& grid, std::uint32_t extent, float x,
                               float z) {
    if (!extent || grid.attributes.size() != std::uint64_t(extent) * extent || !std::isfinite(x) ||
        !std::isfinite(z))
        throw std::invalid_argument("Invalid terrain attribute input");
    if (x < 0 || z < 0 || x >= float(extent) || z >= float(extent))
        return 0;
    return grid.attributes[std::size_t(z) * extent + std::size_t(x)];
}
TerrainFieldData decode_terrain_field(const std::vector<std::uint8_t>& bytes, float spacing) {
    TerrainFieldData result;
    result.prefix = decode_terrain_prefix(bytes);
    if (result.prefix.field > std::uint32_t(std::numeric_limits<std::int32_t>::max()))
        throw std::invalid_argument("Terrain world size exceeds signed range");
    result.dimension = terrain_dimension(std::int32_t(result.prefix.field), spacing);
    result.grid = decode_grid_at(result.dimension, bytes, result.prefix.bytes_consumed);
    return result;
}
TerrainScale terrain_scale(float horizontal, float vertical, float offset) {
    if (!std::isfinite(horizontal) || !std::isfinite(vertical) || !std::isfinite(offset) ||
        horizontal == 0 || vertical == 0)
        throw std::invalid_argument("Invalid terrain scale");
    return {horizontal, vertical, offset, float(1.L / horizontal), float(1.L / vertical)};
}
TerrainScale default_terrain_scale() {
    return terrain_scale(2.f, .02f, -200.f);
}
static long double triangle_extended(const std::array<float, 3>&, const std::array<float, 3>&,
                                     const std::array<float, 3>&, float, float);
TerrainHeights::TerrainHeights(std::uint32_t extent, std::vector<std::uint16_t> samples)
    : extent_(extent), samples_(std::move(samples)) {
    if (!extent || extent > std::uint32_t(std::numeric_limits<std::int32_t>::max()) ||
        std::uint64_t(extent) * extent > samples_.size())
        throw std::invalid_argument("Terrain height samples do not cover the grid");
}
TerrainHeights TerrainHeights::from_grid(std::uint32_t extent, const TerrainGridData& grid) {
    if (grid.attributes.size() != std::uint64_t(extent) * extent ||
        grid.heights.size() != grid.attributes.size())
        throw std::invalid_argument("Terrain grid dimension mismatch");
    return TerrainHeights(extent, grid.heights);
}
TerrainHeights TerrainHeights::from_grid(std::uint32_t extent, TerrainGridData&& grid) {
    if (grid.attributes.size() != std::uint64_t(extent) * extent ||
        grid.heights.size() != grid.attributes.size())
        throw std::invalid_argument("Terrain grid dimension mismatch");
    return TerrainHeights(extent, std::move(grid.heights));
}
TerrainCellMesh terrain_cell_mesh(const TerrainHeights& heights, std::uint32_t cell_x,
                                  std::uint32_t cell_z, const TerrainScale& scale) {
    if (std::uint64_t(cell_x) * 4 + 4 >= heights.extent() ||
        std::uint64_t(cell_z) * 4 + 4 >= heights.extent())
        throw std::invalid_argument("Terrain cell exceeds the height grid");
    TerrainCellMesh mesh;
    constexpr int x[5] = {0, 4, 4, 0, 2};
    constexpr int z[5] = {0, 0, 4, 4, 2};
    for (unsigned i = 0; i < 5; ++i) {
        auto gx = cell_x * 4 + x[i], gz = cell_z * 4 + z[i];
        auto& v = mesh.vertices[i];
        v.position[0] = float(gx) * scale.horizontal;
        v.position[1] = heights.sample(gx, gz) * scale.vertical + scale.offset;
        v.position[2] = float(gz) * scale.horizontal;
        v.argb = 0xffffffff;
        v.uv[0] = float(x[i]) / 4.f;
        v.uv[1] = float(z[i]) / 4.f;
    }
    mesh.indices = {0, 4, 1, 1, 4, 2, 2, 4, 3, 3, 4, 0};
    return mesh;
}
float TerrainHeights::sample(std::int32_t x, std::int32_t z) const {
    auto upper = std::int32_t(extent_);
    auto column = std::clamp(x, 0, upper), row = std::clamp(z, 0, upper);
    auto index = std::size_t(extent_) * std::size_t(row) + std::size_t(column);
    if (index >= samples_.size())
        throw std::out_of_range("Terrain sample exceeds loaded grid");
    return float(samples_[index]);
}
float TerrainHeights::height(float x, float z, std::int32_t outside) const {
    return float(height_extended(x, z, outside));
}
float TerrainHeights::world_height(float x, float z, float horizontal_scale, float vertical_scale,
                                   float vertical_offset, float inverse_vertical_scale) const {
    if (!std::isfinite(x) || !std::isfinite(z) || !std::isfinite(horizontal_scale) ||
        !std::isfinite(vertical_scale) || !std::isfinite(vertical_offset) ||
        !std::isfinite(inverse_vertical_scale))
        throw std::invalid_argument("Non-finite terrain world parameters");
    float local_x = float(static_cast<long double>(x) * horizontal_scale);
    float local_z = float(static_cast<long double>(z) * horizontal_scale);
    std::int32_t outside = 0;
    if (local_x < 0 || local_z < 0 || local_x >= float(extent_) || local_z >= float(extent_)) {
        auto fallback = -static_cast<long double>(vertical_offset) * inverse_vertical_scale;
        if (fallback < std::numeric_limits<std::int32_t>::min() ||
            fallback > std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("Terrain outside height exceeds integer range");
        outside = std::int32_t(fallback);
    }
    auto height = height_extended(local_x, local_z, outside);
    return float(height * vertical_scale + vertical_offset);
}
long double TerrainHeights::height_extended(float x, float z, std::int32_t outside) const {
    if (!std::isfinite(x) || !std::isfinite(z))
        throw std::invalid_argument("Non-finite terrain query");
    if (x < 0 || z < 0 || x >= float(extent_) || z >= float(extent_))
        return float(outside);
    auto base_x = std::int32_t(x * .25f) * 4;
    auto base_z = std::int32_t(z * .25f) * 4;
    auto point = [&](int dx, int dz) {
        return std::array<float, 3>{float(base_x + dx), sample(base_x + dx, base_z + dz),
                                    float(base_z + dz)};
    };
    const auto dx = static_cast<long double>(x) - float(base_x);
    const auto dz = static_cast<long double>(z) - float(base_z);
    if (dz > dx) {
        if (dz > 4.L - dx)
            return triangle_extended(point(2, 2), point(0, 4), point(4, 4), x, z);
        return triangle_extended(point(0, 0), point(0, 4), point(2, 2), x, z);
    }
    if (dz > 4.L - dx)
        return triangle_extended(point(2, 2), point(4, 4), point(4, 0), x, z);
    return triangle_extended(point(0, 0), point(2, 2), point(4, 0), x, z);
}
static long double triangle_extended(const std::array<float, 3>& a, const std::array<float, 3>& b,
                                     const std::array<float, 3>& c, float x, float z) {
    const long double ux = static_cast<long double>(b[0]) - a[0];
    const long double uy = static_cast<long double>(b[1]) - a[1];
    const long double uz = static_cast<long double>(b[2]) - a[2];
    const float vx = float(static_cast<long double>(c[0]) - a[0]);
    const float vy = float(static_cast<long double>(c[1]) - a[1]);
    const long double vz = static_cast<long double>(c[2]) - a[2];
    float nx = float(vz * uy - static_cast<long double>(vy) * uz);
    float ny = float(uz * vx - vz * ux);
    long double nz = static_cast<long double>(vy) * ux - static_cast<long double>(vx) * uy;
    auto inverse = 1.L / std::sqrt(nz * nz + static_cast<long double>(ny) * ny +
                                   static_cast<long double>(nx) * nx);
    nx = float(nx * inverse);
    ny = float(ny * inverse);
    nz *= inverse;
    const long double negative_inverse_y = -1.L / ny;
    auto height =
        (nz * a[2] + static_cast<long double>(nx) * a[0] + static_cast<long double>(ny) * a[1]) /
        ny;
    height += nz * z * negative_inverse_y;
    height += static_cast<long double>(nx) * x * negative_inverse_y;
    return height;
}
float terrain_triangle_height(const std::array<float, 3>& a, const std::array<float, 3>& b,
                              const std::array<float, 3>& c, float x, float z) {
    return float(triangle_extended(a, b, c, x, z));
}
} // namespace genesis
