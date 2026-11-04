#pragma once
#include "archive.h"
#include "renderer.h"
#include "skin.h"
namespace genesis {
class SkinResource {
  public:
    SkinResource(Renderer& renderer, const shaiya::portable::Archive& archive,
                 std::string_view mesh, std::string_view texture = {}, bool reflective = false);
    ~SkinResource();
    SkinResource(const SkinResource&) = delete;
    SkinResource& operator=(const SkinResource&) = delete;
    Matrix fitted_world() const;
    void draw_bind_pose(const Matrix& world);
    void draw(const Matrix& world, const std::vector<Matrix>& skeleton_finals);
    const SkinnedModel& data() const {
        return data_;
    }

  private:
    void draw_palette(const Matrix& world, const std::vector<Matrix>& palette);
    Renderer& renderer_;
    SkinnedModel data_;
    Renderer::MeshId mesh_ = 0;
    GLuint texture_ = 0;
    bool reflective_ = false;
};
} // namespace genesis
