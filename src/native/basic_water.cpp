#include "basic_water.h"
#include "image.h"
#include "graphics/render_types.h"
#include <stdexcept>
namespace genesis {
BasicWater::BasicWater(Renderer& renderer, const shaiya::portable::Archive& archive,
                       std::string_view definition_path)
    : renderer_(renderer), definition_(decode_water_definition(archive.read(definition_path))) {
    basic_water_plane(definition_.tile_size); // validates the tile size
    if (definition_.frames.empty() || definition_.frames.size() > 0x7fffffffu ||
        definition_.frame_period > 0x7fffffffu)
        throw std::invalid_argument("Unusable water definition");
    clock_.period = std::int32_t(definition_.frame_period);
    clock_.frame_count = std::int32_t(definition_.frames.size());
    try {
        for (const auto& name : definition_.frames) {
            GLuint texture = 0;
            if (!name.empty()) {
                auto decoded = decode_image(archive.read("data/entity/water/" + name));
                texture = renderer_.texture_rgba(int(decoded.width), int(decoded.height),
                                                 decoded.pixels.data());
            }
            textures_.push_back(texture);
        }
    } catch (...) {
        for (auto texture : textures_)
            if (texture)
                renderer_.release_texture(texture);
        throw;
    }
}
BasicWater::~BasicWater() {
    for (auto texture : textures_)
        if (texture)
            renderer_.release_texture(texture);
}
void BasicWater::advance(float frame_delta_seconds) {
    clock_.advance(water_frame_step(frame_delta_seconds));
}
void BasicWater::draw(const Matrix& world, float camera_height) {
    auto texture = textures_[std::size_t(clock_.frame)];
    if (!texture)
        return;
    const auto plane = basic_water_plane(definition_.tile_size);
    const float lo = plane.min_corner, hi = plane.max_corner, uv = plane.uv_extent;
    using shaiya::graphics::native::WorldVertex;
    const WorldVertex vertices[4] = {{{lo, 0, lo}, definition_.argb, {0, 0}},
                                     {{lo, 0, hi}, definition_.argb, {0, uv}},
                                     {{hi, 0, lo}, definition_.argb, {uv, 0}},
                                     {{hi, 0, hi}, definition_.argb, {uv, uv}}};
    const std::uint16_t indices[6] = {0, 1, 2, 2, 1, 3};
    ObjectUniforms object;
    object.world = world;
    renderer_.object(object);
    renderer_.bind_images({texture, 0, 0, 0});
    renderer_.draw(Layout::world, VertexPass::world, PixelPass::surface, vertices, 4,
                   {Blend::alpha, Depth::write, camera_height > 0 ? Cull::back : Cull::none,
                    Sampling::linear_wrap},
                   indices, 6);
}
} // namespace genesis
