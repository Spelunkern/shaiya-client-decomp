#pragma once
#include "archive.h"
#include "renderer.h"
#include "terrain.h"
#include <string_view>
#include <vector>
namespace genesis {
// The original "basic water": one textured, alpha-blended plane at world y = 0
// whose frame texture cycles on the `.wtr` clock (005b0610, 005a9fc0).
class BasicWater {
  public:
    BasicWater(Renderer& renderer, const shaiya::portable::Archive& archive,
               std::string_view definition_path);
    ~BasicWater();
    BasicWater(const BasicWater&) = delete;
    BasicWater& operator=(const BasicWater&) = delete;
    // `frame_delta_seconds` is the original per-frame delta (dat_00719a54).
    void advance(float frame_delta_seconds);
    // 00447ef0: back-face culling above the plane, none below it.
    void draw(const Matrix& world, float camera_height);
    const WaterDefinition& definition() const {
        return definition_;
    }
    unsigned frame() const {
        return unsigned(clock_.frame);
    }

  private:
    Renderer& renderer_;
    WaterDefinition definition_;
    WaterFrameClock clock_;
    std::vector<GLuint> textures_;
};
} // namespace genesis
