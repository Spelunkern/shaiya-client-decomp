#include "water.h"
#include <cstdio>
#include <stdexcept>

using namespace genesis;
namespace {
void expect(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        auto normals = water_procedural_normals();
        expect(normals.size() == 131072, "Procedural normal size");
        // x=y=128: angle 0 -> cos 64, sin 0.
        expect(normals[(128 * 256 + 128) * 2] == 64 && normals[(128 * 256 + 128) * 2 + 1] == 0,
               "Procedural normal at the phase origin");
        expect(normals[0] == std::int8_t(int(std::cos(-12.566371f) * 64)),
               "Procedural normal wraps through int truncation");

        Image flat;
        flat.width = flat.height = 4;
        flat.pixels.assign(64, 100);
        auto flat_normals = water_height_normals(flat);
        for (int i = 0; i < 64; i += 4)
            expect(flat_normals.pixels[i] == 128 && flat_normals.pixels[i + 1] == 128 &&
                       flat_normals.pixels[i + 2] == 255 && flat_normals.pixels[i + 3] == 255,
                   "A flat height map yields straight-up normals");
        Image ramp;
        ramp.width = 4;
        ramp.height = 1;
        ramp.pixels = {0, 0, 0, 255, 100, 0, 0, 255, 200, 0, 0, 255, 255, 0, 0, 255};
        auto ramp_normals = water_height_normals(ramp);
        expect(ramp_normals.pixels[4] < 128, "Rising heights tilt the normal toward -x");
        expect(ramp_normals.pixels[5] == 128, "No y gradient on a single row");

        Matrix vp = identity;
        auto bias = water_projective_bias(vp);
        expect(bias.m[0][0] == .5f && bias.m[1][1] == -.5f && bias.m[3][0] == .5f &&
                   bias.m[3][1] == .5f,
               "Projective bias");
        auto quad = water56_quad();
        expect(quad[3].uv[0] == 2000 && quad[0].position[0] == -2048, "Water56 quad");

        WaterAnimation flow(0);
        flow.advance(1);
        auto scrolled = flow.scrolled_quad();
        expect(std::abs(scrolled[1].uv[1] - .35f) < 1e-5f &&
                   std::abs(scrolled[0].uv[1] - 400.35f) < 1e-3f && scrolled[2].uv[0] == 400.5f,
               "Mode 0 scrolls V by time * .7 * .5");
        WaterAnimation ripple(1);
        ripple.advance(2);
        expect(ripple.phase() == 3 && ripple.second_offset()[0] == -3, "Mode 1 phase");

        Renderer r(32, 32, true);
        auto snorm = water_procedural_normals();
        GLuint texture = r.texture_rg_snorm(256, 256, snorm.data());
        r.release_texture(texture);
        WaterCapture capture(r, 1, 16, 16);
        expect(!capture.valid(WaterCapture::Kind::reflection), "Capture starts invalid");
        {
            auto scope = capture.begin(WaterCapture::Kind::reflection);
            expect(r.width() == 16, "Capture redirects to the reflection target");
            SceneUniforms scene;
            scene.view_projection.m[0][0] = 2;
            r.scene(scene);
            capture.capture(WaterCapture::Kind::reflection);
        }
        expect(r.width() == 32 && r.current_target() == 0, "Capture scope restores the target");
        expect(capture.valid(WaterCapture::Kind::reflection) &&
                   capture.projection(WaterCapture::Kind::reflection).m[0][0] == 1,
               "Captured projection is biased view-projection");
        auto pixels = r.read_pixels();
        (void)pixels;
        try {
            WaterCapture bad(r, 0, 8, 8);
            (void)bad.begin(WaterCapture::Kind::refraction);
            throw std::logic_error("Accepted refraction in mode 0");
        } catch (const std::invalid_argument&) {
        }
        {
            Renderer w(32, 32, true);
            const unsigned char white[] = {255, 255, 255, 255};
            GLuint reflection = w.texture_rgba(1, 1, white);
            auto signed_normals = water_procedural_normals();
            GLuint normal = w.texture_rg_snorm(256, 256, signed_normals.data());
            SceneUniforms scene;
            scene.view_projection = {{{1.f / 2048, 0, 0, 0}, {0, 0, 0, 0},
                                      {0, 1.f / 2048, 0, 0}, {0, 0, .5f, 1}}};
            w.clear({0, 0, 0, 1});
            w.scene(scene);
            WaterSurface surface;
            surface.normal = normal;
            surface.reflection = reflection;
            draw_water(w, surface);
            auto frame = w.read_pixels();
            const auto* center = &frame[(16 * 32 + 16) * 4];
            expect(center[0] || center[1] || center[2], "Mode 0 water draws over the clear color");
            surface.mode = 1;
            surface.refraction = reflection;
            surface.normal = reflection;
            w.clear({0, 0, 0, 1});
            draw_water(w, surface);
            frame = w.read_pixels();
            expect(frame[(16 * 32 + 16) * 4 + 2] > 0, "Mode 1 water draws");

            w.clear({1, 0, 0, 1});
            draw_underwater_attenuation(w, 0);
            frame = w.read_pixels();
            expect(std::abs(int(frame[(16 * 32 + 16) * 4]) - 77) <= 2,
                   "Underwater attenuation uses the constant blend factor");
            w.clear({1, 0, 0, 1});
            draw_underwater_attenuation(w, 1);
            expect(w.read_pixels()[(16 * 32 + 16) * 4] == 255, "No attenuation at depth factor 1");
        }
        std::puts("Water orchestration checks passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
