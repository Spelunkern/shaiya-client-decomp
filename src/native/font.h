#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace genesis {
struct Glyph {
    unsigned index = 0;
    int width = 0, height = 0, left = 0, top = 0;
    float advance = 0;
    std::vector<std::uint8_t> coverage;
};
struct TextSize {
    float width = 0, height = 0;
};
// Font bytes, FreeType handles and glyphs are host-owned, with no HDC/HFONT or
// recovered 32-bit layout. UTF-8 decoding is explicit and independent of locale.
class Font {
  public:
    Font(const std::filesystem::path& path, unsigned pixel_height);
    ~Font();
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    const Glyph& glyph(char32_t codepoint);
    float kerning(unsigned first, unsigned second) const;
    float ascender() const;
    float line_height() const;
    TextSize measure(std::string_view utf8);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
std::vector<char32_t> decode_utf8(std::string_view text);
std::filesystem::path default_font_path();
} // namespace genesis
