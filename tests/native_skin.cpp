#include "skin.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <stdexcept>
namespace {
using namespace genesis;
using Matrix = shaiya::graphics::native::Matrix;
using Bytes = std::vector<std::uint8_t>;
void u32(Bytes& b, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(std::uint8_t(n >> (8 * i)));
}
void f32(Bytes& b, float f) {
    std::uint32_t n;
    std::memcpy(&n, &f, 4);
    u32(b, n);
}
void expect(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
void reject(const Bytes& b) {
    bool failed = false;
    try {
        (void)decode_skin(b);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    expect(failed, "Malformed skin accepted");
}
Bytes fixture() {
    Bytes b;
    u32(b, 0);
    u32(b, 2);
    for (unsigned bone = 0; bone < 2; ++bone)
        for (unsigned row = 0; row < 4; ++row)
            for (unsigned column = 0; column < 4; ++column)
                f32(b, row == 3 && column == 0 ? float(bone + 1) : (row == column ? 1.f : 0.f));
    u32(b, 3);
    for (unsigned i = 0; i < 3; ++i) {
        for (auto value : {float(i), float(i + 1), float(i + 2), .25f})
            f32(b, value);
        // Unused bytes are not extra influences in the original client.
        b.insert(b.end(), {0, 1, 254, 255});
        for (auto value : {0.f, 1.f, 0.f, .5f, .75f})
            f32(b, value);
    }
    u32(b, 1);
    b.insert(b.end(), {0, 0, 1, 0, 2, 0});
    return b;
}
} // namespace
int main() {
    try {
        auto b = fixture();
        auto model = genesis::decode_skin(b);
        expect(model.inverse_bind.size() == 2 && model.vertices.size() == 3 &&
                   model.indices == std::vector<std::uint16_t>{0, 1, 2},
               "Skin counts/indices");
        expect(model.inverse_bind[1].m[3][0] == 2 && model.inverse_bind[1].m[0][3] == 0,
               "Skin matrix row-vector convention");
        expect(model.vertices[2].position[0] == 2 && model.vertices[1].weight == .25f &&
                   model.vertices[1].bones[1] == 1 && model.vertices[0].bones[2] == 254 &&
                   model.vertices[0].uv[1] == .75f,
               "Skin authored vertex offsets");
        for (std::size_t i = 0; i < b.size(); ++i)
            reject(Bytes(b.begin(), b.begin() + i));
        auto invalid = b;
        invalid.push_back(0);
        reject(invalid);
        invalid = b;
        invalid[8 + 128 + 4 + 16] = 2;
        reject(invalid);
        invalid = b;
        invalid[invalid.size() - 2] = 3;
        reject(invalid);
        invalid = b;
        invalid[0] = 0xbc;
        invalid[1] = 1;
        reject(invalid);
        invalid = b;
        invalid[8 + 128 + 4 + 14] = 0x80;
        invalid[8 + 128 + 4 + 15] = 0x7f;
        reject(invalid);
        std::vector<Matrix> finals(2);
        for (auto& matrix : finals) {
            for (unsigned i = 0; i < 4; ++i)
                matrix.m[i][i] = 1;
            matrix.m[0][0] = 2;
        }
        auto palette = genesis::skin_palette(model, finals);
        expect(palette[0].m[3][0] == 2 && palette[1].m[3][0] == 4,
               "Skin inverse-bind/animation multiplication order");
        model.inverse_bind.push_back(model.inverse_bind.back());
        expect(genesis::skin_palette(model, finals).size() == 2,
               "Unused inverse binds demanded nonexistent animation bones");
        float weighted = .25f * (1 * palette[0].m[0][0] + palette[0].m[3][0]) +
                         .75f * (1 * palette[1].m[0][0] + palette[1].m[3][0]);
        expect(std::abs(weighted - 5.5f) < .0001f, "Recovered two-bone blend");
        bool rejected = false;
        try {
            (void)genesis::skin_palette(model, {});
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        expect(rejected, "Short animation palette accepted");
        std::puts("Native 40-byte skin, inverse-bind and two-bone contracts passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
