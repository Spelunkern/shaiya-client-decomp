#include "cloth.h"
#include "math.h"
#include <limits>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace genesis {
namespace {
class Decoder {
  public:
    explicit Decoder(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}
    ClothMesh mesh() {
        ClothMesh result;
        auto length = u32();
        if (length > 256)
            throw std::runtime_error("Cloth embedded name exceeds client buffer");
        result.embedded_name = name(take(length), length);
        auto count = u32();
        require_records(count, 40);
        result.vertices.resize(count);
        for (auto& vertex : result.vertices) {
            for (auto& value : vertex.position)
                value = f32(true);
            for (auto& value : vertex.attributes)
                value = u32();
            for (auto& value : vertex.normal)
                value = f32(false);
            for (auto& value : vertex.uv)
                value = f32(false);
        }
        auto triangles = u32();
        require_records(triangles, 6);
        result.indices.reserve(std::size_t(triangles) * 3);
        for (std::size_t i = 0; i < std::size_t(triangles) * 3; ++i) {
            auto p = take(2);
            auto index = std::uint16_t(p[0]) | std::uint16_t(p[1]) << 8;
            if (index >= count)
                throw std::runtime_error("Cloth index exceeds vertex count");
            result.indices.push_back(std::uint16_t(index));
        }
        finish();
        return result;
    }
    ClothCatalog catalog() {
        ClothCatalog result;
        result.dynamic_meshes = references();
        auto count = u32();
        require_records(count, 524);
        result.simulation.resize(count);
        // 0041470e..0041479a copies these first ten words into the
        // cloth owner. The actor draw consumes twenty 24-byte anchors.
        for (auto& record : result.simulation) {
            record.dynamic_mesh = u32();
            record.texture = u32();
            record.mode = u32();
            record.static_mesh = i32();
            record.columns = u32();
            record.rows = u32();
            for (auto& value : record.parameters)
                value = u32();
            record.reserved = u32();
            for (auto& anchor : record.anchors) {
                anchor.control = u32();
                anchor.vertex = i32();
                anchor.bone = i32();
                for (auto& value : anchor.offset)
                    value = f32(false);
            }
        }
        result.static_meshes = references();
        finish();
        return result;
    }

    std::vector<std::string> textures() {
        auto count = i32();
        if (count <= 0)
            throw std::runtime_error("Cloth texture catalog needs a positive count");
        require_records(std::size_t(count), 256);
        std::vector<std::string> result;
        result.reserve(std::size_t(count));
        for (std::int32_t i = 0; i < count; ++i)
            result.push_back(name(take(256), 256));
        finish();
        return result;
    }

  private:
    std::vector<ClothMeshReference> references() {
        auto count = u32();
        require_records(count, 132);
        std::vector<ClothMeshReference> result;
        result.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            auto id = u32();
            result.push_back({id, name(take(128), 128)});
        }
        return result;
    }
    static std::string name(const std::uint8_t* p, std::size_t count) {
        auto end = std::find(p, p + count, 0);
        return std::string(reinterpret_cast<const char*>(p), std::size_t(end - p));
    }
    void require_records(std::size_t count, std::size_t width) const {
        if (count > (bytes_.size() - offset_) / width)
            throw std::runtime_error("Truncated cloth records");
    }
    const std::uint8_t* take(std::size_t count) {
        if (count > bytes_.size() - offset_)
            throw std::runtime_error("Truncated cloth data");
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
        return bits <= 0x7fffffff ? std::int32_t(bits) : -1 - std::int32_t(0xffffffffu - bits);
    }
    float f32(bool finite) {
        auto bits = u32();
        float value;
        static_assert(sizeof(float) == 4);
        std::memcpy(&value, &bits, 4);
        if (finite && !std::isfinite(value))
            throw std::runtime_error("Non-finite cloth position");
        return value;
    }
    void finish() const {
        if (offset_ != bytes_.size())
            throw std::runtime_error("Unconsumed cloth data");
    }
    const std::vector<std::uint8_t>& bytes_;
    std::size_t offset_ = 0;
};
} // namespace
ClothMesh decode_cloth_mesh(const std::vector<std::uint8_t>& bytes) {
    return Decoder(bytes).mesh();
}
ClothCatalog decode_cloth_catalog(const std::vector<std::uint8_t>& bytes) {
    return Decoder(bytes).catalog();
}
std::vector<std::string> decode_cloth_textures(const std::vector<std::uint8_t>& bytes) {
    return Decoder(bytes).textures();
}
ClothTexturePaths cloth_texture_paths(std::uint8_t race) {
    if (race > 3)
        throw std::runtime_error("Unknown cloth texture race");
    static constexpr const char* codes[] = {"HU", "EL", "VI", "DE"};
    const std::string root = "data/cloak/ClothTexture/";
    return {root + "CLOTH_TEXTHRE_" + codes[race] + ".CTL", root + codes[race]};
}
std::vector<ClothPin> cloth_pins(const ClothSetup& setup,
                                 const std::vector<shaiya::graphics::native::Matrix>& finals) {
    // The grid has inclusive bounds: columns+1 by rows+1 (00581040).
    const std::uint64_t width = std::uint64_t(setup.columns) + 1;
    const std::uint64_t height = std::uint64_t(setup.rows) + 1;
    if (height > std::numeric_limits<std::uint64_t>::max() / width)
        throw std::runtime_error("Cloth grid size overflow");
    const auto count = width * height;
    std::vector<ClothPin> result;
    for (const auto& anchor : setup.anchors) {
        if (!anchor.enabled())
            break;
        if (anchor.vertex < 0 || anchor.bone < 0)
            continue;
        if (std::uint64_t(anchor.vertex) >= count || std::size_t(anchor.bone) >= finals.size())
            throw std::runtime_error("Cloth anchor exceeds grid or skeleton");
        shaiya::graphics::native::Matrix local = {
            {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
        for (unsigned i = 0; i < 3; ++i) {
            if (!std::isfinite(anchor.offset[i]))
                throw std::runtime_error("Non-finite active cloth anchor");
            local.m[3][i] = anchor.offset[i];
        }
        auto transformed = multiply(local, finals[std::size_t(anchor.bone)]);
        result.push_back({std::size_t(anchor.vertex),
                          {transformed.m[3][0], transformed.m[3][1], transformed.m[3][2]}});
    }
    return result;
}
} // namespace genesis
