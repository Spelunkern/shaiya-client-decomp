#pragma once
#include "character_resource.h"
#include "character_equipment.h"
#include "cloth_resource.h"
namespace genesis {
// Character selection with recovered clothing/helmet appearance resolution.
// Weapon attachments and actor gameplay remain separate.
class CharacterPreview {
  public:
    CharacterPreview(Renderer& renderer, const shaiya::portable::Archive& archive,
                     std::uint8_t race, std::uint8_t job, std::uint8_t gender, std::uint8_t face,
                     std::uint8_t hair);
    CharacterPreview(Renderer& renderer, const shaiya::portable::Archive& archive,
                     std::uint8_t race, std::uint8_t job, std::uint8_t gender,
                     const CharacterAppearance& appearance,
                     std::optional<CharacterCloak> cloak = std::nullopt);
    void advance(float seconds);
    void draw();
    // Draw in an arbitrary world: `world` places the character's authored
    // (unnormalized) model space, feet at the origin.
    void draw_at(const Matrix& world);
    void draw_shadow_caster_at(const Matrix& world);
    // Bounding radius of the bind-pose character in authored units.
    float radius() const;
    float frame() const {
        return frame_;
    }
    std::int32_t end_frame() const {
        return pose_.data().end;
    }
    const std::string& profile() const {
        return profile_;
    }

  private:
    std::string profile_;
    CharacterResource character_;
    AnimationPose pose_;
    std::unique_ptr<ClothResource> cloak_;
    std::unique_ptr<SkinResource> cloak_static_;
    float frame_ = 0;
};
} // namespace genesis
