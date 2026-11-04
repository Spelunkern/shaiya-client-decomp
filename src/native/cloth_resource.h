#pragma once
#include "archive.h"
#include "cloth_geometry.h"
#include "renderer.h"
namespace genesis {
// Explicit mesh/material binding. Actor equipment/CTL selection is a caller.
class ClothResource {
  public:
    ClothResource(Renderer& renderer, const shaiya::portable::Archive& archive, ClothSetup setup,
                  std::string_view mesh, std::string_view texture);
    ~ClothResource();
    ClothResource(const ClothResource&) = delete;
    ClothResource& operator=(const ClothResource&) = delete;
    void pin(const std::vector<Matrix>& finals);
    ClothTickStats advance(float seconds, std::uint32_t phase,
                           std::array<float, 3> reset_translation, const ClothScene& scene);
    Matrix fitted_world() const;
    void draw(const Matrix& world, bool fallback = false,
              Sampling sampling = Sampling::linear_wrap);
    const ClothParticles& particles() const {
        return particles_;
    }
    const ClothMesh& data() const {
        return data_;
    }

  private:
    Renderer& renderer_;
    ClothSetup setup_;
    ClothMesh data_;
    ClothParticles particles_;
    GLuint texture_ = 0;
};
} // namespace genesis
