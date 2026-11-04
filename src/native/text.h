#pragma once
#include "font.h"
#include "renderer.h"
#include <map>
#include <string>
#include <vector>

namespace genesis {
// Win32 DrawText flag values, kept so recovered call sites pass them unchanged.
enum TextFlags : unsigned {
    text_center = 0x1,
    text_right = 0x2,
    text_vcenter = 0x4,
    text_bottom = 0x8,
    text_noclip = 0x100,
    text_calcrect = 0x400
};
struct TextRect {
    int left = 0, top = 0, right = 0, bottom = 0;
};
// native_font.cpp outlined_text_texture(): optional "<< Nc>>"-style marker
// selecting N characters to outline separately. Segments are [start, count)
// in code points; absent markers yield one segment covering everything.
struct OutlineSegments {
    struct Range {
        std::size_t start = 0, count = 0;
    } part[3];
};
OutlineSegments parse_outline_segments(const std::vector<char32_t>& text);
struct OutlinedLabel {
    GLuint texture = 0;
    int width = 0, height = 0;
};
// Destroy before its renderer. Glyph pages are updated incrementally and are
// shared by all strings, so changing input text does not allocate new textures.
class Text {
  public:
    Text(Renderer& renderer, Font& font) : renderer_(renderer), font_(font) {}
    ~Text();
    Text(const Text&) = delete;
    Text& operator=(const Text&) = delete;
    void draw(std::string_view utf8, float x, float y, std::uint32_t argb = 0xffffffff,float scale=1);
    // DrawText-style layout: '\n' splits lines (a trailing '\r' is dropped),
    // lines advance by the font's line height, text_center/right align each
    // line, text_vcenter/bottom align the block, text_calcrect only resizes
    // the rectangle, and drawing is clipped to it unless text_noclip.
    // Returns the block height.
    int draw_in_rect(std::string_view utf8, TextRect& rect, unsigned flags,
                     std::uint32_t argb = 0xffffffff);
    TextSize measure_block(std::string_view utf8);
    // Black five-pass outline with a white fill over a keyed background; the
    // caller owns (and releases) the returned texture.
    OutlinedLabel outlined_label(std::string_view utf8);

  private:
    struct Page {
        GLuint texture = 0;
        int x = 1, y = 1, row_height = 0;
    };
    struct Region {
        GLuint texture = 0;
        int x = 0, y = 0;
    };
    Region region(char32_t point, const Glyph& glyph);
    Renderer& renderer_;
    Font& font_;
    std::vector<Page> pages_;
    std::map<char32_t, Region> regions_;
};
} // namespace genesis
