#include "image.h"
#include <cstdlib>
#include <stdexcept>
#include <cstdio>
#include <utility>

using Bytes = std::vector<std::uint8_t>;
void u32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (i * 8));
}
void expect(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
int main() {
    try {
        // Bottom-origin TGA: bottom row blue, top row red. Decode to top-left.
        Bytes tga(18, 0);
        tga[2] = 2;
        tga[12] = 1;
        tga[14] = 2;
        tga[16] = 24;
        tga.insert(tga.end(), {255, 0, 0, 0, 0, 255});
        auto image = genesis::decode_image(tga);
        expect(image.width == 1 && image.height == 2 &&
                   image.pixels == Bytes({255, 0, 0, 255, 0, 0, 255, 255}),
               "TGA orientation/BGR conversion");
        tga[2] = 10;
        tga.resize(18);
        tga.insert(tga.end(), {0x81, 0, 255, 0});
        image = genesis::decode_image(tga);
        expect(image.pixels == Bytes({0, 255, 0, 255, 0, 255, 0, 255}), "TGA RLE");
        tga.pop_back();
        try {
            (void)genesis::decode_image(tga);
            throw std::logic_error("Accepted truncated TGA");
        } catch (const std::runtime_error&) {
        }
        // Single BC1 block: red first endpoint, all selectors zero.
        Bytes dds(136, 0);
        u32(dds, 0, 0x20534444);
        u32(dds, 4, 124);
        u32(dds, 12, 4);
        u32(dds, 16, 4);
        u32(dds, 76, 32);
        u32(dds, 80, 4);
        u32(dds, 84, 0x31545844);
        dds[128] = 0;
        dds[129] = 0xf8;
        image = genesis::decode_image(dds);
        expect(image.width == 4 && image.height == 4, "DDS dimensions");
        for (std::size_t i = 0; i < 64; i += 4)
            expect(image.pixels[i] == 255 && image.pixels[i + 1] == 0 && image.pixels[i + 2] == 0 &&
                       image.pixels[i + 3] == 255,
                   "BC1 endpoint decode");
        // Three-color BC1 selector 3 must be transparent.
        dds[128] = dds[129] = 0;
        dds[130] = 255;
        dds[131] = 255;
        for (int i = 132; i < 136; ++i)
            dds[i] = 255;
        image = genesis::decode_image(dds);
        for (std::size_t i = 3; i < 64; i += 4)
            expect(image.pixels[i] == 0, "BC1 alpha decode");
        for (std::size_t size = 0; size < 136; ++size) {
            Bytes truncated(dds.begin(), dds.begin() + size);
            try {
                (void)genesis::decode_image(truncated);
                throw std::logic_error("Accepted truncated DDS");
            } catch (const std::runtime_error&) {
            }
        }
        expect(genesis::texture_mip_count(1, 1) == 1, "Mip count of a 1x1 texture");
        expect(genesis::texture_mip_count(256, 256) == 9, "Mip count of a 256x256 texture");
        expect(genesis::texture_mip_count(256, 32) == 9, "Mip count follows the larger dimension");
        expect(genesis::texture_power_of_two(100) == 128, "Power-of-two rounds up");
        expect(genesis::texture_power_of_two(128) == 128, "Power-of-two is stable at an exact power");

        genesis::Image single;
        single.width = single.height = 1;
        single.pixels = {40, 84, 120, 200};
        auto mip = genesis::texture_box_mip(single);
        expect(mip.width == 1 && mip.height == 1, "Box mip of a 1x1 source clamps to 1x1");
        expect(mip.pixels == single.pixels,
               "Box mip of a single pixel reproduces it exactly (edge taps clamp)");

        genesis::Image checker;
        checker.width = checker.height = 2;
        checker.pixels = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
        auto checker_mip = genesis::texture_box_mip(checker);
        expect(checker_mip.width == 1 && checker_mip.height == 1, "Box mip of a 2x2 source is 1x1");
        expect(checker_mip.pixels[0] == 128 && checker_mip.pixels[1] == 128 &&
                   checker_mip.pixels[2] == 128 && checker_mip.pixels[3] == 255,
               "Box mip averages all four source taps");

        genesis::Image solid;
        solid.width = 4;
        solid.height = 4;
        solid.pixels.assign(4 * 4 * 4, 0);
        for (std::size_t i = 0; i < solid.pixels.size(); i += 4) {
            solid.pixels[i] = 10;
            solid.pixels[i + 1] = 90;
            solid.pixels[i + 2] = 200;
            solid.pixels[i + 3] = 255;
        }
        for (auto [w, h] : {std::pair{2u, 2u}, std::pair{6u, 6u}, std::pair{3u, 5u}}) {
            auto resized = genesis::texture_resize_triangle(solid, w, h);
            expect(resized.width == w && resized.height == h, "Resized dimensions match the request");
            for (std::size_t i = 0; i < resized.pixels.size(); i += 4) {
                expect(std::abs(int(resized.pixels[i]) - 10) <= 1 &&
                           std::abs(int(resized.pixels[i + 1]) - 90) <= 1 &&
                           std::abs(int(resized.pixels[i + 2]) - 200) <= 1 &&
                           resized.pixels[i + 3] == 255,
                       "Resizing a solid-color image preserves its color");
            }
        }
        genesis::Image halves;
        halves.width = 2;
        halves.height = 1;
        halves.pixels = {0, 0, 0, 255, 255, 255, 255, 255};
        auto blended = genesis::texture_resize_triangle(halves, 1, 1);
        expect(blended.width == 1 && blended.height == 1, "Downscale to 1x1");
        expect(blended.pixels[0] >= 120 && blended.pixels[0] <= 135,
               "Triangle filter blends both source columns instead of nearest-sampling");

        genesis::Image keyed;
        keyed.width = 2;
        keyed.height = 1;
        keyed.pixels = {255, 0, 255, 255, 10, 20, 30, 255};
        genesis::texture_apply_color_key(keyed, 0xff00ff);
        expect(keyed.pixels[3] == 0, "Matching color key clears alpha");
        expect(keyed.pixels[7] == 255, "Non-matching pixel keeps its alpha");

        std::puts("Native image decoder checks passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
