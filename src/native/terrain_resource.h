#pragma once
#include "archive.h"
#include "terrain.h"
#include "renderer.h"
#include <memory>
#include <optional>
namespace genesis {
// Load the field grid from an archive path selected by the world session.
TerrainFieldData load_terrain_field(const shaiya::portable::Archive& archive, std::string_view path,
                                    float spacing);
std::string terrain_material_texture_path(const TerrainMaterialRecord& material);
void draw_terrain_cell(Renderer& renderer, const TerrainCellMesh& cell, GLuint texture,
                       const Matrix& world);
void draw_terrain_region(Renderer& renderer, const TerrainHeights& heights, std::uint32_t x,
                         std::uint32_t z, std::uint32_t width, std::uint32_t height,
                         const TerrainScale& scale, GLuint texture, const Matrix& world);

// Owns the GPU batches for one authored FLD field, grouped by the original
// per-cell material index so each batch binds a single detail texture.
// Centered on a bounded span around the field's middle; it is a viewer/scene
// connector, not a streaming world terrain system.
class TerrainResource {
  public:
    // center_world, when given, is a {x, z} world position (the connected
    // player's own coordinates); the rendered span is centered on its
    // nearest cell instead of the field's geometric middle.
    TerrainResource(Renderer& renderer, const shaiya::portable::Archive& archive,
                    std::string_view path, const TerrainScale& scale = default_terrain_scale(),
                    std::optional<std::array<float, 2>> center_world = std::nullopt);
    ~TerrainResource();
    TerrainResource(const TerrainResource&) = delete;
    TerrainResource& operator=(const TerrainResource&) = delete;
    Matrix fitted_world() const;
    void draw(const Matrix& world);
    const TerrainFieldData& data() const {
        return data_;
    }
    const TerrainHeights& heights() const {
        return *heights_;
    }
    std::size_t mesh_count() const {
        return batches_.size();
    }

  private:
    void build_batches(const shaiya::portable::Archive& archive,
                       std::optional<std::array<float, 2>> center_world);
    void clear();
    struct Batch {
        Renderer::MeshId mesh;
        GLuint texture;
    };
    Renderer& renderer_;
    TerrainScale scale_;
    TerrainFieldData data_;
    std::unique_ptr<TerrainHeights> heights_;
    std::array<float, 3> bounds_min_{}, bounds_max_{};
    std::vector<Batch> batches_;
    std::vector<GLuint> images_;
};
} // namespace genesis
