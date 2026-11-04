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

// Mirrors TextureLoadOptions in native_resources.h.
struct TextureLoadOptions {
    bool generate_mips = false;
    bool interface_power_of_two = false;
    bool color_key_enabled = false;
    std::uint32_t color_key = 0;
    std::uint32_t requested_width = 0, requested_height = 0;
    std::uint32_t level_limit = 0;
};
// Ports quality_options() from native_resource_ports.cpp: archive textures are
// always interface-power-of-two; 0 and 0xffffffff sizes mean "authored size",
// levels 1 disables mip generation, 2..15 limits it, a non-zero key enables
// the 24-bit color key.
TextureLoadOptions texture_quality_options(std::uint32_t width, std::uint32_t height,
                                           std::uint32_t levels, std::uint32_t color_key);
// Full level chain produced by the original load_texture policy: requested or
// power-of-two sizing, DDS-authored levels versus box-filtered generated
// levels, level limit, per-level color key and triangle resize. All levels
// are top-left RGBA8. Throws std::runtime_error/invalid_argument on failure.
std::vector<Image> decode_texture_levels(const std::vector<std::uint8_t>& encoded,
                                         const TextureLoadOptions& options);
// Lossless PNG (8-bit RGBA, stored deflate blocks; no zlib dependency) for
// screenshots. Replaces the original's WIC JPEG capture.
std::vector<std::uint8_t> encode_png(const Image& image);
} // namespace genesis
