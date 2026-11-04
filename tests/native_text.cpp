#include "text.h"
#include <cstdio>
#include <stdexcept>

using namespace genesis;
int main() {
    try {
        Renderer renderer(256, 96, true);
        Font font(default_font_path(), 20);
        const auto size = font.measure("Genesis á ñ");
        {
            Text text(renderer, font);
            renderer.clear({0, 0, 0, 1});
            text.draw("Genesis á ñ", 8, 8);
            auto pixels = renderer.read_pixels();
            unsigned visible = 0, antialiased = 0;
            for (int y = 0; y < renderer.height(); ++y)
                for (int x = 0; x < renderer.width(); ++x) {
                    auto value = pixels[(std::size_t(y) * renderer.width() + x) * 4];
                    if (value) {
                        if (x < 7 || x > 8 + size.width + 2)
                            throw std::runtime_error(
                                "Rendered text exceeds measured horizontal bounds");
                        ++visible;
                        if (value < 255)
                            ++antialiased;
                    }
                }
            if (visible < 100 || !antialiased)
                throw std::runtime_error("Glyph atlas did not render antialiased text");
            renderer.clear({0, 0, 0, 1});
            text.draw("Genesis á ñ", 8, 8);
            if (renderer.read_pixels() != pixels)
                throw std::runtime_error("Cached glyph rendering changed pixels");
        }
        const unsigned char pixel[] = {255, 0, 0, 255};
        auto texture = renderer.texture_rgba(1, 1, pixel);
        renderer.update_texture(texture, 0, 0, 1, 1, pixel);
        renderer.release_texture(texture);
        try {
            renderer.update_texture(texture, 0, 0, 1, 1, pixel);
            throw std::logic_error("Updated a released texture");
        } catch (const std::invalid_argument&) {
        }
        renderer.check("Font atlas release");
        std::puts("Native glyph atlas/pixel checks passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
