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
    // Black silhouette for shadow maps (begin_shadow's textured caster state:
    // opaque, depth-written, no culling, alpha cutoff 7.5/255, black tint).
    void draw_shadow_caster(const Matrix& world, const std::vector<Matrix>& skeleton_finals);
    // Non-owning sphere-map for reflective skins (data/spheremap.bmp in the
    // original). Reflective draws use the recovered actor material: specular
    // color (1,1,1) with power 20 (native_actor_specular's three call sites).
    void set_environment(GLuint texture) {
        environment_ = texture;
    }
    bool reflective() const {
        return reflective_;
    }
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
    GLuint environment_ = 0;
};
} // namespace genesis
