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

        // Level-chain policy (load_texture): 4x4 BMP-free path via TGA.
        Bytes quad(18, 0);
        quad[2] = 2;
        quad[12] = 4;
        quad[14] = 4;
        quad[16] = 24;
        for (int i = 0; i < 16; ++i)
            quad.insert(quad.end(), {10, 20, 30});
        genesis::TextureLoadOptions options;
        auto chain = genesis::decode_texture_levels(quad, options);
        expect(chain.size() == 1 && chain[0].width == 4, "Default policy keeps one level");
        options.generate_mips = true;
        chain = genesis::decode_texture_levels(quad, options);
        expect(chain.size() == 3 && chain[1].width == 2 && chain[2].width == 1 &&
                   chain[2].pixels[0] == 30,
               "generate_mips builds a box-filtered chain down to 1x1");
        options.level_limit = 2;
        chain = genesis::decode_texture_levels(quad, options);
        expect(chain.size() == 2, "level_limit truncates the chain");
        options = {};
        options.requested_width = 8;
        options.requested_height = 2;
        chain = genesis::decode_texture_levels(quad, options);
        expect(chain.size() == 1 && chain[0].width == 8 && chain[0].height == 2,
               "requested size resizes with the triangle filter");
        options = {};
        options.interface_power_of_two = true;
        quad[12] = 3;
        quad.resize(18 + 3 * 4 * 3);
        chain = genesis::decode_texture_levels(quad, options);
        expect(chain.size() == 1 && chain[0].width == 4 && chain[0].height == 4,
               "interface textures round up to powers of two with one level");
        options = {};
        options.color_key_enabled = true;
        options.color_key = 0x1e140a; // stored as B,G,R 10,20,30 -> RGB 30,20,10
        chain = genesis::decode_texture_levels(quad, options);
        expect(chain[0].pixels[3] == 0, "color key applies to decoded levels");
        options = {};
        options.level_limit = 16;
        try {
            (void)genesis::decode_texture_levels(quad, options);
            throw std::logic_error("Accepted level_limit above the maximum");
        } catch (const std::invalid_argument&) {
        }

        auto quality = genesis::texture_quality_options(0xffffffffu, 64, 1, 0);
        expect(quality.interface_power_of_two && !quality.generate_mips &&
                   quality.requested_width == 0 && quality.requested_height == 64 &&
                   quality.level_limit == 1 && !quality.color_key_enabled,
               "Quality options with levels 1");
        quality = genesis::texture_quality_options(0, 0, 0, 0xff00ff);
        expect(quality.generate_mips && quality.level_limit == 0 && quality.color_key_enabled &&
                   quality.color_key == 0xff00ff,
               "Quality options with full chain and key");
        quality = genesis::texture_quality_options(0, 0, 16, 0);
        expect(quality.generate_mips && quality.level_limit == 0, "Levels above 15 are ignored");

        genesis::Image png;
        png.width = 3;
        png.height = 2;
        png.pixels.assign(24, 0);
        for (std::size_t i = 0; i < 24; ++i)
            png.pixels[i] = std::uint8_t(i * 10);
        const auto encoded = genesis::encode_png(png);
        expect(encoded.size() > 40 && encoded[1] == 'P' && encoded[12] == 'I', "PNG framing");
        if (const char* path = std::getenv("GENESIS_PNG_OUT")) {
            std::FILE* file = std::fopen(path, "wb");
            expect(file != nullptr, "PNG output path");
            std::fwrite(encoded.data(), 1, encoded.size(), file);
            std::fclose(file);
        }

        std::puts("Native image decoder checks passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
