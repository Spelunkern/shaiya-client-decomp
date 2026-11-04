#include "font.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>

using namespace genesis;
void expect(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
int main() {
    try {
        expect(decode_utf8("A\xc3\xb1\xe2\x82\xac\xf0\x9f\x98\x80") ==
                   std::vector<char32_t>{U'A', U'ñ', U'€', U'😀'},
               "UTF-8 decoding");
        for (const auto& bytes : {std::string("\xc0\xaf"), std::string("\xed\xa0\x80"),
                                  std::string("\xf4\x90\x80\x80"), std::string("\xe2")}) {
            auto decoded = decode_utf8(bytes);
            expect(!decoded.empty() && decoded[0] == 0xfffd, "UTF-8 invalid sequence replacement");
        }
        Font font(default_font_path(), 20);
        const auto& a = font.glyph(U'A');
        expect(a.width > 0 && a.height > 0 && a.advance > 0 &&
                   a.coverage.size() == std::size_t(a.width) * a.height,
               "FreeType glyph dimensions");
        expect(
            std::any_of(a.coverage.begin(), a.coverage.end(), [](auto alpha) { return alpha > 0; }),
            "FreeType glyph coverage");
        expect(&font.glyph(U'A') == &a, "Glyph cache lifetime");
        expect(font.glyph(U'ñ').index != 0 && font.glyph(U'á').index != 0,
               "Western localized glyph mapping");
        auto single = font.measure("Genesis"), multi = font.measure("Genesis\nGenesis");
        expect(single.width > 0 && multi.width == single.width && multi.height == 2 * single.height,
               "Multiline metrics");
        expect(font.measure("").width == 0 && font.glyph(U' ').advance > 0,
               "Empty text and space advance");
        std::puts("Native font/Unicode/metrics checks passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
