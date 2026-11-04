#include "skin.h"
#include "math.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace genesis {
namespace {
class Decoder {
  public:
    explicit Decoder(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}
    SkinnedModel read() {
        SkinnedModel model;
        model.version = u32();
        if (model.version == 444)
            throw std::runtime_error("EP6 four-weight skin requires a separate vertex contract");
        auto bones = u32();
        require(std::size_t(bones) * 64);
        if (!bones || bones > 256)
            throw std::runtime_error("Skin bone count is outside the client palette");
        model.inverse_bind.resize(bones);
        for (auto& matrix : model.inverse_bind)
            for (auto& row : matrix.m)
                for (auto& value : row)
                    value = f32();
        auto vertices = u32();
        require(std::size_t(vertices) * 40);
        model.vertices.resize(vertices);
        for (auto& vertex : model.vertices) {
            for (auto& value : vertex.position)
                value = f32();
            vertex.weight = f32();
            if (vertex.weight < 0 || vertex.weight > 1)
                throw std::runtime_error("Skin weight is outside [0,1]");
            for (auto& bone : vertex.bones)
                bone = *take(1);
            // The recovered CPU and GPU paths use offsets 0x10/0x11 only.
            if (vertex.bones[0] >= bones || vertex.bones[1] >= bones)
                throw std::runtime_error("Skin bone index exceeds palette");
            model.referenced_bones =
                std::max(model.referenced_bones,
                         std::size_t(std::max(vertex.bones[0], vertex.bones[1])) + 1);
            for (auto& value : vertex.normal)
                value = f32(false);
            for (auto& value : vertex.uv)
                value = f32(false);
        }
        auto triangles = u32();
        require(std::size_t(triangles) * 6);
        model.indices.reserve(std::size_t(triangles) * 3);
        for (std::size_t i = 0; i < std::size_t(triangles) * 3; ++i) {
            auto p = take(2);
            std::uint16_t index = std::uint16_t(p[0]) | std::uint16_t(p[1]) << 8;
            if (index >= vertices)
                throw std::runtime_error("Skin index exceeds vertex count");
            model.indices.push_back(index);
        }
        if (offset_ != bytes_.size())
            throw std::runtime_error("Unconsumed skin records (different mesh contract)");
        return model;
    }

  private:
    void require(std::size_t count) const {
        if (count > bytes_.size() - offset_)
            throw std::runtime_error("Truncated skin");
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
    float f32(bool finite = true) {
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
        auto bits = u32();
        float value;
        std::memcpy(&value, &bits, 4);
        if (finite && !std::isfinite(value))
            throw std::runtime_error("Non-finite skin position/matrix/weight");
        return value;
    }
    const std::vector<std::uint8_t>& bytes_;
    std::size_t offset_ = 0;
};
} // namespace
SkinnedModel decode_skin(const std::vector<std::uint8_t>& bytes) {
    return Decoder(bytes).read();
}
std::vector<shaiya::graphics::native::Matrix>
skin_palette(const SkinnedModel& model,
             const std::vector<shaiya::graphics::native::Matrix>& finals) {
    if (finals.size() < model.referenced_bones)
        throw std::runtime_error("Animation skeleton is smaller than skin");
    std::vector<shaiya::graphics::native::Matrix> result;
    auto count = std::min(finals.size(), model.inverse_bind.size());
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        result.push_back(multiply(model.inverse_bind[i], finals[i]));
    return result;
}
} // namespace genesis
