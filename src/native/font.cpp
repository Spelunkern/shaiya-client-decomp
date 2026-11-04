#include "font.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>

namespace genesis {
std::vector<char32_t> decode_utf8(std::string_view text) {
    std::vector<char32_t> result;
    result.reserve(text.size());
    std::size_t at = 0;
    while (at < text.size()) {
        auto first = static_cast<unsigned char>(text[at++]);
        if (first < 128) {
            result.push_back(first);
            continue;
        }
        unsigned count = 0;
        char32_t point = 0, minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) {
            count = 1;
            point = first & 31;
            minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            count = 2;
            point = first & 15;
            minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            count = 3;
            point = first & 7;
            minimum = 0x10000;
        } else {
            result.push_back(0xfffd);
            continue;
        }
        auto next = at;
        bool valid = count <= text.size() - at;
        for (unsigned i = 0; i < count && valid; ++i) {
            auto byte = static_cast<unsigned char>(text[next++]);
            if ((byte & 0xc0) != 0x80)
                valid = false;
            else
                point = (point << 6) | (byte & 63);
        }
        if (!valid) {
            result.push_back(0xfffd);
            continue;
        }
        at = next;
        if (point < minimum || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff))
            point = 0xfffd;
        result.push_back(point);
    }
    return result;
}
struct Font::Impl {
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    std::vector<unsigned char> bytes;
    std::map<char32_t, Glyph> glyphs;
    ~Impl() {
        if (face)
            FT_Done_Face(face);
        if (library)
            FT_Done_FreeType(library);
    }
};
Font::Font(const std::filesystem::path& path, unsigned pixel_height)
    : impl_(std::make_unique<Impl>()) {
    if (!pixel_height || pixel_height > 256)
        throw std::invalid_argument("Invalid font pixel height");
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    auto size = stream.tellg();
    if (!stream || size <= 0 || size > 32 * 1024 * 1024)
        throw std::runtime_error("Cannot read font file");
    impl_->bytes.resize(static_cast<std::size_t>(size));
    stream.seekg(0);
    if (!stream.read(reinterpret_cast<char*>(impl_->bytes.data()), size))
        throw std::runtime_error("Truncated font file");
    if (FT_Init_FreeType(&impl_->library) ||
        FT_New_Memory_Face(impl_->library, impl_->bytes.data(),
                           static_cast<FT_Long>(impl_->bytes.size()), 0, &impl_->face))
        throw std::runtime_error("Cannot initialize font face");
    if (FT_Select_Charmap(impl_->face, FT_ENCODING_UNICODE) ||
        FT_Set_Pixel_Sizes(impl_->face, 0, pixel_height))
        throw std::runtime_error("Cannot select Unicode font size");
}
Font::~Font() = default;
const Glyph& Font::glyph(char32_t point) {
    auto found = impl_->glyphs.find(point);
    if (found != impl_->glyphs.end())
        return found->second;
    auto face = impl_->face;
    unsigned index = FT_Get_Char_Index(face, point);
    if (FT_Load_Glyph(face, index, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL))
        throw std::runtime_error("Cannot render font glyph");
    auto slot = face->glyph;
    const auto& bitmap = slot->bitmap;
    Glyph glyph;
    glyph.index = index;
    glyph.width = static_cast<int>(bitmap.width);
    glyph.height = static_cast<int>(bitmap.rows);
    glyph.left = slot->bitmap_left;
    glyph.top = slot->bitmap_top;
    glyph.advance = float(slot->advance.x) / 64;
    if (bitmap.width > 1024 || bitmap.rows > 1024)
        throw std::runtime_error("Font glyph exceeds atlas dimensions");
    glyph.coverage.resize(std::size_t(bitmap.width) * bitmap.rows);
    for (unsigned y = 0; y < bitmap.rows; ++y) {
        const auto* row = bitmap.pitch >= 0 ? bitmap.buffer + std::size_t(y) * bitmap.pitch
                                            : bitmap.buffer + std::size_t(bitmap.rows - 1 - y) *
                                                                  std::size_t(-bitmap.pitch);
        for (unsigned x = 0; x < bitmap.width; ++x) {
            unsigned coverage = 0;
            if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
                if (bitmap.num_grays < 2)
                    throw std::runtime_error("Invalid glyph gray levels");
                coverage = unsigned(row[x]) * 255 / (bitmap.num_grays - 1);
            } else if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
                coverage = (row[x / 8] & (0x80 >> (x % 8))) ? 255 : 0;
            else
                throw std::runtime_error("Unsupported glyph bitmap mode");
            glyph.coverage[std::size_t(y) * bitmap.width + x] = static_cast<std::uint8_t>(coverage);
        }
    }
    return impl_->glyphs.emplace(point, std::move(glyph)).first->second;
}
float Font::kerning(unsigned first, unsigned second) const {
    if (!first || !second || !FT_HAS_KERNING(impl_->face))
        return 0;
    FT_Vector delta = {};
    if (FT_Get_Kerning(impl_->face, first, second, FT_KERNING_DEFAULT, &delta))
        return 0;
    return float(delta.x) / 64;
}
float Font::ascender() const {
    return float(impl_->face->size->metrics.ascender) / 64;
}
float Font::line_height() const {
    return float(impl_->face->size->metrics.height) / 64;
}
TextSize Font::measure(std::string_view text) {
    float line = 0, max = 0;
    unsigned previous = 0, lines = 1;
    for (auto point : decode_utf8(text)) {
        if (point == '\n') {
            max = std::max(max, line);
            line = 0;
            previous = 0;
            ++lines;
            continue;
        }
        const auto& next = glyph(point);
        line += kerning(previous, next.index) + next.advance;
        previous = next.index;
    }
    return {std::max(max, line), lines * line_height()};
}
std::filesystem::path default_font_path() {
    std::vector<std::filesystem::path> paths;
    if (const auto* configured = std::getenv("GENESIS_FONT"))
        paths.emplace_back(configured);
#ifdef _WIN32
    if (const auto* windows = std::getenv("WINDIR")) {
        auto fonts = std::filesystem::path(windows) / "Fonts";
        for (const auto* name : {"arial.ttf", "segoeui.ttf", "tahoma.ttf"})
            paths.push_back(fonts / name);
    }
#else
    paths.emplace_back("/usr/share/fonts/TTF/Arial.ttf");
    paths.emplace_back("/usr/share/fonts/liberation/LiberationSans-Regular.ttf");
    paths.emplace_back("/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf");
    paths.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    paths.emplace_back("/usr/share/fonts/TTF/DejaVuSans.ttf");
    paths.emplace_back("/usr/share/fonts/Adwaita/AdwaitaSans-Regular.ttf");
    paths.emplace_back("/usr/share/fonts/noto/NotoSans-Regular.ttf");
    paths.emplace_back("/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf");
#endif
    for (const auto& path : paths)
        if (std::filesystem::is_regular_file(path))
            return path;
    throw std::runtime_error("Cannot find a font; set GENESIS_FONT to a TrueType/OpenType file");
}
} // namespace genesis
