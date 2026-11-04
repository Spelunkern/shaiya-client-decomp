#include "animation.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace genesis {
namespace {
class Decoder {
  public:
    explicit Decoder(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}
    Animation read() {
        if (bytes_.size() >= 6 && std::memcmp(bytes_.data(), "ANI_V2", 6) == 0)
            throw std::runtime_error("ANI_V2 requires its client loading contract to be reviewed");
        // 00552910 uses signed FILD for frames. Authored clips include a -1
        // start frame, so treating these as unsigned rejects valid content.
        Animation animation;
        animation.start = i32();
        animation.end = i32();
        if (animation.start > animation.end)
            throw std::runtime_error("Invalid animation frame range");
        auto p = take(2);
        unsigned bones = unsigned(p[0]) | unsigned(p[1]) << 8;
        require(std::size_t(bones) * 76);
        if (bones > 4096)
            throw std::runtime_error("Animation bone limit exceeded");
        animation.bones.resize(bones);
        for (auto& bone : animation.bones) {
            auto parent = i32();
            if (parent >= 0 && std::uint32_t(parent) >= bones)
                throw std::runtime_error("Animation parent exceeds bone count");
            bone.parent = parent;
            for (auto& row : bone.authored.m)
                for (auto& value : row)
                    value = f32();
            bone.rotations = keys<4>();
            bone.translations = keys<3>();
        }
        if (offset_ != bytes_.size())
            throw std::runtime_error("Unconsumed animation records");
        // Validate even disconnected components, without trusting input depth
        // or recursively traversing a potentially cyclic authored hierarchy.
        std::vector<unsigned char> state(bones, 0);
        for (unsigned i = 0; i < bones; ++i) {
            auto at = static_cast<std::int32_t>(i);
            while (at >= 0 && state[at] == 0) {
                state[at] = 1;
                at = animation.bones[at].parent;
            }
            if (at >= 0 && state[at] == 1)
                throw std::runtime_error("Cyclic animation hierarchy");
            at = static_cast<std::int32_t>(i);
            while (at >= 0 && state[at] == 1) {
                state[at] = 2;
                at = animation.bones[at].parent;
            }
        }
        return animation;
    }

  private:
    template <std::size_t N> std::vector<AnimationKey<N>> keys() {
        auto count = u32();
        require(std::size_t(count) * (4 + N * 4));
        std::vector<AnimationKey<N>> result;
        result.resize(count);
        std::int32_t previous = std::numeric_limits<std::int32_t>::min();
        for (auto& key : result) {
            key.frame = i32();
            if (key.frame < previous)
                throw std::runtime_error("Invalid animation key ordering");
            previous = key.frame;
            for (auto& value : key.value)
                value = f32();
        }
        return result;
    }
    void require(std::size_t count) const {
        if (count > bytes_.size() - offset_)
            throw std::runtime_error("Truncated animation");
    }
    const std::uint8_t* take(std::size_t count) {
        require(count);
        auto p = bytes_.data() + offset_;
        offset_ += count;
        return p;
    }
    std::uint32_t u32() {
        auto p = take(4);
        return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 |
               std::uint32_t(p[3]) << 24;
    }
    std::int32_t i32() {
        auto bits = u32();
        std::int32_t value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
    float f32() {
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
        auto bits = u32();
        float value;
        std::memcpy(&value, &bits, 4);
        if (!std::isfinite(value))
            throw std::runtime_error("Non-finite animation matrix/key");
        return value;
    }
    const std::vector<std::uint8_t>& bytes_;
    std::size_t offset_ = 0;
};
} // namespace
Animation decode_animation(const std::vector<std::uint8_t>& bytes) {
    return Decoder(bytes).read();
}
} // namespace genesis
