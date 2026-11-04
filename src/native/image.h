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

// Original quality-policy algorithms from native_resources.cpp/
// native_texture_decode.inc, ported to operate on decoded RGBA8 bytes
// instead of a D3D11 texture resource.
std::uint32_t texture_mip_count(std::uint32_t width, std::uint32_t height);
std::uint32_t texture_power_of_two(std::uint32_t size);
// 2x2 box-filtered half-size mip; edge taps clamp to the last row/column.
Image texture_box_mip(const Image& source);
// Separable triangle-filter resize; out-of-range taps wrap, matching the
// original's modulo sample addressing.
Image texture_resize_triangle(const Image& source, std::uint32_t width, std::uint32_t height);
// Zeroes alpha for every pixel whose RGB exactly matches the 24-bit key.
void texture_apply_color_key(Image& image, std::uint32_t color);
} // namespace genesis
