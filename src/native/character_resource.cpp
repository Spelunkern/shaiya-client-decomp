#include "character_resource.h"
#include "appearance.h"
#include "asset_path.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
namespace genesis {
std::vector<CharacterPart> default_character_parts(std::string_view prefix) {
    std::vector<CharacterPart> result;
    for (const auto* name : {"face", "hair", "upper", "lower", "hand", "foot"})
        result.push_back({std::string(prefix) + "_" + name + ".MLT", 0});
    return result;
}
CharacterResource::CharacterResource(Renderer& renderer, const shaiya::portable::Archive& archive,
                                     const std::vector<CharacterPart>& parts)
    : renderer_(renderer) {
    if (parts.empty())
        throw std::invalid_argument("Character requires mesh parts");
    parts_.reserve(parts.size());
    for (const auto& part : parts) {
        auto table = decode_appearance(archive.read(part.table));
        if (part.appearance >= table.appearances.size())
            throw std::runtime_error("Character appearance is outside table");
        const auto& appearance = table.appearances[part.appearance];
        auto root = asset_path::parent(part.table);
        auto mesh =
            asset_path::join(asset_path::join(root, "3DC"), table.models.at(appearance.model));
        auto texture =
            asset_path::join(asset_path::join(root, "DDS"), table.textures.at(appearance.texture));
        // 00415535..00415585 chooses actor mode two for reflective parts.
        // The shader uses diffuse alpha as reflectivity rather than opacity.
        // Environment/specular scene inputs still belong to the caller.
        parts_.push_back(std::make_unique<SkinResource>(renderer, archive, mesh, texture,
                                                        appearance.flags != 0));
    }
    // FUN_0040d480 loads actor environment images with power-of-two sizing
    // and a generated mip chain.
    const bool reflective = std::any_of(parts_.begin(), parts_.end(),
                                        [](const auto& part) { return part->reflective(); });
    if (reflective && archive.find("data/spheremap.bmp")) {
        TextureLoadOptions options;
        options.interface_power_of_two = true;
        options.generate_mips = true;
        environment_ = renderer.load_texture(archive.read("data/spheremap.bmp"), options);
        for (auto& part : parts_)
            part->set_environment(environment_);
    }
}
CharacterResource::~CharacterResource() {
    parts_.clear();
    if (environment_)
        renderer_.release_texture(environment_);
}
Matrix CharacterResource::fitted_world() const {
    std::array<float, 3> low{}, high{};
    bool first = true;
    for (const auto& part : parts_)
        for (const auto& vertex : part->data().vertices) {
            for (unsigned i = 0; i < 3; ++i) {
                low[i] = first ? vertex.position[i] : std::min(low[i], vertex.position[i]);
                high[i] = first ? vertex.position[i] : std::max(high[i], vertex.position[i]);
            }
            first = false;
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
void CharacterResource::draw_bind_pose(const Matrix& world) {
    for (auto& part : parts_)
        part->draw_bind_pose(world);
}
void CharacterResource::draw(const Matrix& world, const std::vector<Matrix>& finals) {
    // Validate the shared skeleton before drawing any part of the character.
    for (const auto& part : parts_)
        if (part->data().referenced_bones > finals.size())
            throw std::runtime_error("Character part exceeds shared skeleton");
    for (auto& part : parts_)
        part->draw(world, finals);
}
void CharacterResource::draw_shadow_caster(const Matrix& world, const std::vector<Matrix>& finals) {
    for (const auto& part : parts_)
        if (part->data().referenced_bones > finals.size())
            throw std::runtime_error("Character part exceeds shared skeleton");
    for (auto& part : parts_)
        part->draw_shadow_caster(world, finals);
}
} // namespace genesis
