#pragma once
#include "math.h"
#include "renderer.h"
#include <optional>

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

// One actor's ground shadow: a top-down caster pass into a ShadowMap. `radius`
// is the actor's bounding radius in scene units; the 0.98-scaled extents are
// radius / eye height, matching the original's slope-based extents.
class ActorShadow {
  public:
    // The shadow target is allocated on first use.
    explicit ActorShadow(Renderer& renderer, int size = 256) : renderer_(renderer), size_(size) {}
    template <class Caster>
    void render(Vec3 feet, float radius, Caster&& draw_casters) {
        const float slope = radius / 50.f;
        projection_ = make_shadow_projection(feet, .01f, slope, slope, .1f, 200.f);
        if (!map_)
            map_.emplace(renderer_, size_);
        map_->begin(projection_);
        draw_casters();
        map_->end();
        valid_ = true;
    }
    bool valid() const {
        return valid_;
    }
    GLuint texture() const {
        return map_->texture();
    }
    const Matrix& sampling() const {
        return projection_.sampling;
    }

  private:
    Renderer& renderer_;
    int size_;
    std::optional<ShadowMap> map_;
    ShadowProjection projection_{};
    bool valid_ = false;
};
} // namespace genesis
