#include "dungeon.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace genesis {
namespace {
class Decoder {
  public:
    explicit Decoder(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}
    Dungeon read() {
        Dungeon result;
        result.bounds = bounds();
        auto count = u32();
        require(std::size_t(count) * 256);
        if (count > 4096)
            fail("DG texture count exceeds limit");
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto* value = take(256);
            std::size_t length = 0;
            while (length < 256 && value[length])
                ++length;
            result.textures.emplace_back(reinterpret_cast<const char*>(value), length);
        }
        result.lightmaps = u32();
        if (result.lightmaps > 4096)
            fail("DG lightmap count exceeds limit");
        if (u32())
            node(result, 0);
        if (offset_ != bytes_.size())
            fail("Unconsumed DG records");
        return result;
    }

  private:
    [[noreturn]] static void fail(const char* message) {
        throw std::runtime_error(message);
    }
    void require(std::size_t size) const {
        if (size > bytes_.size() - offset_)
            fail("Truncated DG");
    }
    const std::uint8_t* take(std::size_t size) {
        require(size);
        auto p = bytes_.data() + offset_;
        offset_ += size;
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
        float f;
        std::memcpy(&f, &bits, 4);
        if (finite && !std::isfinite(f))
            fail("Non-finite DG position/bounds");
        return f;
    }
    std::array<float, 3> vec3() {
        return {f32(), f32(), f32()};
    }
    // Empty octants carry inverted sentinel bounds. Preserve these rather than
    // expanding them into geometry during culling/collision setup.
    Bounds bounds() {
        return {vec3(), vec3()};
    }
    std::vector<std::uint16_t> indices(std::size_t vertices) {
        auto triangles = u32();
        require(std::size_t(triangles) * 6);
        std::vector<std::uint16_t> result;
        result.reserve(std::size_t(triangles) * 3);
        for (std::size_t i = 0; i < std::size_t(triangles) * 3; ++i) {
            auto p = take(2);
            auto index = std::uint16_t(p[0]) | std::uint16_t(p[1]) << 8;
            if (std::size_t(index) >= vertices)
                fail("DG index exceeds local vertex count");
            result.push_back(std::uint16_t(index));
        }
        return result;
    }
    std::int32_t node(Dungeon& result, unsigned depth) {
        if (depth > 64 || result.nodes.size() >= 1000000)
            fail("DG octree limit exceeded");
        auto index = static_cast<std::int32_t>(result.nodes.size());
        result.nodes.emplace_back();
        // Keep the node local while recursion may reallocate the node vector.
        DungeonNode current;
        current.center = vec3();
        current.view = bounds();
        current.collision = bounds();
        auto groups = u32();
        if (groups > 4096)
            fail("DG mesh group limit exceeded");
        for (std::uint32_t group = 0; group < groups; ++group) {
            auto texture = u32(), count = u32();
            if (texture >= result.textures.size() || count > 4096)
                fail("Invalid DG mesh group");
            for (std::uint32_t m = 0; m < count; ++m) {
                DungeonMesh mesh;
                mesh.texture = texture;
                mesh.lightmap = u32();
                auto vertices = u32();
                require(std::size_t(vertices) * 44);
                mesh.vertices.resize(vertices);
                for (auto& vertex : mesh.vertices) {
                    for (auto& f : vertex.position)
                        f = f32();
                    for (auto& f : vertex.normal)
                        f = f32(false);
                    vertex.argb = u32();
                    for (auto& f : vertex.uv)
                        f = f32(false);
                    for (auto& f : vertex.lightmap_uv)
                        f = f32(false);
                }
                mesh.indices = indices(vertices);
                current.meshes.push_back(std::move(mesh));
            }
        }
        auto collision = u32();
        if (collision > 1)
            fail("Unsupported DG collision record");
        if (collision) {
            auto vertices = u32();
            require(std::size_t(vertices) * 12);
            current.collision_vertices.reserve(vertices);
            for (std::uint32_t i = 0; i < vertices; ++i)
                current.collision_vertices.push_back(vec3());
            current.collision_indices = indices(vertices);
        }
        for (auto& child : current.children)
            if (u32())
                child = node(result, depth + 1);
        result.nodes[index] = std::move(current);
        return index;
    }
    const std::vector<std::uint8_t>& bytes_;
    std::size_t offset_ = 0;
};
} // namespace
Dungeon decode_dungeon(const std::vector<std::uint8_t>& bytes) {
    return Decoder(bytes).read();
}
} // namespace genesis
