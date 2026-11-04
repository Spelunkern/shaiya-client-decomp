#include "model.h"
#include <cstring>
#include <stdexcept>
#include <cstdio>

namespace {
using Bytes = std::vector<std::uint8_t>;
void u32(Bytes& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i)
        b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}
void f32(Bytes& b, float f) {
    std::uint32_t v;
    std::memcpy(&v, &f, 4);
    u32(b, v);
}
} // namespace
int main() {
    try {
        Bytes b;
        for (int i = 0; i < 10; ++i)
            f32(b, float(i));
        u32(b, 1);
        u32(b, 6);
        for (auto c : std::string("a.dds"))
            b.push_back(c);
        b.push_back(0);
        u32(b, 3);
        for (int v = 0; v < 3; ++v) {
            for (int i = 0; i < 6; ++i)
                f32(b, float(i + v));
            u32(b, 0xff112233);
            f32(b, 0);
            f32(b, 1);
        }
        u32(b, 1);
        for (auto c : {0, 0, 1, 0, 2, 0})
            b.push_back(c);
        auto model = genesis::decode_smod(b);
        if (model.meshes.size() != 1 || model.meshes[0].texture != "a.dds" ||
            model.meshes[0].vertices[0].argb != 0xff112233 || model.meshes[0].indices[2] != 2)
            throw std::runtime_error("SMOD round-trip failed");
        for (std::size_t n = 0; n < b.size(); ++n) {
            try {
                (void)genesis::decode_smod(Bytes(b.begin(), b.begin() + n));
                throw std::logic_error("Accepted truncated SMOD");
            } catch (const std::runtime_error&) {
            }
        }
        b.back() = 255;
        try {
            (void)genesis::decode_smod(b);
            throw std::logic_error("Accepted invalid mesh index");
        } catch (const std::runtime_error&) {
        }
        std::puts("Native model checks passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
