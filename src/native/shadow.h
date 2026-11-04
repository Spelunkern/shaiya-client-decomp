#pragma once
#include "math.h"
#include "renderer.h"

namespace genesis {
// Actor projected-shadow camera, ported from native_shadow_ports.cpp
// (prepare_view/begin_shadow/FUN_00548880): a top-down view 50 units above the
// actor (z-shifted by `offset`; 20 for the selection shadow, .01 for the
// textured actor shadow), the 0.98-scaled extents-derived projection, and the
// 0.5/-0.5/0.5 bias that maps clip space to shadow-map UVs.
struct ShadowProjection {
    Matrix world, view, projection, view_projection, sampling;
};
ShadowProjection make_shadow_projection(Vec3 position, float offset, float width, float height,
                                        float near_plane, float far_plane);

// Offscreen shadow map. begin() redirects drawing to a cleared transparent
// target and installs the shadow scene; end() restores the previous target
// and scene. Draw casters between them with `caster_style()`.
class ShadowMap {
  public:
    ShadowMap(Renderer& renderer, int size);
    Renderer::TargetId target() const {
        return target_;
    }
    GLuint texture() const;
    // Black, opaque caster style: depth-written, no culling, no fog, no lighting.
    static Style caster_style();
    void begin(const ShadowProjection& projection);
    void end();

  private:
    struct Saved;
    Renderer& renderer_;
    Renderer::TargetId target_;
    std::shared_ptr<Saved> saved_;
};
} // namespace genesis
