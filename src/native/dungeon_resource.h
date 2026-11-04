#pragma once
#include "archive.h"
#include "dungeon.h"
#include "renderer.h"
namespace genesis {
class DungeonResource {
  public:
    DungeonResource(Renderer& renderer, const shaiya::portable::Archive& archive,
                    std::string_view path);
    ~DungeonResource();
    DungeonResource(const DungeonResource&) = delete;
    DungeonResource& operator=(const DungeonResource&) = delete;
    Matrix fitted_world() const;
    void draw(const Matrix& world);
    const Dungeon& data() const {
        return data_;
    }
    std::size_t mesh_count() const {
        return batches_.size();
    }

  private:
    void clear();
    struct Batch {
        Renderer::MeshId mesh;
        GLuint texture, lightmap;
    };
    Renderer& renderer_;
    Dungeon data_;
    std::vector<Batch> batches_;
    std::vector<GLuint> images_;
};
} // namespace genesis
