#include "glow.h"
#include <cstdio>
#include <stdexcept>

using namespace genesis;
namespace {
void quad(Renderer& r, float x, float y, float w, float h, std::uint32_t color) {
    UiVertex vertices[] = {{x, y, .5f, 1, color, 0, 0},
                           {x + w, y, .5f, 1, color, 1, 0},
                           {x + w, y + h, .5f, 1, color, 1, 1},
                           {x, y + h, .5f, 1, color, 0, 1}};
    const std::uint16_t indices[] = {0, 1, 2, 0, 2, 3};
    r.draw(Layout::ui, VertexPass::ui, PixelPass::ui, vertices, 4,
           {Blend::opaque, Depth::disabled, Cull::none, Sampling::point_clamp}, indices, 6);
}
int red(Renderer& r, int x, int y) {
    return r.read_pixels()[(y * r.width() + x) * 4];
}
void expect(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        expect(GlowEffect::strength(.5f, 0) == .5f, "Authored strength wins");
        expect(GlowEffect::strength(0, 0) == 0 && GlowEffect::strength(0, 1) == 25.f / 255 &&
                   GlowEffect::strength(2, 2) == 50.f / 255,
               "Quality-derived strength");
        Renderer r(64, 64, true);
        GlowEffect glow(r);
        r.clear({0, 0, 0, 1});
        quad(r, 24, 24, 16, 16, 0xffff0000);
        expect(red(r, 8, 32) == 0, "Pixel starts dark");
        glow.apply(0, 0); // zero strength is a no-op
        expect(red(r, 8, 32) == 0 && red(r, 32, 32) == 255, "Zero strength leaves the frame");
        glow.apply(4, 1);
        const int halo = red(r, 20, 32);
        expect(halo > 0, "Glow spreads onto neighbouring dark pixels");
        expect(red(r, 4, 4) <= halo, "Glow falls off with distance");
        expect(red(r, 32, 32) == 255, "Additive composite saturates the source");
        std::puts("Glow orchestration checks passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
