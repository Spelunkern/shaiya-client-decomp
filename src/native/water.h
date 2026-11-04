#pragma once
#include "image.h"
#include "math.h"
#include "renderer.h"
#include <memory>

namespace genesis {
// Water/planar-reflection orchestration ported from native_water_ports.cpp,
// native_water_capture.cpp and native_water_draw.cpp.

// FUN_0053e020: 256x256 R8G8 signed normal source, cos/sin of
// ((y/256-.5)+(x/256-.5))*4pi scaled by 64 and truncated to int8.
std::vector<std::int8_t> water_procedural_normals();
// height_normals(): wrapped central differences of the red channel, x10/255
// gain, packed as unsigned RGBA normals (alpha 255). Used per mip level.
Image water_height_normals(const Image& heights);
// projective_bias(): clip space -> texture space (x*.5+.5, y*-.5+.5) for
// sampling the reflection/refraction captures.
Matrix water_projective_bias(const Matrix& view_projection);
// FUN_0053e7b0: the +/-2048 quads for the two water modes.
std::array<WaterVertex32, 4> water32_quad();
std::array<WaterVertex56, 4> water56_quad();
// FUN_0053ded0: per-frame animation. Mode 0 scrolls the quad's V coordinates
// (accumulated time * .7, stored *.5, second row +400); mode 1 advances a
// single UV phase by delta * 1.5.
class WaterAnimation {
  public:
    explicit WaterAnimation(int mode) : mode_(mode) {}
    void advance(float delta_seconds);
    // Mode 0 only.
    std::array<WaterVertex32, 4> scrolled_quad() const;
    // Mode 1 only; second layer uses (-phase, phase).
    float phase() const {
        return phase_;
    }
    std::array<float, 2> first_offset() const {
        return {phase_, phase_};
    }
    std::array<float, 2> second_offset() const {
        return {-phase_, phase_};
    }

  private:
    int mode_;
    float time_ = 0, phase_ = 0;
};

// water_quad()/draw_water32()/draw_water56(): one +/-2048 water quad. Mode 0
// samples a signed procedural normal map (`normal`) plus the reflection
// capture; mode 1 adds the refraction capture and a second normal layer.
struct WaterSurface {
    int mode = 0;
    Matrix world = identity;
    GLuint normal = 0, normal_second = 0, reflection = 0, refraction = 0;
    Matrix reflection_projection = identity, refraction_projection = identity;
    std::array<float, 2> uv_offset = {}, second_uv_offset = {};
    float distortion = 0.002f, reflectivity = 240.f / 255, opacity = 114.f / 255;
    Color water_color = {63.f / 255, 158.f / 255, 102.f / 255, 1};
    bool normal_signed = false;
};
void draw_water(Renderer& renderer, const WaterSurface& surface);
// FUN_0053f470: full-screen black quad blended with constant factors so the
// frame is attenuated by `amount` = trunc((1 - depth_factor) * .7 * 255) / 255
// (depth_factor is the original dat_0085d138; 1 = no attenuation).
void draw_underwater_attenuation(Renderer& renderer, float depth_factor);

// Owns the reflection (and, for mode 1, refraction) render textures and the
// captured projective matrices.
class WaterCapture {
  public:
    enum class Kind { reflection, refraction };
    WaterCapture(Renderer& renderer, int mode, int width, int height);
    // Redirects drawing to the capture target cleared to white (FUN_0053e3e0)
    // for the scope of the returned object.
    std::shared_ptr<TargetScope> begin(Kind kind);
    // Call while the mirrored/refracted camera is installed
    // (capture_water_reflection/refraction).
    void capture(Kind kind);
    bool valid(Kind kind) const;
    const Matrix& projection(Kind kind) const {
        return kind == Kind::reflection ? reflection_ : refraction_;
    }
    GLuint texture(Kind kind) const;

  private:
    Renderer& renderer_;
    int mode_;
    Renderer::TargetId reflection_target_, refraction_target_ = 0;
    Matrix reflection_ = identity, refraction_ = identity;
    bool reflection_valid_ = false, refraction_valid_ = false;
};
} // namespace genesis
