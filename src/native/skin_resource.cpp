#include "skin_resource.h"
#include "image.h"
#include "math.h"
#include <algorithm>
#include <cmath>
namespace genesis {
SkinResource::SkinResource(Renderer& renderer, const shaiya::portable::Archive& archive,
                           std::string_view mesh, std::string_view texture, bool reflective)
    : renderer_(renderer), data_(decode_skin(archive.read(mesh))), reflective_(reflective) {
    if (data_.vertices.empty() || data_.indices.empty())
        throw std::runtime_error("Skin has no renderable triangles");
    try {
        if (!texture.empty()) {
            auto image = decode_image(archive.read(texture));
            texture_ =
                renderer_.texture_rgba(int(image.width), int(image.height), image.pixels.data());
        }
        mesh_ = renderer_.create_mesh(Layout::skin, data_.vertices.data(), data_.vertices.size(),
                                      data_.indices.data(), data_.indices.size());
    } catch (...) {
        if (texture_)
            renderer_.release_texture(texture_);
        throw;
    }
}
SkinResource::~SkinResource() {
    renderer_.release_mesh(mesh_);
    if (texture_)
        renderer_.release_texture(texture_);
}
Matrix SkinResource::fitted_world() const {
    auto minimum = data_.vertices.front().position, maximum = data_.vertices.front().position;
    std::array<float, 3> low = {minimum[0], minimum[1], minimum[2]},
                         high = {maximum[0], maximum[1], maximum[2]};
    for (const auto& vertex : data_.vertices)
        for (unsigned i = 0; i < 3; ++i) {
            low[i] = std::min(low[i], vertex.position[i]);
            high[i] = std::max(high[i], vertex.position[i]);
        }
    float squared = 0;
    for (unsigned i = 0; i < 3; ++i) {
        auto half = (high[i] - low[i]) * .5f;
        squared += half * half;
    }
    auto radius = std::max(std::sqrt(squared), .001f);
    Matrix world = identity;
    for (unsigned i = 0; i < 3; ++i) {
        world.m[i][i] = 1 / radius;
        world.m[3][i] = -(high[i] + low[i]) * .5f / radius;
    }
    return world;
}
void SkinResource::draw_bind_pose(const Matrix& world) {
    draw_palette(world, std::vector<Matrix>(data_.inverse_bind.size(), identity));
}
void SkinResource::draw(const Matrix& world, const std::vector<Matrix>& finals) {
    draw_palette(world, skin_palette(data_, finals));
}
void SkinResource::draw_palette(const Matrix& world, const std::vector<Matrix>& palette) {
    renderer_.bones(palette.data(), palette.size());
    ObjectUniforms object;
    object.world = world;
    renderer_.object(object);
    renderer_.bind_images({texture_, 0, 0, 0});
    renderer_.draw_mesh(mesh_, reflective_ ? VertexPass::reflective_skin : VertexPass::skin,
                        reflective_ ? PixelPass::reflection : PixelPass::surface,
                        {reflective_ ? Blend::opaque : Blend::alpha, Depth::write, Cull::none,
                         Sampling::anisotropic_wrap});
}
} // namespace genesis
