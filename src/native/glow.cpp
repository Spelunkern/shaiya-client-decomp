#include "glow.h"
#include <algorithm>

namespace genesis {
float GlowEffect::strength(float authored, unsigned quality) {
    if (authored > 0 && authored <= 1)
        return authored;
    return quality == 0 ? 0.f : quality == 1 ? 25.f / 255.f : 50.f / 255.f;
}
bool GlowEffect::prepare() {
    if (capture_ && width_ == renderer_.width() && height_ == renderer_.height())
        return true;
    release();
    const int width = renderer_.width(), height = renderer_.height();
    capture_ = renderer_.create_target(width, height);
    image_ = renderer_.create_target(std::max(1, width / 2), std::max(1, height / 2));
    scratch_ = renderer_.create_target(std::max(1, width / 2), std::max(1, height / 2));
    width_ = width;
    height_ = height;
    return true;
}
void GlowEffect::release() {
    // Render targets are owned by the renderer for its lifetime; resizing
    // simply allocates a fresh set (the original does the same).
    capture_ = image_ = scratch_ = 0;
    width_ = height_ = 0;
}
void GlowEffect::filter(GLuint source, int source_width, int source_height,
                        Renderer::TargetId target, PixelPass pass, float alpha, Blend blend) {
    TargetScope scope(renderer_, target);
    ObjectUniforms object;
    object.parameters = {7.5f / 255, 1, 0, alpha};
    object.uv = {0, 0, 1.f / float(source_width), 1.f / float(source_height)};
    renderer_.object(object);
    renderer_.bind_images({source, 0, 0, 0});
    Style style;
    style.blend = blend;
    style.depth = Depth::disabled;
    style.cull = Cull::none;
    style.sampling = Sampling::linear_clamp;
    renderer_.fullscreen(pass, style);
    renderer_.bind_images({});
}
void GlowEffect::capture() {
    if (!prepare())
        return;
    const auto source = renderer_.target(renderer_.current_target());
    filter(source.color, source.width, source.height, capture_, PixelPass::copy, 1, Blend::opaque);
}
void GlowEffect::blur(unsigned iterations) {
    const auto capture = renderer_.target(capture_), half = renderer_.target(image_);
    filter(capture.color, capture.width, capture.height, image_, PixelPass::eight_tap, 1,
           Blend::opaque);
    for (unsigned i = 0; i < std::min(iterations, 16u); ++i) {
        filter(half.color, half.width, half.height, scratch_, PixelPass::gaussian_x, 1,
               Blend::opaque);
        const auto scratch = renderer_.target(scratch_);
        filter(scratch.color, scratch.width, scratch.height, image_, PixelPass::gaussian_y, 1,
               Blend::opaque);
    }
}
void GlowEffect::composite(float strength) {
    if (!prepare())
        return;
    const auto image = renderer_.target(image_);
    ObjectUniforms object;
    object.parameters = {7.5f / 255, 1, 0, strength};
    object.uv = {0, 0, 1.f / float(image.width), 1.f / float(image.height)};
    renderer_.object(object);
    renderer_.bind_images({image.color, 0, 0, 0});
    Style style;
    style.blend = Blend::additive;
    style.depth = Depth::disabled;
    style.cull = Cull::none;
    style.sampling = Sampling::linear_clamp;
    renderer_.fullscreen(PixelPass::copy, style);
    renderer_.bind_images({});
}
void GlowEffect::apply(unsigned iterations, float strength) {
    if (!(strength > 0))
        return;
    capture();
    blur(iterations);
    composite(strength);
}
} // namespace genesis
