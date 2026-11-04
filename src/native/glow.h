#pragma once
#include "renderer.h"

namespace genesis {
// Glow orchestration ported from native_postfx_ports.cpp (FUN_0054f860/
// FUN_0054ea70): capture the current target, 8-tap downsample into a
// half-size image, 0-16 separable gaussian iterations through a scratch
// target, then composite additively onto the current target.
class GlowEffect {
  public:
    explicit GlowEffect(Renderer& renderer) : renderer_(renderer) {}
    // authored_strength in (0,1] wins; otherwise quality 0/1/other selects
    // 0, 25/255 or 50/255, exactly like the original strength().
    static float strength(float authored_strength, unsigned quality);
    // Returns false if the resources cannot be created; the effect is then
    // skipped (the original clears its enabled flag on failure).
    bool prepare();
    void capture();
    void blur(unsigned iterations);
    void composite(float strength);
    // capture + downsample + blur + composite for the current target.
    void apply(unsigned iterations, float strength);
    void release();

  private:
    void filter(GLuint source, int source_width, int source_height, Renderer::TargetId target,
                PixelPass pass, float alpha, Blend blend);
    Renderer& renderer_;
    Renderer::TargetId capture_ = 0, image_ = 0, scratch_ = 0;
    int width_ = 0, height_ = 0;
};
} // namespace genesis
