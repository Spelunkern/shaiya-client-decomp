#include "cloth_resource.h"
#include "image.h"
#include <algorithm>
#include <cmath>
namespace genesis {
ClothResource::ClothResource(Renderer& renderer, const shaiya::portable::Archive& archive,
                             ClothSetup setup, std::string_view mesh, std::string_view texture)
    : renderer_(renderer), setup_(std::move(setup)), data_(decode_cloth_mesh(archive.read(mesh))),
      particles_(setup_, data_) {
    if (data_.vertices.empty() || data_.indices.empty())
        throw std::invalid_argument("Cloth rendering needs indexed geometry");
    auto image = decode_image(archive.read(texture));
    texture_ =
        renderer_.texture_rgba(int(image.width), int(image.height), image.pixels.data(), true);
}
ClothResource::~ClothResource() {
    renderer_.release_texture(texture_);
}
void ClothResource::pin(const std::vector<Matrix>& finals) {
    particles_.pin(cloth_pins(setup_, finals));
}
ClothTickStats ClothResource::advance(float seconds, std::uint32_t phase,
                                      std::array<float, 3> translation, const ClothScene& scene) {
    return particles_.advance(seconds, phase, translation, scene);
}
Matrix ClothResource::fitted_world() const {
    auto low = data_.vertices.front().position, high = low;
    for (const auto& vertex : data_.vertices)
        for (unsigned c = 0; c < 3; ++c) {
            low[c] = std::min(low[c], vertex.position[c]);
            high[c] = std::max(high[c], vertex.position[c]);
        }
    float squared = 0;
    for (unsigned c = 0; c < 3; ++c) {
        auto half = (high[c] - low[c]) * .5f;
        squared += half * half;
    }
    auto radius = std::max(std::sqrt(squared), .001f);
    auto world = identity;
    for (unsigned c = 0; c < 3; ++c) {
        world.m[c][c] = 1 / radius;
        world.m[3][c] = -(high[c] + low[c]) * .5f / radius;
    }
    return world;
}
void ClothResource::draw(const Matrix& world, bool fallback, Sampling sampling) {
    auto vertices = fallback ? cloth_fallback_vertices(data_, particles_, setup_.columns)
                             : cloth_vertices(particles_);
    ObjectUniforms object;
    object.world = world;
    renderer_.object(object);
    renderer_.bind_images({texture_, 0, 0, 0});
    renderer_.draw(Layout::world, VertexPass::world, PixelPass::surface, vertices.data(),
                   vertices.size(), {Blend::alpha, Depth::write, Cull::none, sampling},
                   data_.indices.data(), data_.indices.size());
}
} // namespace genesis
