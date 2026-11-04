#include "dungeon_resource.h"
#include "image.h"
#include "asset_path.h"
#include "math.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <unordered_map>

namespace genesis {
DungeonResource::DungeonResource(Renderer& renderer, const shaiya::portable::Archive& archive,
                                 std::string_view path)
    : renderer_(renderer), data_(decode_dungeon(archive.read(path))) {
    const auto folder = asset_path::join(asset_path::parent(path), asset_path::stem(path));
    std::unordered_map<std::string, GLuint> loaded;
    auto image = [&](const std::string& name) {
        auto found = loaded.find(name);
        if (found != loaded.end())
            return found->second;
        auto decoded = decode_image(archive.read(name));
        auto texture =
            renderer_.texture_rgba(static_cast<int>(decoded.width),
                                   static_cast<int>(decoded.height), decoded.pixels.data());
        images_.push_back(texture);
        loaded.emplace(name, texture);
        return texture;
    };
    try {
        std::vector<GLuint> textures, lightmaps;
        images_.reserve(data_.textures.size() + data_.lightmaps);
        for (const auto& name : data_.textures) {
            std::string resolved;
            for (const auto& root :
                 {std::string("data/Entity/Texture"), folder, asset_path::parent(path)}) {
                auto candidate = asset_path::join(root, name);
                if (archive.find(candidate)) {
                    resolved = candidate;
                    break;
                }
                candidate = asset_path::replace_extension(candidate, ".dds");
                if (archive.find(candidate)) {
                    resolved = candidate;
                    break;
                }
            }
            if (resolved.empty())
                throw std::runtime_error("Missing DG texture: " + name);
            textures.push_back(image(resolved));
        }
        for (std::uint32_t i = 0; i < data_.lightmaps; ++i) {
            auto name = asset_path::join(folder, asset_path::stem(path) + "_L" + std::to_string(i) +
                                                     ".dds");
            if (!archive.find(name))
                throw std::runtime_error("Missing DG lightmap: " + name);
            lightmaps.push_back(image(name));
        }
        std::size_t count = 0;
        for (const auto& node : data_.nodes)
            count += node.meshes.size();
        batches_.reserve(count);
        for (const auto& node : data_.nodes)
            for (const auto& mesh : node.meshes) {
                if (mesh.vertices.empty() || mesh.indices.empty())
                    continue;
                auto id = renderer_.create_mesh(Layout::lightmap, mesh.vertices.data(),
                                                mesh.vertices.size(), mesh.indices.data(),
                                                mesh.indices.size());
                batches_.push_back(
                    {id, textures.at(mesh.texture),
                     mesh.lightmap < lightmaps.size() ? lightmaps[mesh.lightmap] : 0});
            }
    } catch (...) {
        clear();
        throw;
    }
}
DungeonResource::~DungeonResource() {
    clear();
}
void DungeonResource::clear() {
    for (auto batch : batches_)
        renderer_.release_mesh(batch.mesh);
    for (auto image : images_)
        renderer_.release_texture(image);
    batches_.clear();
    images_.clear();
}
Matrix DungeonResource::fitted_world() const {
    Matrix world = identity;
    float squared = 0;
    for (unsigned i = 0; i < 3; ++i) {
        auto half = (data_.bounds.maximum[i] - data_.bounds.minimum[i]) * .5f;
        squared += half * half;
    }
    auto radius = std::max(std::sqrt(squared), .001f);
    for (unsigned i = 0; i < 3; ++i) {
        world.m[i][i] = 1 / radius;
        world.m[3][i] = -(data_.bounds.maximum[i] + data_.bounds.minimum[i]) * .5f / radius;
    }
    return world;
}
void DungeonResource::draw(const Matrix& world) {
    ObjectUniforms object;
    object.world = world;
    renderer_.object(object);
    for (auto batch : batches_) {
        renderer_.bind_images({batch.texture, batch.lightmap, 0, 0});
        renderer_.draw_mesh(batch.mesh, VertexPass::lightmap,
                            batch.lightmap ? PixelPass::lightmap : PixelPass::surface,
                            {Blend::alpha, Depth::write, Cull::none, Sampling::anisotropic_wrap});
    }
}
} // namespace genesis
