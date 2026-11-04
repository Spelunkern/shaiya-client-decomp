#pragma once
#include <cstdint>
#include <vector>
namespace genesis {
struct Image {
    std::uint32_t width = 0, height = 0;
    std::vector<std::uint8_t> pixels; // top-left RGBA8
};
// DDS BC1/2/3 and packed RGB, TGA (including RLE), and uncompressed BMP.
// No Win32, WIC, IJL, or 32-bit runtime dependencies.
Image decode_image(const std::vector<std::uint8_t>& encoded);
} // namespace genesis
