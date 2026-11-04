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
        {
            Text text(renderer, font);
            auto points = decode_utf8("ab<<  3c>>XYZtail");
            auto parts = parse_outline_segments(points);
            if (parts.part[0].count != 2 || parts.part[1].start != 10 || parts.part[1].count != 3 ||
                parts.part[2].start != 13 || parts.part[2].count != 4)
                throw std::runtime_error("Outline marker segmentation");
            parts = parse_outline_segments(decode_utf8("plain"));
            if (parts.part[0].count != 5 || parts.part[1].count || parts.part[2].count)
                throw std::runtime_error("Unmarked text is one segment");
            parts = parse_outline_segments(decode_utf8("ab<<  9c>>XY"));
            if (parts.part[1].count != 2 || parts.part[2].count != 0)
                throw std::runtime_error("Outline length is clamped to the text");
            auto one = text.measure_block("Line");
            auto two = text.measure_block("Line\r\nLonger line");
            if (two.height != one.height * 2 || two.width <= one.width)
                throw std::runtime_error("Block measurement");
            TextRect calc{10, 20, 0, 0};
            text.draw_in_rect("Line\nLine", calc, text_calcrect);
            if (calc.bottom - calc.top != int(one.height * 2) || calc.right <= calc.left)
                throw std::runtime_error("CALCRECT");
            renderer.clear({0, 0, 0, 1});
            TextRect box{0, 0, 256, 96};
            text.draw_in_rect("Mid", box, text_center | text_vcenter);
            auto shot = renderer.read_pixels();
            long left = 0, right = 0;
            for (int y = 0; y < 96; ++y)
                for (int x = 0; x < 256; ++x)
                    if (shot[(y * 256 + x) * 4]) (x < 128 ? left : right) += 1;
            if (!left || !right) throw std::runtime_error("Centered text straddles the midline");
            renderer.clear({0, 0, 0, 1});
            TextRect clip{0, 0, 8, 96};
            text.draw_in_rect("Clipped text goes here", clip, 0);
            shot = renderer.read_pixels();
            for (int y = 0; y < 96; ++y)
                for (int x = 12; x < 256; ++x)
                    if (shot[(y * 256 + x) * 4]) throw std::runtime_error("Rect clipping");
            const auto before = renderer.resource_counts();
            auto label = text.outlined_label("Hi");
            bool opaque_white = false, keyed = false;
            std::vector<std::uint8_t> row(std::size_t(label.width) * label.height * 4);
            renderer.api().BindTexture(GL_TEXTURE_2D, label.texture);
            renderer.api().GetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, row.data());
            for (std::size_t i = 0; i < row.size(); i += 4) {
                if (row[i] > 200 && row[i + 1] > 200 && row[i + 3] == 255)
                    opaque_white = true;
                if (row[i + 3] == 0) keyed = true;
            }
            if (!opaque_white || !keyed || label.width < 3)
                throw std::runtime_error("Outlined label fill/key");
            renderer.release_texture(label.texture);
            if (renderer.resource_counts().textures > before.textures)
                throw std::runtime_error("Outlined label leaked");
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
