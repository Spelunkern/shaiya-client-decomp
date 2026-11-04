#include "model.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace genesis {
namespace {
struct Reader {
    const std::vector<std::uint8_t>& bytes;
    std::size_t offset = 0;
    void require(std::size_t size) const {
        if (offset > bytes.size() || size > bytes.size() - offset)
            throw std::runtime_error("Truncated SMOD");
    }
    std::uint32_t u32() {
        require(4);
        auto p = bytes.data() + offset;
        offset += 4;
        return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 |
               std::uint32_t(p[3]) << 24;
    }
    float f32(bool require_finite = true) {
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
        auto bits = u32();
        float result;
        std::memcpy(&result, &bits, 4);
        if (require_finite && !std::isfinite(result))
            throw std::runtime_error("Non-finite SMOD coordinate");
        return result;
    }
    std::string name() {
        auto size = u32();
        if (size > 256)
            throw std::runtime_error("Invalid SMOD texture name length");
        require(size);
        std::string result(reinterpret_cast<const char*>(bytes.data() + offset), size);
        offset += size;
        if (!result.empty() && result.back() == 0)
            result.pop_back();
        if (result.find('\0') != std::string::npos)
            throw std::runtime_error("Embedded NUL in SMOD texture name");
        return result;
    }
};
} // namespace
StaticModel decode_smod(const std::vector<std::uint8_t>& bytes) {
    Reader r{bytes};
    StaticModel result;
    for (auto& f : result.sphere)
        f = r.f32();
    for (auto& f : result.minimum)
        f = r.f32();
    for (auto& f : result.maximum)
        f = r.f32();
    auto meshes = r.u32();
    if (meshes > 65536 || meshes > (bytes.size() - r.offset) / 12)
        throw std::runtime_error("Invalid SMOD mesh count");
    result.meshes.reserve(meshes);
    for (std::uint32_t m = 0; m < meshes; ++m) {
        StaticMesh mesh;
        mesh.texture = r.name();
        auto count = r.u32();
        r.require(std::size_t(count) * sizeof(shaiya::graphics::native::MeshVertex));
        mesh.vertices.resize(count);
        for (auto& v : mesh.vertices) {
            for (float& f : v.position)
                f = r.f32();
            // Original assets include NaN normals on degenerate triangles.
            // Retain their bits; unlit geometry does not consume the normals.
            for (float& f : v.normal)
                f = r.f32(false);
            // Untextured authored meshes can also contain unused NaN UVs.
            v.argb = r.u32();
            for (float& f : v.uv)
                f = r.f32(false);
        }
        auto triangles = r.u32();
        r.require(std::size_t(triangles) * 6);
        mesh.indices.resize(std::size_t(triangles) * 3);
        for (auto& index : mesh.indices) {
            index = std::uint16_t(bytes[r.offset]) | std::uint16_t(bytes[r.offset + 1]) << 8;
            r.offset += 2;
            if (index >= count)
                throw std::runtime_error("SMOD index exceeds vertex count");
        }
        result.meshes.push_back(std::move(mesh));
    }
    result.trailing_data.assign(bytes.begin() + static_cast<std::ptrdiff_t>(r.offset), bytes.end());
    return result;
}
ModelCollision decode_model_collision(const StaticModel& model) {
    const auto& bytes = model.trailing_data;
    ModelCollision result;
    auto word = [&](std::size_t at) {
        if (at + 4 > bytes.size())
            throw std::invalid_argument("Truncated model collision data");
        return std::uint32_t(bytes[at]) | std::uint32_t(bytes[at + 1]) << 8 |
               std::uint32_t(bytes[at + 2]) << 16 | std::uint32_t(bytes[at + 3]) << 24;
    };
    auto real = [&](std::size_t at) {
        auto bits = word(at);
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    };
    if (bytes.size() < 28)
        throw std::invalid_argument("Truncated model collision data");
    for (std::size_t i = 0; i < 3; ++i) {
        result.minimum[i] = real(4 * i);
        result.maximum[i] = real(12 + 4 * i);
    }
    if (word(24) == 0)
        return result;
    std::uint64_t vertex_count = word(28);
    std::size_t at = 32;
    if (vertex_count > (bytes.size() - at) / 12)
        throw std::invalid_argument("Truncated model collision vertices");
    for (std::uint64_t i = 0; i < vertex_count; ++i, at += 12)
        result.vertices.push_back({real(at), real(at + 4), real(at + 8)});
    std::uint64_t triangles = word(at);
    at += 4;
    if (triangles * 6 > bytes.size() - at)
        throw std::invalid_argument("Truncated model collision indices");
    for (std::uint64_t i = 0; i < triangles * 3; ++i, at += 2) {
        std::uint16_t index = std::uint16_t(bytes[at] | bytes[at + 1] << 8);
        if (index >= vertex_count)
            throw std::invalid_argument("Model collision index out of range");
        result.indices.push_back(index);
    }
    return result;
}
} // namespace genesis
