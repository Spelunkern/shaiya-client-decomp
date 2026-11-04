#pragma once
#include "animation_pose.h"
#include "skin_resource.h"
namespace genesis {
struct CharacterPart {
    std::string table;
    std::uint32_t appearance = 0;
};
class CharacterResource {
  public:
    CharacterResource(Renderer& renderer, const shaiya::portable::Archive& archive,
                      const std::vector<CharacterPart>& parts);
    Matrix fitted_world() const;
    void draw_bind_pose(const Matrix& world);
    void draw(const Matrix& world, const std::vector<Matrix>& skeleton_finals);
    std::size_t part_count() const {
        return parts_.size();
    }

  private:
    std::vector<std::unique_ptr<SkinResource>> parts_;
};
// Base part tables for the viewer and character preview. CharacterPreview
// applies recovered face/hair and equipment selectors before loading resources.
std::vector<CharacterPart> default_character_parts(std::string_view prefix);
} // namespace genesis
