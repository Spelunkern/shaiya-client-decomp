#include "terrain_resource.h"
#include "asset_path.h"
#include "image.h"
#include "math.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
namespace genesis {
std::string terrain_material_texture_path(const TerrainMaterialRecord& material) {
    return asset_path::join("data/Terrain/Detail",
                            asset_path::replace_extension(material.texture, ".dds"));
}
TerrainFieldData load_terrain_field(const shaiya::portable::Archive& archive, std::string_view path,
                                    float spacing) {
    return decode_terrain_field(archive.read(path), spacing);
}
void draw_terrain_cell(Renderer& renderer, const TerrainCellMesh& cell, GLuint texture,
                       const Matrix& world) {
    ObjectUniforms object;
    object.world = world;
    renderer.object(object);
    renderer.bind_images({texture, 0, 0, 0});
    renderer.draw(Layout::world, VertexPass::world, PixelPass::surface, cell.vertices.data(),
                  cell.vertices.size(),
                  {Blend::opaque, Depth::write, Cull::none, Sampling::linear_wrap},
                  cell.indices.data(), cell.indices.size());
}
void draw_terrain_region(Renderer& renderer, const TerrainHeights& heights, std::uint32_t x,
                         std::uint32_t z, std::uint32_t width, std::uint32_t height,
                         const TerrainScale& scale, GLuint texture, const Matrix& world) {
    if (!width || !height || std::uint64_t(width) * height > 13107 ||
        std::uint64_t(x) + width > 0xffffffffu || std::uint64_t(z) + height > 0xffffffffu)
        throw std::invalid_argument("Invalid terrain region size");
    std::vector<shaiya::graphics::native::WorldVertex> vertices;
    std::vector<std::uint16_t> indices;
    vertices.reserve(std::size_t(width) * height * 5);
    indices.reserve(std::size_t(width) * height * 12);
    for (std::uint32_t row = 0; row < height; ++row)
        for (std::uint32_t column = 0; column < width; ++column) {
            auto cell = terrain_cell_mesh(heights, x + column, z + row, scale);
            auto base = std::uint16_t(vertices.size());
            vertices.insert(vertices.end(), cell.vertices.begin(), cell.vertices.end());
            for (auto index : cell.indices)
                indices.push_back(std::uint16_t(base + index));
        }
    ObjectUniforms object;
    object.world = world;
    renderer.object(object);
    renderer.bind_images({texture, 0, 0, 0});
    renderer.draw(Layout::world, VertexPass::world, PixelPass::surface, vertices.data(),
                  vertices.size(), {Blend::opaque, Depth::write, Cull::none, Sampling::linear_wrap},
                  indices.data(), indices.size());
}
TerrainResource::TerrainResource(Renderer& renderer, const shaiya::portable::Archive& archive,
                                 std::string_view path, const TerrainScale& scale,
                                 std::optional<std::array<float, 2>> center_world)
    : renderer_(renderer), scale_(scale) {
    bounds_min_ = {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
                   std::numeric_limits<float>::infinity()};
    bounds_max_ = {-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                   -std::numeric_limits<float>::infinity()};
    auto bytes = archive.read(path);
    data_ = decode_terrain_field(bytes, scale_.horizontal);
    data_.materials = decode_terrain_materials(
        bytes, data_.prefix.bytes_consumed + data_.grid.bytes_consumed);
    heights_ = std::make_unique<TerrainHeights>(
        TerrainHeights::from_grid(data_.dimension, std::move(data_.grid)));
    try {
        build_batches(archive, center_world);
    } catch (...) {
        clear();
        throw;
    }
}
TerrainResource::~TerrainResource() {
    clear();
}
void TerrainResource::clear() {
    for (auto batch : batches_)
        renderer_.release_mesh(batch.mesh);
    for (auto image : images_)
        renderer_.release_texture(image);
    batches_.clear();
    images_.clear();
}
void TerrainResource::build_batches(const shaiya::portable::Archive& archive,
                                    std::optional<std::array<float, 2>> center_world) {
    // 005b3250's extent covers N*N height samples; a 4x4 cell needs four more
    // samples past its origin, matching terrain_cell_mesh's own bound.
    std::uint32_t cell_count = data_.dimension >= 5 ? (data_.dimension - 5) / 4 + 1 : 0;
    if (!cell_count)
        throw std::invalid_argument("Terrain field has no renderable cells");
    constexpr std::uint32_t max_span = 96; // keeps width*height under the GPU region cap
    std::uint32_t span = std::min(cell_count, max_span);
    auto centered_origin = [&](float world) {
        if (!std::isfinite(world))
            return (cell_count - span) / 2;
        auto sample = static_cast<long double>(world) * scale_.inverse_horizontal;
        auto cell = std::int64_t(sample / 4) - std::int64_t(span / 2);
        cell = std::clamp<std::int64_t>(cell, 0, std::int64_t(cell_count - span));
        return std::uint32_t(cell);
    };
    std::uint32_t origin_x = center_world ? centered_origin((*center_world)[0])
                                          : (cell_count - span) / 2;
    std::uint32_t origin_z = center_world ? centered_origin((*center_world)[1])
                                          : (cell_count - span) / 2;
    constexpr std::uint32_t no_material = 0xffffffffu;
    std::unordered_map<std::uint32_t, std::vector<shaiya::graphics::native::WorldVertex>>
        vertices_by_material;
    std::unordered_map<std::uint32_t, std::vector<std::uint16_t>> indices_by_material;
    for (std::uint32_t row = 0; row < span; ++row)
        for (std::uint32_t column = 0; column < span; ++column) {
            auto cell_x = origin_x + column, cell_z = origin_z + row;
            auto attribute = terrain_attribute(data_.grid, data_.dimension, float(cell_x * 4 + 2),
                                               float(cell_z * 4 + 2));
            auto key = attribute < data_.materials.size() ? std::uint32_t(attribute) : no_material;
            auto cell = terrain_cell_mesh(*heights_, cell_x, cell_z, scale_);
            for (const auto& vertex : cell.vertices)
                for (unsigned axis = 0; axis < 3; ++axis) {
                    bounds_min_[axis] = std::min(bounds_min_[axis], vertex.position[axis]);
                    bounds_max_[axis] = std::max(bounds_max_[axis], vertex.position[axis]);
                }
            auto& vertices = vertices_by_material[key];
            auto& indices = indices_by_material[key];
            auto base = std::uint16_t(vertices.size());
            vertices.insert(vertices.end(), cell.vertices.begin(), cell.vertices.end());
            for (auto index : cell.indices)
                indices.push_back(std::uint16_t(base + index));
        }
    std::unordered_map<std::string, GLuint> loaded;
    auto load_texture = [&](const std::string& name) {
        auto found = loaded.find(name);
        if (found != loaded.end())
            return found->second;
        auto decoded = decode_image(archive.read(name));
        auto texture = renderer_.texture_rgba(static_cast<int>(decoded.width),
                                              static_cast<int>(decoded.height),
                                              decoded.pixels.data());
        images_.push_back(texture);
        loaded.emplace(name, texture);
        return texture;
    };
    batches_.reserve(vertices_by_material.size());
    for (auto& [key, vertices] : vertices_by_material) {
        auto& indices = indices_by_material[key];
        if (vertices.empty() || indices.empty())
            continue;
        GLuint texture = 0;
        if (key != no_material) {
            auto texture_path = terrain_material_texture_path(data_.materials[key]);
            if (archive.find(texture_path))
                texture = load_texture(texture_path);
        }
        auto mesh = renderer_.create_mesh(Layout::world, vertices.data(), vertices.size(),
                                          indices.data(), indices.size());
        batches_.push_back({mesh, texture});
    }
}
Matrix TerrainResource::fitted_world() const {
    Matrix world = identity;
    float squared = 0;
    for (unsigned i = 0; i < 3; ++i) {
        auto half = (bounds_max_[i] - bounds_min_[i]) * .5f;
        squared += half * half;
    }
    auto radius = std::max(std::sqrt(squared), .001f);
    for (unsigned i = 0; i < 3; ++i) {
        world.m[i][i] = 1 / radius;
        world.m[3][i] = -(bounds_max_[i] + bounds_min_[i]) * .5f / radius;
    }
    return world;
}
void TerrainResource::draw(const Matrix& world) {
    ObjectUniforms object;
    object.world = world;
    renderer_.object(object);
    for (auto batch : batches_) {
        renderer_.bind_images({batch.texture, 0, 0, 0});
        renderer_.draw_mesh(batch.mesh, VertexPass::world, PixelPass::surface,
                            {Blend::opaque, Depth::write, Cull::none, Sampling::linear_wrap});
    }
}
void TerrainResource::draw_shadow(const Matrix& world, GLuint shadow_texture,
                                  const Matrix& sampling, float opacity) {
    ObjectUniforms object;
    object.world = world;
    object.reflection = sampling;
    object.parameters = {0, 0, opacity, 1};
    renderer_.object(object);
    for (auto batch : batches_) {
        renderer_.bind_images({shadow_texture, 0, 0, 0});
        renderer_.draw_mesh(batch.mesh, VertexPass::world, PixelPass::projected_shadow,
                            {Blend::alpha, Depth::read, Cull::none, Sampling::linear_wrap});
    }
}
} // namespace genesis
