#include "dungeon.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
using Bytes = std::vector<std::uint8_t>;
void u32(Bytes& b, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(std::uint8_t(n >> (8 * i)));
}
void f32(Bytes& b, float f) {
    std::uint32_t bits;
    std::memcpy(&bits, &f, 4);
    u32(b, bits);
}
void vector(Bytes& b, float x, float y, float z) {
    f32(b, x);
    f32(b, y);
    f32(b, z);
}
void bounds(Bytes& b) {
    vector(b, -1, -2, -3);
    vector(b, 4, 5, 6);
}
Bytes header() {
    Bytes b;
    bounds(b);
    u32(b, 1);
    std::string name = "stone.dds";
    b.insert(b.end(), name.begin(), name.end());
    b.resize(b.size() + 256 - name.size());
    u32(b, 2);
    u32(b, 1);
    return b;
}
void node(Bytes& b, bool child) {
    vector(b, 1, 2, 3);
    bounds(b);
    // An empty collision bbox in original assets may be inverted.
    vector(b, 10, 10, 10);
    vector(b, -10, -10, -10);
    u32(b, 1);
    u32(b, 0);
    u32(b, 1);
    u32(b, 1);
    u32(b, 3);
    for (unsigned i = 0; i < 3; ++i) {
        vector(b, float(i), float(i + 1), float(i + 2));
        vector(b, 0, 1, 0);
        u32(b, 0xffabcdef);
        f32(b, .25f);
        f32(b, .5f);
        f32(b, .75f);
        f32(b, 1);
    }
    u32(b, 1);
    for (auto i : {0, 1, 2}) {
        b.push_back(std::uint8_t(i));
        b.push_back(0);
    }
    u32(b, 1);
    u32(b, 3);
    for (unsigned i = 0; i < 3; ++i)
        vector(b, float(i), 0, 0);
    u32(b, 1);
    for (auto i : {2, 1, 0}) {
        b.push_back(std::uint8_t(i));
        b.push_back(0);
    }
    for (unsigned i = 0; i < 8; ++i) {
        u32(b, child && i == 3 ? 1 : 0);
        if (child && i == 3)
            node(b, false);
    }
}
void expect(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
void rejected(const Bytes& b) {
    bool failed = false;
    try {
        (void)genesis::decode_dungeon(b);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    expect(failed, "Malformed DG accepted");
}
} // namespace
int main() {
    try {
        auto b = header();
        node(b, true);
        auto dungeon = genesis::decode_dungeon(b);
        expect(dungeon.textures == std::vector<std::string>{"stone.dds"} && dungeon.lightmaps == 2,
               "DG material catalog");
        expect(dungeon.nodes.size() == 2 && dungeon.nodes[0].children[3] == 1 &&
                   dungeon.nodes[0].children[0] == -1,
               "DG octree ownership");
        for (const auto& n : dungeon.nodes) {
            expect(n.meshes.size() == 1 && n.meshes[0].lightmap == 1 && n.meshes[0].texture == 0,
                   "DG mesh grouping");
            const auto& mesh = n.meshes[0];
            expect(mesh.vertices[2].position[0] == 2 && mesh.vertices[1].argb == 0xffabcdef &&
                       mesh.vertices[0].lightmap_uv[0] == .75f,
                   "DG vertex decoding");
            expect(mesh.indices == std::vector<std::uint16_t>{0, 1, 2}, "DG render indices");
            expect(n.collision_indices == std::vector<std::uint16_t>{2, 1, 0} &&
                       n.collision_vertices.size() == 3,
                   "DG node-local collision indices");
            expect(n.collision.minimum[0] > n.collision.maximum[0], "DG empty bounds were altered");
        }
        for (std::size_t length = 0; length < b.size(); ++length)
            rejected(Bytes(b.begin(), b.begin() + length));
        auto invalid = b;
        invalid.push_back(0);
        rejected(invalid);
        // First root group references a nonexistent texture.
        invalid = b;
        std::size_t texture = 28 + 256 + 8 + 60 + 4;
        invalid[texture] = 2;
        rejected(invalid);
        // Non-finite geometry must not enter the scene.
        invalid = b;
        std::size_t vertex = texture + 4 + 4 + 4 + 4;
        invalid[vertex + 2] = 0x80;
        invalid[vertex + 3] = 0x7f;
        rejected(invalid);
        // Index three is outside this mesh's three vertices.
        invalid = b;
        invalid[vertex + 3 * 44 + 4] = 3;
        rejected(invalid);
        std::puts("Native DG octree, lightmap and collision decoding passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
