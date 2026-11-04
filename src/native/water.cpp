#include "water.h"
#include <cmath>

namespace genesis {
std::vector<std::int8_t> water_procedural_normals() {
    std::vector<std::int8_t> bytes(256 * 256 * 2);
    for (int y = 0; y < 256; ++y)
        for (int x = 0; x < 256; ++x) {
            const float angle =
                (float(y) * (1.f / 256) - .5f + float(x) * (1.f / 256) - .5f) * 12.566371f;
            bytes[(y * 256 + x) * 2] = std::int8_t(int(std::cos(angle) * 64.f));
            bytes[(y * 256 + x) * 2 + 1] = std::int8_t(int(std::sin(angle) * 64.f));
        }
    return bytes;
}
Image water_height_normals(const Image& heights) {
    if (!heights.width || !heights.height ||
        heights.pixels.size() != std::size_t(heights.width) * heights.height * 4)
        throw std::invalid_argument("Invalid water height map");
    Image out;
    out.width = heights.width;
    out.height = heights.height;
    out.pixels.resize(heights.pixels.size());
    const auto w = heights.width, h = heights.height;
    auto red = [&](std::uint32_t x, std::uint32_t y) {
        return float(heights.pixels[(std::size_t(y) * w + x) * 4]);
    };
    for (std::uint32_t y = 0; y < h; ++y)
        for (std::uint32_t x = 0; x < w; ++x) {
            const auto left = x ? x - 1 : w - 1, right = x + 1 == w ? 0 : x + 1,
                       above = y ? y - 1 : h - 1, below = y + 1 == h ? 0 : y + 1;
            const float nx = (red(left, y) - red(right, y)) * (10.f / 255),
                        ny = (red(x, above) - red(x, below)) * (10.f / 255);
            const float inverse = 1 / std::sqrt(nx * nx + ny * ny + 1);
            auto* pixel = out.pixels.data() + (std::size_t(y) * w + x) * 4;
            pixel[0] = std::uint8_t((nx * inverse * .5f + .5f) * 255 + .5f);
            pixel[1] = std::uint8_t((ny * inverse * .5f + .5f) * 255 + .5f);
            pixel[2] = std::uint8_t((inverse * .5f + .5f) * 255 + .5f);
            pixel[3] = 255;
        }
    return out;
}
Matrix water_projective_bias(const Matrix& source) {
    const Matrix bias = {{{.5f, 0, 0, 0}, {0, -.5f, 0, 0}, {0, 0, 1, 0}, {.5f, .5f, 0, 1}}};
    return multiply(source, bias);
}
std::array<WaterVertex32, 4> water32_quad() {
    const std::uint32_t color = 0x723f9e66;
    return {{{{-2048, 0, -2048}, color, {0, 0}, {0, 0}},
             {{-2048, 0, 2048}, color, {0, 0}, {0, 0}},
             {{2048, 0, -2048}, color, {0, 0}, {0, 0}},
             {{2048, 0, 2048}, color, {0, 0}, {0, 0}}}};
}
std::array<WaterVertex56, 4> water56_quad() {
    return {{{{-2048, 0, -2048}, {0, 1, 0}, {0, 2000}, {0, 2000}, {0, 2000}, {0, 2000}},
             {{-2048, 0, 2048}, {0, 1, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
             {{2048, 0, -2048}, {0, 1, 0}, {2000, 2000}, {2000, 2000}, {2000, 2000}, {2000, 2000}},
             {{2048, 0, 2048}, {0, 1, 0}, {2000, 0}, {2000, 0}, {2000, 0}, {2000, 0}}}};
}
void WaterAnimation::advance(float delta) {
    if (mode_ == 0)
        time_ += delta * .7f;
    else
        phase_ += delta * 1.5f;
}
std::array<WaterVertex32, 4> WaterAnimation::scrolled_quad() const {
    auto quad = water32_quad();
    const float phase = time_ * .5f;
    quad[0].uv[0] = quad[1].uv[0] = .5f;
    quad[2].uv[0] = quad[3].uv[0] = 400.5f;
    quad[0].uv[1] = quad[3].uv[1] = phase + 400.f;
    quad[1].uv[1] = quad[2].uv[1] = phase;
    return quad;
}
WaterCapture::WaterCapture(Renderer& renderer, int mode, int width, int height)
    : renderer_(renderer), mode_(mode), reflection_target_(renderer.create_target(width, height)) {
    if (mode < 0 || mode > 1)
        throw std::invalid_argument("Invalid water mode");
    if (mode == 1)
        refraction_target_ = renderer.create_target(width, height);
}
std::shared_ptr<TargetScope> WaterCapture::begin(Kind kind) {
    if (kind == Kind::refraction && mode_ != 1)
        throw std::invalid_argument("Refraction needs water mode 1");
    auto scope = std::make_shared<TargetScope>(
        renderer_, kind == Kind::reflection ? reflection_target_ : refraction_target_);
    renderer_.clear({1, 1, 1, 1});
    return scope;
}
void WaterCapture::capture(Kind kind) {
    const auto projection = water_projective_bias(renderer_.current_scene().view_projection);
    if (kind == Kind::reflection) {
        reflection_ = projection;
        reflection_valid_ = true;
    } else {
        refraction_ = projection;
        refraction_valid_ = true;
    }
}
bool WaterCapture::valid(Kind kind) const {
    return kind == Kind::reflection ? reflection_valid_ : refraction_valid_;
}
GLuint WaterCapture::texture(Kind kind) const {
    return renderer_.target(kind == Kind::reflection ? reflection_target_ : refraction_target_)
        .color;
}
void draw_water(Renderer& r, const WaterSurface& surface) {
    if (surface.mode < 0 || surface.mode > 1)
        throw std::invalid_argument("Invalid water mode");
    ObjectUniforms object;
    object.world = surface.world;
    object.reflection = surface.reflection_projection;
    object.parameters = {0, surface.distortion, surface.reflectivity, 1};
    object.uv = {surface.uv_offset[0], surface.uv_offset[1], 0, 0};
    r.object(object);
    Style style;
    style.blend = Blend::alpha;
    style.depth = Depth::read;
    style.cull = Cull::none;
    style.sampling = Sampling::linear_wrap;
    const std::uint16_t indices[6] = {0, 1, 2, 2, 1, 3};
    if (surface.mode == 0) {
        r.bind_images({surface.normal, surface.reflection, 0, 0});
        const auto quad = water32_quad();
        r.draw(Layout::water32, VertexPass::water32, PixelPass::water, quad.data(), 4, style,
               indices, 6);
    } else {
        WaterUniforms water;
        water.refraction = surface.refraction_projection;
        water.color = surface.water_color;
        water.parameters = {surface.distortion, surface.reflectivity, surface.opacity,
                            surface.normal_signed ? 1.f : 0.f};
        water.offsets = {surface.uv_offset[0], surface.uv_offset[1], surface.second_uv_offset[0],
                         surface.second_uv_offset[1]};
        r.water(water);
        r.bind_images({surface.normal, surface.reflection, surface.refraction,
                       surface.normal_second ? surface.normal_second : surface.normal});
        const auto quad = water56_quad();
        r.draw(Layout::water56, VertexPass::water56, PixelPass::water56, quad.data(), 4, style,
               indices, 6);
    }
    r.bind_images({});
}
void draw_underwater_attenuation(Renderer& r, float depth_factor) {
    const float amount = float(unsigned(int((1 - depth_factor) * .7f * 255)) & 255) / 255;
    const float w = float(r.width()) - .5f, h = float(r.height()) - .5f;
    const std::uint32_t black = 0xff000000;
    const UiVertex quad[4] = {{-.5f, -.5f, 0, 1, black, 0, 0},
                              {w, -.5f, 0, 1, black, 1, 0},
                              {w, h, 0, 1, black, 1, 1},
                              {-.5f, h, 0, 1, black, 0, 1}};
    const std::uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
    ObjectUniforms object;
    object.parameters[0] = 0;
    r.object(object);
    Style style;
    style.depth = Depth::disabled;
    style.cull = Cull::none;
    style.sampling = Sampling::point_clamp;
    style.custom_blend = CustomBlend{BlendFactor::blend_factor, BlendFactor::inv_blend_factor};
    style.blend_factor = {amount, amount, amount, amount};
    r.bind_images({});
    r.draw(Layout::ui, VertexPass::ui, PixelPass::ui, quad, 4, style, indices, 6);
}
} // namespace genesis
