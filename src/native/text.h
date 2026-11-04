#pragma once
#include "font.h"
#include "renderer.h"
#include <map>

namespace genesis {
// Destroy before its renderer. Glyph pages are updated incrementally and are
// shared by all strings, so changing input text does not allocate new textures.
class Text {
  public:
    Text(Renderer& renderer, Font& font) : renderer_(renderer), font_(font) {}
    ~Text();
    Text(const Text&) = delete;
    Text& operator=(const Text&) = delete;
    void draw(std::string_view utf8, float x, float y, std::uint32_t argb = 0xffffffff,float scale=1);

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
