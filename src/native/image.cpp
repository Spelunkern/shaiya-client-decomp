#include "image.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <cstddef>

// CPU asset algorithms ported from graphics/native_texture_decode.inc.
// Disk formats stay fixed-width; allocation and arithmetic use the host CRT.
namespace genesis {
namespace {
using std::size_t;
using DecodeStatus = int;
constexpr DecodeStatus ok = 0, invalid = -1, unsupported = -2, no_memory = -3;
bool failed(DecodeStatus status) {
    return status < 0;
}
constexpr std::uint32_t maximum_dimension = 16384, maximum_levels = 15;
void* allocate(size_t bytes) {
    return bytes && bytes <= 256 * 1024 * 1024 ? std::calloc(1, bytes) : nullptr;
}
void free_bytes(void* p) {
    std::free(p);
}
void copy_bytes(void* to, const void* from, size_t count) {
    std::memmove(to, from, count);
}
struct TextureMip {
    const void* bytes = nullptr;
    std::uint32_t row_pitch = 0, slice_pitch = 0;
};
enum class Format {
    A8_UNORM = 0x41,
    B4G4R4A4_UNORM = 0x73,
    B5G5R5A1_UNORM = 0x56,
    B5G6R5_UNORM = 0x55,
    B8G8R8A8_UNORM = 0x57,
    B8G8R8A8_UNORM_SRGB = 0x5b,
    B8G8R8X8_UNORM = 0x58,
    B8G8R8X8_UNORM_SRGB = 0x5d,
    BC1_UNORM = 0x47,
    BC1_UNORM_SRGB = 0x48,
    BC2_UNORM = 0x4a,
    BC2_UNORM_SRGB = 0x4b,
    BC3_UNORM = 0x4d,
    BC3_UNORM_SRGB = 0x4e,
    BC4_SNORM = 0x51,
    BC4_UNORM = 0x50,
    BC5_SNORM = 0x54,
    BC5_UNORM = 0x53,
    BC6H_SF16 = 0x60,
    BC6H_UF16 = 0x5f,
    BC7_UNORM = 0x62,
    BC7_UNORM_SRGB = 0x63,
    D24_UNORM_S8_UINT = 0x2d,
    R10G10B10A2_UNORM = 0x18,
    R16G16B16A16_FLOAT = 0xa,
    R16G16_FLOAT = 0x22,
    R16_UNORM = 0x38,
    R24G8_TYPELESS = 0x2c,
    R32_FLOAT = 0x29,
    R8G8B8A8_UNORM = 0x1c,
    R8G8B8A8_UNORM_SRGB = 0x1d,
    R8G8_SNORM = 0x33,
    R8G8_UNORM = 0x31,
    R8_UNORM = 0x3d,
    UNKNOWN = 0x0,
};
std::uint32_t smaller(std::uint32_t a, std::uint32_t b) {
    return a < b ? a : b;
}
std::uint32_t half(std::uint32_t value) {
    return value > 1 ? value / 2 : 1;
}
std::uint32_t mip_count(std::uint32_t width, std::uint32_t height) {
    std::uint32_t count = 1;
    for (std::uint32_t size = width > height ? width : height; size > 1; size /= 2)
        ++count;
    return count;
}
std::uint32_t power_of_two(std::uint32_t size) {
    std::uint32_t result = 1;
    while (result < size)
        result *= 2;
    return result;
}
bool valid_dimensions(std::uint32_t width, std::uint32_t height) {
    return width && height && width <= maximum_dimension && height <= maximum_dimension;
}
uint16_t read16(const unsigned char* bytes) {
    return uint16_t(bytes[0]) | uint16_t(bytes[1]) << 8;
}
uint32_t read32(const unsigned char* bytes) {
    return uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 | uint32_t(bytes[2]) << 16 |
           uint32_t(bytes[3]) << 24;
}
std::uint32_t divide_channel(uint64_t numerator, std::uint32_t denominator) {
    return std::uint32_t(numerator / denominator);
}
std::uint32_t block_size(Format format) {
    switch (format) {
    case Format::BC1_UNORM:
    case Format::BC1_UNORM_SRGB:
    case Format::BC4_UNORM:
    case Format::BC4_SNORM:
        return 8;
    case Format::BC2_UNORM:
    case Format::BC2_UNORM_SRGB:
    case Format::BC3_UNORM:
    case Format::BC3_UNORM_SRGB:
    case Format::BC5_UNORM:
    case Format::BC5_SNORM:
    case Format::BC6H_UF16:
    case Format::BC6H_SF16:
    case Format::BC7_UNORM:
    case Format::BC7_UNORM_SRGB:
        return 16;
    default:
        return 0;
    }
}
std::uint32_t pixel_size(Format format) {
    switch (format) {
    case Format::R8_UNORM:
    case Format::A8_UNORM:
        return 1;
    case Format::R8G8_UNORM:
    case Format::R8G8_SNORM:
    case Format::R16_UNORM:
    case Format::B5G6R5_UNORM:
    case Format::B5G5R5A1_UNORM:
    case Format::B4G4R4A4_UNORM:
        return 2;
    case Format::R8G8B8A8_UNORM:
    case Format::R8G8B8A8_UNORM_SRGB:
    case Format::B8G8R8A8_UNORM:
    case Format::B8G8R8A8_UNORM_SRGB:
    case Format::B8G8R8X8_UNORM:
    case Format::B8G8R8X8_UNORM_SRGB:
    case Format::R10G10B10A2_UNORM:
    case Format::R16G16_FLOAT:
    case Format::R32_FLOAT:
    case Format::R24G8_TYPELESS:
    case Format::D24_UNORM_S8_UINT:
        return 4;
    case Format::R16G16B16A16_FLOAT:
        return 8;
    default:
        return 0;
    }
}
struct CpuImage {
    std::uint32_t width = 0, height = 0;
    unsigned char* pixels = nullptr;
    ~CpuImage() {
        free_bytes(pixels);
    }
    DecodeStatus initialize(std::uint32_t w, std::uint32_t h) {
        if (!valid_dimensions(w, h))
            return invalid;
        free_bytes(pixels);
        pixels = static_cast<unsigned char*>(allocate(size_t(w) * h * 4));
        width = w;
        height = h;
        return pixels ? ok : no_memory;
    }
};
struct Cursor {
    const unsigned char* bytes;
    size_t size, offset;
    const unsigned char* take(size_t length) {
        if (offset > size || length > size - offset)
            return nullptr;
        const auto* result = bytes + offset;
        offset += length;
        return result;
    }
};
// Native asset decoding: zigrebuild/src/assets/{texture,dds}.zig and
// zigrebuild/src/graphics/{texture_quality,wic}.zig are the algorithm sources.
void color16(const unsigned char* sample, bool alpha, unsigned char* rgba) {
    std::uint32_t value = read16(sample);
    for (std::uint32_t channel = 0; channel < 3; ++channel) {
        std::uint32_t bits = (value >> (10 - channel * 5)) & 31;
        rgba[channel] = static_cast<unsigned char>((bits << 3) | (bits >> 2));
    }
    rgba[3] = alpha && !(value & 0x8000) ? 0 : 255;
}
void tga_color(const unsigned char* sample, std::uint32_t depth, bool alpha, unsigned char* rgba) {
    if (depth == 15 || depth == 16)
        color16(sample, alpha, rgba);
    else {
        rgba[0] = sample[2];
        rgba[1] = sample[1];
        rgba[2] = sample[0];
        rgba[3] = depth == 32 && alpha ? sample[3] : 255;
    }
}
DecodeStatus decode_tga(const unsigned char* bytes, size_t size, CpuImage& image) {
    if (size < 18)
        return invalid;
    std::uint32_t map = bytes[1], type = bytes[2], base = type & 7, depth = bytes[16];
    std::uint32_t map_first = read16(bytes + 3), map_length = read16(bytes + 5),
                  map_depth = bytes[7], descriptor = bytes[17];
    if (type > 11 || (base != 1 && base != 2 && base != 3) || map > 1 || (base == 1 && map != 1) ||
        (descriptor & 0xc0) || (base == 1 && depth != 8 && depth != 16) ||
        (base == 2 && depth != 15 && depth != 16 && depth != 24 && depth != 32) ||
        (base == 3 && depth != 8 && depth != 16) ||
        (map && map_depth != 15 && map_depth != 16 && map_depth != 24 && map_depth != 32))
        return invalid;
    Cursor cursor{bytes, size, 18};
    std::uint32_t map_stride = (map_depth + 7) / 8, stride = (depth + 7) / 8;
    if (!cursor.take(bytes[0]))
        return invalid;
    const auto* palette = map ? cursor.take(size_t(map_length) * map_stride) : nullptr;
    if (map && !palette)
        return invalid;
    DecodeStatus result = image.initialize(read16(bytes + 12), read16(bytes + 14));
    if (failed(result))
        return result;
    std::uint32_t count = image.width * image.height, index = 0;
    while (index < count) {
        std::uint32_t packet = 0;
        if (type & 8) {
            const auto* header = cursor.take(1);
            if (!header)
                return invalid;
            packet = *header;
        }
        std::uint32_t length = type & 8 ? (packet & 127) + 1 : 1;
        bool repeated = (type & 8) && (packet & 128);
        if (length > count - index)
            return invalid;
        const auto* repeat = repeated ? cursor.take(stride) : nullptr;
        if (repeated && !repeat)
            return invalid;
        for (std::uint32_t i = 0; i < length; ++i, ++index) {
            const auto* sample = repeated ? repeat : cursor.take(stride);
            if (!sample)
                return invalid;
            unsigned char rgba[4];
            if (base == 1) {
                std::uint32_t entry = depth == 8 ? sample[0] : read16(sample);
                if (entry < map_first || entry - map_first >= map_length)
                    return invalid;
                tga_color(palette + (entry - map_first) * map_stride, map_depth,
                          map_depth == 16 || map_depth == 32, rgba);
            } else if (base == 2)
                tga_color(sample, depth, (descriptor & 15) != 0, rgba);
            else {
                rgba[0] = rgba[1] = rgba[2] = sample[0];
                rgba[3] = depth == 16 ? sample[1] : 255;
            }
            std::uint32_t x = index % image.width, y = index / image.width;
            if (descriptor & 16)
                x = image.width - 1 - x;
            if (!(descriptor & 32))
                y = image.height - 1 - y;
            copy_bytes(image.pixels + (size_t(y) * image.width + x) * 4, rgba, 4);
        }
    }
    return ok;
}
DecodeStatus decode_bmp(const unsigned char* bytes, size_t size, CpuImage& image) {
    if (size < 54 || bytes[0] != 'B' || bytes[1] != 'M')
        return invalid;
    std::uint32_t offset = read32(bytes + 10), header = read32(bytes + 14),
                  depth = read16(bytes + 28);
    int width = static_cast<int>(read32(bytes + 18)),
        signed_height = static_cast<int>(read32(bytes + 22));
    if (header < 40 || read16(bytes + 26) != 1 || read32(bytes + 30) != 0 || width <= 0 ||
        width > int(maximum_dimension) || signed_height == 0 ||
        signed_height < -int(maximum_dimension) || signed_height > int(maximum_dimension) ||
        (depth != 8 && depth != 16 && depth != 24 && depth != 32))
        return invalid;
    std::uint32_t height = std::uint32_t(signed_height < 0 ? -signed_height : signed_height),
                  pitch = ((std::uint32_t(width) * depth + 31) / 32) * 4;
    if (offset > size || size_t(pitch) * height > size - offset)
        return invalid;
    DecodeStatus result = image.initialize(std::uint32_t(width), height);
    if (failed(result))
        return result;
    for (std::uint32_t y = 0; y < height; ++y) {
        const auto* row = bytes + offset + size_t(signed_height < 0 ? y : height - 1 - y) * pitch;
        for (std::uint32_t x = 0; x < std::uint32_t(width); ++x) {
            const auto* sample = row + x * (depth / 8);
            auto* rgba = image.pixels + (size_t(y) * width + x) * 4;
            if (depth == 8) {
                size_t entry = size_t(14) + header + std::uint32_t(sample[0]) * 4;
                if (entry > size || 4 > size - entry || entry + 4 > offset)
                    return invalid;
                sample = bytes + entry;
            }
            if (depth == 16)
                color16(sample, false, rgba);
            else {
                rgba[0] = sample[2];
                rgba[1] = sample[1];
                rgba[2] = sample[0];
                rgba[3] = 255;
            }
        }
    }
    return ok;
}
struct Dds {
    std::uint32_t width = 0, height = 0, levels = 0, bits = 0, masks[4] = {};
    Format format = Format::UNKNOWN;
    bool premultiplied = false, cpu_supported = true;
    TextureMip mips[maximum_levels] = {};
};
DecodeStatus parse_dds(const unsigned char* bytes, size_t size, Dds& dds) {
    if (size < 128 || read32(bytes) != 0x20534444 || read32(bytes + 4) != 124 ||
        read32(bytes + 76) != 32)
        return invalid;
    dds.width = read32(bytes + 16);
    dds.height = read32(bytes + 12);
    dds.levels = read32(bytes + 28);
    if (!dds.levels)
        dds.levels = 1;
    if (!valid_dimensions(dds.width, dds.height) || dds.levels > mip_count(dds.width, dds.height) ||
        (read32(bytes + 112) & 0x20fe00) || read32(bytes + 24) > 1)
        return invalid;
    std::uint32_t flags = read32(bytes + 80), fourcc = read32(bytes + 84);
    size_t offset = 128;
    dds.bits = read32(bytes + 88);
    for (std::uint32_t c = 0; c < 4; ++c)
        dds.masks[c] = read32(bytes + 92 + c * 4);
    if (flags & 4) {
        switch (fourcc) {
        case 0x31545844:
            dds.format = Format::BC1_UNORM;
            break;
        case 0x32545844:
        case 0x33545844:
            dds.format = Format::BC2_UNORM;
            dds.premultiplied = fourcc == 0x32545844;
            break;
        case 0x34545844:
        case 0x35545844:
            dds.format = Format::BC3_UNORM;
            dds.premultiplied = fourcc == 0x34545844;
            break;
        case 0x31495441:
        case 0x55344342:
            dds.format = Format::BC4_UNORM;
            break;
        case 0x32495441:
        case 0x55354342:
            dds.format = Format::BC5_UNORM;
            break;
        case 0x30315844:
            if (size < 148 || read32(bytes + 132) != 3 || read32(bytes + 140) != 1 ||
                (read32(bytes + 136) & 4))
                return invalid;
            dds.format = static_cast<Format>(read32(bytes + 128));
            offset = 148;
            if (!block_size(dds.format) && !pixel_size(dds.format))
                return unsupported;
            dds.bits = pixel_size(dds.format) * 8;
            for (auto& mask : dds.masks)
                mask = 0;
            if (block_size(dds.format))
                break;
            switch (dds.format) {
            case Format::R8G8B8A8_UNORM:
            case Format::R8G8B8A8_UNORM_SRGB:
                dds.masks[0] = 0xff;
                dds.masks[1] = 0xff00;
                dds.masks[2] = 0xff0000;
                dds.masks[3] = 0xff000000;
                break;
            case Format::B8G8R8A8_UNORM:
            case Format::B8G8R8A8_UNORM_SRGB:
            case Format::B8G8R8X8_UNORM:
            case Format::B8G8R8X8_UNORM_SRGB:
                dds.masks[0] = 0xff0000;
                dds.masks[1] = 0xff00;
                dds.masks[2] = 0xff;
                if (dds.format == Format::B8G8R8A8_UNORM ||
                    dds.format == Format::B8G8R8A8_UNORM_SRGB)
                    dds.masks[3] = 0xff000000;
                break;
            case Format::R8_UNORM:
            case Format::R16_UNORM:
                dds.masks[0] = dds.bits == 8 ? 0xff : 0xffff;
                break;
            case Format::A8_UNORM:
                dds.masks[3] = 0xff;
                break;
            default:
                dds.cpu_supported = false;
                break;
            }
            break;
        default:
            return unsupported;
        }
    } else {
        if (!(flags & (0x40 | 0x20000 | 2)) ||
            (dds.bits != 8 && dds.bits != 16 && dds.bits != 24 && dds.bits != 32))
            return unsupported;
        if (dds.bits == 32 && dds.masks[0] == 0xff && dds.masks[1] == 0xff00 &&
            dds.masks[2] == 0xff0000 && dds.masks[3] == 0xff000000)
            dds.format = Format::R8G8B8A8_UNORM;
        if (dds.bits == 32 && dds.masks[0] == 0xff0000 && dds.masks[1] == 0xff00 &&
            dds.masks[2] == 0xff)
            dds.format = dds.masks[3] == 0xff000000 ? Format::B8G8R8A8_UNORM
                         : dds.masks[3] == 0        ? Format::B8G8R8X8_UNORM
                                                    : Format::UNKNOWN;
    }
    std::uint32_t width = dds.width, height = dds.height, block = block_size(dds.format);
    for (std::uint32_t level = 0; level < dds.levels; ++level) {
        std::uint32_t pitch = block ? ((width + 3) / 4) * block : (width * dds.bits + 7) / 8,
                      rows = block ? (height + 3) / 4 : height;
        size_t length = size_t(pitch) * rows;
        if (!pitch || offset > size || length > size - offset || length > 0xffffffffu)
            return invalid;
        dds.mips[level] = {bytes + offset, pitch, std::uint32_t(length)};
        offset += length;
        width = half(width);
        height = half(height);
    }
    return ok;
}
void rgb565(std::uint32_t value, unsigned char* rgba) {
    std::uint32_t r = (value >> 11) & 31, g = (value >> 5) & 63, b = value & 31;
    rgba[0] = static_cast<unsigned char>((r << 3) | (r >> 2));
    rgba[1] = static_cast<unsigned char>((g << 2) | (g >> 4));
    rgba[2] = static_cast<unsigned char>((b << 3) | (b >> 2));
    rgba[3] = 255;
}
void decode_block(const unsigned char* bytes, Format format, unsigned char rgba[16][4]) {
    bool bc1 = format == Format::BC1_UNORM || format == Format::BC1_UNORM_SRGB;
    bool bc2 = format == Format::BC2_UNORM || format == Format::BC2_UNORM_SRGB;
    const auto* colors = bytes + (bc1 ? 0 : 8);
    std::uint32_t first = read16(colors), second = read16(colors + 2);
    unsigned char palette[4][4] = {};
    rgb565(first, palette[0]);
    rgb565(second, palette[1]);
    bool four = !bc1 || first > second;
    for (std::uint32_t c = 0; c < 3; ++c) {
        std::uint32_t a = palette[0][c], b = palette[1][c];
        palette[2][c] = static_cast<unsigned char>(four ? (2 * a + b) / 3 : (a + b) / 2);
        palette[3][c] = static_cast<unsigned char>(four ? (a + 2 * b) / 3 : 0);
    }
    palette[2][3] = 255;
    palette[3][3] = four ? 255 : 0;
    std::uint32_t selectors = read32(colors + 4);
    for (std::uint32_t index = 0; index < 16; ++index) {
        copy_bytes(rgba[index], palette[selectors & 3], 4);
        selectors >>= 2;
    }
    if (bc2) {
        for (std::uint32_t i = 0; i < 16; ++i)
            rgba[i][3] = static_cast<unsigned char>(((bytes[i / 2] >> ((i & 1) * 4)) & 15) * 17);
    } else if (!bc1) {
        unsigned char alpha[8] = {bytes[0], bytes[1], 0, 0, 0, 0, 0, 255};
        if (alpha[0] > alpha[1]) {
            for (std::uint32_t i = 2; i < 8; ++i)
                alpha[i] =
                    static_cast<unsigned char>(((8 - i) * alpha[0] + (i - 1) * alpha[1]) / 7);
        } else
            for (std::uint32_t i = 2; i < 6; ++i)
                alpha[i] =
                    static_cast<unsigned char>(((6 - i) * alpha[0] + (i - 1) * alpha[1]) / 5);
        for (std::uint32_t i = 0; i < 16; ++i) {
            std::uint32_t bit = i * 3, byte = 2 + bit / 8, shift = bit & 7,
                          selector = bytes[byte] >> shift;
            if (shift > 5)
                selector |= std::uint32_t(bytes[byte + 1]) << (8 - shift);
            rgba[i][3] = alpha[selector & 7];
        }
    }
}
DecodeStatus decode_dds_level(const Dds& dds, std::uint32_t level, CpuImage& image) {
    if (!dds.cpu_supported)
        return unsupported;
    std::uint32_t width = dds.width, height = dds.height;
    for (std::uint32_t i = 0; i < level; ++i) {
        width = half(width);
        height = half(height);
    }
    std::uint32_t block = block_size(dds.format);
    if (block && dds.format != Format::BC1_UNORM && dds.format != Format::BC1_UNORM_SRGB &&
        dds.format != Format::BC2_UNORM && dds.format != Format::BC2_UNORM_SRGB &&
        dds.format != Format::BC3_UNORM && dds.format != Format::BC3_UNORM_SRGB)
        return unsupported;
    DecodeStatus result = image.initialize(width, height);
    if (failed(result))
        return result;
    const auto* source = static_cast<const unsigned char*>(dds.mips[level].bytes);
    if (block) {
        for (std::uint32_t y = 0; y < (height + 3) / 4; ++y)
            for (std::uint32_t x = 0; x < (width + 3) / 4; ++x) {
                unsigned char rgba[16][4];
                decode_block(source + y * dds.mips[level].row_pitch + x * block, dds.format, rgba);
                for (std::uint32_t dy = 0; dy < 4 && y * 4 + dy < height; ++dy)
                    for (std::uint32_t dx = 0; dx < 4 && x * 4 + dx < width; ++dx) {
                        auto* color = rgba[dy * 4 + dx];
                        if (dds.premultiplied && color[3])
                            for (std::uint32_t c = 0; c < 3; ++c)
                                color[c] = static_cast<unsigned char>(
                                    smaller(255, std::uint32_t(color[c]) * 255 / color[3]));
                        copy_bytes(image.pixels + (size_t(y * 4 + dy) * width + x * 4 + dx) * 4,
                                   color, 4);
                    }
            }
        return ok;
    }
    for (std::uint32_t y = 0; y < height; ++y)
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto* sample = source + y * dds.mips[level].row_pitch + x * (dds.bits / 8);
            auto* color = image.pixels + (size_t(y) * width + x) * 4;
            std::uint32_t value = 0;
            for (std::uint32_t byte = 0; byte < dds.bits / 8; ++byte)
                value |= std::uint32_t(sample[byte]) << (byte * 8);
            color[0] = color[1] = color[2] = 0;
            color[3] = 255;
            for (std::uint32_t channel = 0; channel < 4; ++channel) {
                std::uint32_t mask = dds.masks[channel];
                if (!mask)
                    continue;
                std::uint32_t shift = 0;
                while (!(mask & 1)) {
                    mask >>= 1;
                    ++shift;
                }
                color[channel] = static_cast<unsigned char>(
                    divide_channel(uint64_t((value >> shift) & mask) * 255 + mask / 2, mask));
            }
            if (!dds.masks[1] && !dds.masks[2])
                color[1] = color[2] = color[0];
        }
    return ok;
}

} // namespace
Image decode_image(const std::vector<std::uint8_t>& bytes) {
    if (bytes.empty())
        throw std::runtime_error("Empty image");
    CpuImage decoded;
    DecodeStatus status = invalid;
    if (bytes.size() >= 4 && read32(bytes.data()) == 0x20534444) {
        Dds dds;
        status = parse_dds(bytes.data(), bytes.size(), dds);
        if (!failed(status))
            status = decode_dds_level(dds, 0, decoded);
    } else if (bytes.size() >= 2 && bytes[0] == 'B' && bytes[1] == 'M') {
        status = decode_bmp(bytes.data(), bytes.size(), decoded);
    } else
        status = decode_tga(bytes.data(), bytes.size(), decoded);
    if (failed(status))
        throw std::runtime_error("Invalid or unsupported DDS/TGA/BMP image (" +
                                 std::to_string(status) + ")");
    Image result;
    result.width = decoded.width;
    result.height = decoded.height;
    result.pixels.assign(decoded.pixels,
                         decoded.pixels + size_t(decoded.width) * decoded.height * 4);
    return result;
}
} // namespace genesis
