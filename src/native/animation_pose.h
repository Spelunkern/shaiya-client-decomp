#pragma once
#include "animation.h"
namespace genesis {
class AnimationPose {
  public:
    explicit AnimationPose(Animation animation);
    std::vector<shaiya::graphics::native::Matrix> sample(float frame) const;
    // 00552cb0 blends the sampled source toward the target's local REST,
    // not toward its first animation key. Phase is measured in five steps.
    std::vector<shaiya::graphics::native::Matrix>
    blend_to_rest(float source_frame, const AnimationPose& target, float phase) const;
    const Animation& data() const {
        return animation_;
    }

  private:
    std::vector<shaiya::graphics::native::Matrix> locals(float frame) const;
    std::vector<shaiya::graphics::native::Matrix>
    compose(const std::vector<shaiya::graphics::native::Matrix>& locals) const;
    Animation animation_;
    std::vector<shaiya::graphics::native::Matrix> rest_;
    std::vector<std::size_t> order_;
};
} // namespace genesis
