#include "shadow.h"

namespace genesis {
ShadowProjection make_shadow_projection(Vec3 position, float offset, float width, float height,
                                        float near_plane, float far_plane) {
    if (!(width > 0) || !(height > 0) || !(near_plane > 0) || !(far_plane > near_plane))
        throw std::invalid_argument("Invalid shadow projection parameters");
    ShadowProjection result;
    result.world = identity;
    for (unsigned i = 0; i < 3; ++i)
        result.world.m[3][i] = position[i];
    const Vec3 eye = {position[0], position[1] + 50, position[2] + offset};
    auto safe = [](Vec3 v) {
        float length = std::sqrt(dot(v, v));
        if (length > 0)
            for (auto& c : v)
                c /= length;
        return v;
    };
    const Vec3 forward = safe({0, -50, -offset});
    const Vec3 right = safe({forward[2], 0, -forward[0]});
    const Vec3 up = cross(forward, right);
    result.view = identity;
    for (unsigned i = 0; i < 3; ++i) {
        result.view.m[i][0] = right[i];
        result.view.m[i][1] = up[i];
        result.view.m[i][2] = forward[i];
        result.view.m[3][0] -= right[i] * eye[i];
        result.view.m[3][1] -= up[i] * eye[i];
        result.view.m[3][2] -= forward[i] * eye[i];
    }
    result.projection = {};
    result.projection.m[0][0] = 0.98f / width;
    result.projection.m[1][1] = 0.98f / height;
    result.projection.m[2][2] = far_plane / (far_plane - near_plane);
    result.projection.m[2][3] = 1;
    result.projection.m[3][2] = -near_plane * far_plane / (far_plane - near_plane);
    result.view_projection = multiply(result.view, result.projection);
    Matrix bias = identity;
    bias.m[0][0] = .5f;
    bias.m[1][1] = -.5f;
    bias.m[3][0] = bias.m[3][1] = .5f;
    result.sampling = multiply(result.view_projection, bias);
    return result;
}
struct ShadowMap::Saved {
    TargetScope scope;
    Saved(Renderer& r, Renderer::TargetId id) : scope(r, id) {}
};
ShadowMap::ShadowMap(Renderer& renderer, int size)
    : renderer_(renderer), target_(renderer.create_target(size, size)) {}
GLuint ShadowMap::texture() const {
    return renderer_.target(target_).color;
}
Style ShadowMap::caster_style() {
    Style style;
    style.blend = Blend::opaque;
    style.depth = Depth::write;
    style.cull = Cull::none;
    return style;
}
void ShadowMap::begin(const ShadowProjection& projection) {
    if (saved_)
        throw std::logic_error("Shadow pass already active");
    saved_ = std::make_shared<Saved>(renderer_, target_);
    renderer_.clear({0, 0, 0, 0});
    SceneUniforms scene;
    scene.view = projection.view;
    scene.view_projection = projection.view_projection;
    scene.fog = {};
    scene.clip_plane = {};
    renderer_.scene(scene);
}
void ShadowMap::end() {
    if (!saved_)
        throw std::logic_error("No active shadow pass");
    saved_.reset();
}
} // namespace genesis
