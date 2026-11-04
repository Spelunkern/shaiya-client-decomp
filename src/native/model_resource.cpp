#include "model_resource.h"
#include "image.h"
#include "asset_path.h"
#include <algorithm>
#include <unordered_map>

namespace genesis {
ModelResource::ModelResource(Renderer& renderer, const shaiya::portable::Archive& archive,
                             std::string_view path)
    : renderer_(renderer), model_(decode_smod(archive.read(path))) {
    meshes_.reserve(model_.meshes.size());
    images_.reserve(model_.meshes.size());
    owned_images_.reserve(model_.meshes.size());
    std::unordered_map<std::string, GLuint> loaded;
    try {
        for (const auto& mesh : model_.meshes) {
            meshes_.push_back(mesh.vertices.empty() || mesh.indices.empty()
                                  ? 0
                                  : renderer_.create_mesh(Layout::mesh, mesh.vertices.data(),
                                                          mesh.vertices.size(), mesh.indices.data(),
                                                          mesh.indices.size()));
            if (mesh.texture.empty()) {
                images_.push_back(0);
                continue;
            }
            std::string texture = "data/Entity/Texture/" + mesh.texture;
            if (!archive.find(texture))
                texture = asset_path::replace_extension(texture, ".dds");
            if (!archive.find(texture))
                throw std::runtime_error("Missing model texture: " + texture);
            auto found = loaded.find(texture);
            if (found == loaded.end()) {
                auto image = decode_image(archive.read(texture));
                auto id =
                    renderer_.texture_rgba(static_cast<int>(image.width),
                                           static_cast<int>(image.height), image.pixels.data());
                owned_images_.push_back(id);
                found = loaded.emplace(texture, id).first;
            }
            images_.push_back(found->second);
        }
    } catch (...) {
        clear();
        throw;
    }
}
ModelResource::~ModelResource() {
    clear();
}
void ModelResource::clear() {
    for (auto mesh : meshes_)
        renderer_.release_mesh(mesh);
    for (auto image : owned_images_)
        renderer_.release_texture(image);
    meshes_.clear();
    owned_images_.clear();
    images_.clear();
}
Matrix ModelResource::fitted_world() const {
    Matrix world = identity;
    float radius = std::max(model_.sphere[3], .001f);
    world.m[0][0] = world.m[1][1] = world.m[2][2] = 1 / radius;
    for (unsigned i = 0; i < 3; ++i)
        world.m[3][i] = -model_.sphere[i] / radius;
    return world;
}
void ModelResource::draw(const Matrix& world, const Style& style) {
    ObjectUniforms object;
    object.world = world;
    renderer_.object(object);
    for (std::size_t i = 0; i < meshes_.size(); ++i)
        if (meshes_[i]) {
            renderer_.bind_images({images_[i], 0, 0, 0});
            renderer_.draw_mesh(meshes_[i], VertexPass::mesh, PixelPass::surface, style);
        }
}
} // namespace genesis
