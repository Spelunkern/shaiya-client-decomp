#include "character_preview.h"
#include "character_profile.h"
#include "action.h"
#include "asset_path.h"
#include <cmath>
namespace genesis {
namespace {
std::vector<CharacterPart> parts(const std::string& profile,
                                 const CharacterAppearance& appearance) {
    auto result = default_character_parts(profile);
    if (appearance.helmet)
        result[1].table = profile + "_helmet.MLT";
    for (unsigned i = 0; i < result.size(); ++i)
        result[i].appearance = appearance.indices[i];
    return result;
}
Animation normal(const shaiya::portable::Archive& archive, const std::string& profile) {
    auto actions = decode_actions(archive.read(profile + "_action.alt"));
    if (actions.slots.empty() || actions.slots[0].animation.empty() || actions.slots[0].mode() != 1)
        throw std::runtime_error("Unsupported character-selection normal action");
    return decode_animation(archive.read(asset_path::join(
        asset_path::join(asset_path::parent(profile), "ANI"), actions.slots[0].animation)));
}
} // namespace
CharacterPreview::CharacterPreview(Renderer& renderer, const shaiya::portable::Archive& archive,
                                   std::uint8_t race, std::uint8_t job, std::uint8_t gender,
                                   std::uint8_t face, std::uint8_t hair)
    : CharacterPreview(renderer, archive, race, job, gender,
                       CharacterAppearance{{face, hair, 0, 0, 0, 0}, false}) {}
CharacterPreview::CharacterPreview(Renderer& renderer, const shaiya::portable::Archive& archive,
                                   std::uint8_t race, std::uint8_t job, std::uint8_t gender,
                                   const CharacterAppearance& appearance,
                                   std::optional<CharacterCloak> cloak)
    : profile_(character_profile(race, job, gender)),
      character_(renderer, archive, parts(profile_, appearance)), pose_(normal(archive, profile_)) {
    if (cloak) {
        auto path = character_cloth_catalog(race, job, gender, cloak->variant);
        auto catalog = decode_cloth_catalog(archive.read(path));
        const auto& setup = catalog.simulation.at(cloak->model);
        auto texture_paths = cloth_texture_paths(race);
        auto textures = decode_cloth_textures(archive.read(texture_paths.catalog));
        auto texture = asset_path::join(texture_paths.directory, textures.at(setup.texture));
        auto directory = asset_path::join(asset_path::parent(path), "3DC");
        cloak_ = std::make_unique<ClothResource>(
            renderer, archive, setup,
            asset_path::join(directory, catalog.dynamic_meshes.at(setup.dynamic_mesh).name),
            texture);
        if (setup.static_mesh >= 0)
            cloak_static_ = std::make_unique<SkinResource>(
                renderer, archive,
                asset_path::join(asset_path::join(directory, "Static"),
                                 catalog.static_meshes.at(std::size_t(setup.static_mesh)).name),
                texture);
    }
}
void CharacterPreview::advance(float seconds) {
    if (!std::isfinite(seconds) || seconds < 0)
        throw std::invalid_argument("Invalid character-preview delta");
    // Ordinary state/normal action: 0041be09 advances at 30, stores float,
    // then 0041be1c/0041be75 resets mode one to ZERO strictly after clip end.
    // Drop overshoot; do not use the asset viewer's modulo preview policy.
    frame_ = float(static_cast<long double>(frame_) + static_cast<long double>(seconds) * 30);
    if (frame_ > float(pose_.data().end))
        frame_ = 0;
}
void CharacterPreview::draw() {
    auto world = character_.fitted_world();
    world.m[3][0] += .35f;
    auto finals = pose_.sample(frame_);
    character_.draw(world, finals);
    if (cloak_) {
        if (cloak_static_)
            cloak_static_->draw(world, finals);
        cloak_->pin(finals);
        // Column fallback follows animated anchors without needing a world
        // collision/wind scene. Full cloth solvers are exercised separately.
        cloak_->draw(world, true);
    }
}
} // namespace genesis
