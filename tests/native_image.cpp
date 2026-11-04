#include "image.h"
#include <stdexcept>
#include <cstdio>

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
        std::puts("Native image decoder checks passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
