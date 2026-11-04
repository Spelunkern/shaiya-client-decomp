#include "renderer.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace genesis;
namespace {
void expect_pixel(Renderer& r, int x, int y, std::array<int, 4> expected, const char* message) {
    const auto pixels = r.read_pixels();
    const auto offset = (y * r.width() + x) * 4;
    for (int i = 0; i < 4; ++i)
        if (std::abs(int(pixels[offset + i]) - expected[i]) > 2) {
            std::fprintf(stderr, "%s: (%d,%d) got [%d,%d,%d,%d], expected [%d,%d,%d,%d]\n", message,
                         x, y, pixels[offset], pixels[offset + 1], pixels[offset + 2],
                         pixels[offset + 3], expected[0], expected[1], expected[2], expected[3]);
            throw std::runtime_error(message);
        }
}
void quad(Renderer& r, float x, float y, float w, float h, std::uint32_t color, float z = .5f,
          Style style = {Blend::opaque, Depth::disabled, Cull::none, Sampling::point_clamp}) {
    UiVertex vertices[] = {{x, y, z, 1, color, 0, 0},
                           {x + w, y, z, 1, color, 1, 0},
                           {x + w, y + h, z, 1, color, 1, 1},
                           {x, y + h, z, 1, color, 0, 1}};
    const std::uint16_t indices[] = {0, 1, 2, 0, 2, 3};
    r.draw(Layout::ui, VertexPass::ui, PixelPass::ui, vertices, 4, style, indices, 6);
}
} // namespace
int main() {
    try {
        Renderer r(64, 64, true);
        std::printf("64-bit renderer: OpenGL %s\n", r.version());
        r.clear({0, 0, 0, 1});
        quad(r, 0, 0, 32, 32, 0xffff0000);
        expect_pixel(r, 8, 8, {255, 0, 0, 255}, "Top-left UI and ARGB decoding");
        expect_pixel(r, 8, 48, {0, 0, 0, 255}, "UI vertical orientation");
        Style alpha = {Blend::alpha, Depth::disabled, Cull::none, Sampling::point_clamp};
        quad(r, 0, 0, 64, 64, 0x800000ff, .5f, alpha);
        expect_pixel(r, 8, 8, {127, 0, 128, 255}, "Alpha blend");
        quad(r, 0, 0, 64, 64, 0x0000ff00);
        expect_pixel(r, 8, 8, {127, 0, 128, 255}, "Alpha cutoff discards transparent pixels");
        r.clear({0, 0, 0, 1});
        r.scissor(0, 0, 16, 16);
        quad(r, 0, 0, 64, 64, 0xff00ff00);
        expect_pixel(r, 8, 8, {0, 255, 0, 255}, "Scissor origin");
        expect_pixel(r, 32, 32, {0, 0, 0, 255}, "Scissor bounds");
        r.reset_scissor();
        Style depth = {Blend::opaque, Depth::write, Cull::none, Sampling::point_clamp};
        r.clear({0, 0, 0, 1});
        quad(r, 0, 0, 64, 64, 0xffff0000, .2f, depth);
        quad(r, 0, 0, 64, 64, 0xff0000ff, .8f, depth);
        expect_pixel(r, 8, 8, {255, 0, 0, 255}, "Zero-to-one depth and depth writes");
        // Non-symmetric uploaded texture catches row inversion and channel swizzles.
        const unsigned char texture[] = {255, 0, 0,   255, 0,   255, 0,   255,
                                         0,   0, 255, 255, 255, 255, 255, 255};
        GLuint image = r.texture_rgba(2, 2, texture);
        r.bind_images({image, 0, 0, 0});
        r.clear({0, 0, 0, 1});
        quad(r, 0, 0, 64, 64, 0xffffffff);
        expect_pixel(r, 8, 8, {255, 0, 0, 255}, "Texture top-left");
        expect_pixel(r, 48, 8, {0, 255, 0, 255}, "Texture top-right");
        expect_pixel(r, 8, 48, {0, 0, 255, 255}, "Texture bottom-left");
        // Real model and skin layouts, with translated row-major matrices.
        r.bind_images({});
        r.clear({0, 0, 0, 1});
        ModelVertex mesh[] = {{{-.8f, -.8f, .5f}, {0, 0, 1}, {0, 0}},
                              {{.8f, -.8f, .5f}, {0, 0, 1}, {1, 0}},
                              {{0, .8f, .5f}, {0, 0, 1}, {.5f, 1}}};
        ObjectUniforms object;
        object.tint = {1, 0, 0, 1};
        object.world.m[3][0] = .5f;
        r.object(object);
        r.draw(Layout::model, VertexPass::model, PixelPass::surface, mesh, 3, depth);
        expect_pixel(r, 48, 32, {255, 0, 0, 255}, "Row-major model transform");
        expect_pixel(r, 8, 32, {0, 0, 0, 255}, "Model translation bounds");
        r.clear({0, 0, 0, 1});
        object.world = identity;
        object.tint = {0, 1, 0, 1};
        r.object(object);
        SkinVertex skin[3] = {};
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                skin[i].position[j] = mesh[i].position[j];
                skin[i].normal[j] = mesh[i].normal[j];
            }
            skin[i].weight = .5f;
            skin[i].bones[0] = 0;
            skin[i].bones[1] = 1;
        }
        Matrix bones[] = {identity, identity};
        bones[0].m[3][0] = .5f;
        bones[1].m[3][0] = .5f;
        r.bones(bones, 2);
        r.draw(Layout::skin, VertexPass::skin, PixelPass::surface, skin, 3, depth);
        expect_pixel(r, 48, 32, {0, 255, 0, 255}, "Two-bone skinning and integer bone IDs");
        expect_pixel(r, 8, 32, {0, 0, 0, 255}, "Skin translation bounds");
        r.clear({0, 0, 0, 1});
        object.tint = {1, 0, 1, 1};
        r.object(object);
        // Index values above 65535 exercise the native 32-bit index path.
        std::vector<ModelVertex> large_mesh(70003);
        for (int i = 0; i < 3; ++i)
            large_mesh[70000 + i] = mesh[i];
        const std::uint32_t large_indices[] = {70000, 70001, 70002};
        const auto persistent = r.create_mesh(Layout::model, large_mesh.data(), large_mesh.size(),
                                              large_indices, 3, IndexType::u32);
        r.draw_mesh(persistent, VertexPass::model, PixelPass::surface, depth);
        expect_pixel(r, 32, 32, {255, 0, 255, 255}, "Persistent mesh with 32-bit indices");
        try {
            r.draw_mesh(persistent, VertexPass::model, PixelPass::surface, depth, 0, 3, 3);
            throw std::runtime_error("Accepted out-of-bounds base vertex");
        } catch (const std::invalid_argument&) {
        }
        try {
            r.draw_mesh(persistent, VertexPass::model, PixelPass::surface, depth, 2, 3);
            throw std::runtime_error("Accepted out-of-bounds index range");
        } catch (const std::invalid_argument&) {
        }
        r.release_mesh(persistent);
        try {r.draw_mesh(persistent,VertexPass::model,PixelPass::surface,depth);
            throw std::runtime_error("Drew a released mesh");}catch(const std::invalid_argument&) {}
        // Exercise every pixel program on the GPU, including water and postprocessing.
        r.object({});
        for (int p = 0; p < 22; ++p) {
            r.clear({0, 0, 0, 1});
            r.fullscreen(static_cast<PixelPass>(p), alpha);
        }
        const auto reflection = r.create_target(32, 16), glow = r.create_target(8, 8);
        r.scissor(0, 0, 16, 16);
        {
            TargetScope reflect(r, reflection);
            r.clear({0, 0, 1, 1});
            quad(r, 0, 0, 16, 8, 0xffff0000);
            expect_pixel(r, 4, 4, {255, 0, 0, 255}, "Offscreen top-left origin");
            {
                TargetScope filter(r, glow);
                r.clear({0, 1, 0, 1});
                expect_pixel(r, 4, 4, {0, 255, 0, 255}, "Nested render target");
            }
            if (r.current_target() != reflection || r.width() != 32 || r.height() != 16)
                throw std::runtime_error("Nested target restoration failed");
            expect_pixel(r, 24, 12, {0, 0, 255, 255}, "Offscreen content survives nested target");
        }
        if (r.current_target() != 0 || r.width() != 64 || r.height() != 64)
            throw std::runtime_error("Main target restoration failed");
        GLint scissor_enabled = 0;
        r.api().GetIntegerv(GL_SCISSOR_TEST, &scissor_enabled);
        if (!scissor_enabled)
            throw std::runtime_error("Scissor state not restored after offscreen pass");
        r.reset_scissor();
        r.clear({0, 0, 0, 1});
        r.bind_images({r.target(reflection).color, 0, 0, 0});
        r.fullscreen(PixelPass::copy,
                     {Blend::opaque, Depth::disabled, Cull::none, Sampling::point_clamp});
        expect_pixel(r, 8, 8, {255, 0, 0, 255}, "Render texture sampling orientation");
        expect_pixel(r, 48, 48, {0, 0, 255, 255}, "Render texture sampling bounds");
        r.bind_images({});
        r.clear({0, 0, 0, 1});
        quad(r, 0, 0, 64, 64, 0xffff0000); // opaque red background
        quad(r, 0, 0, 64, 64, 0xff00ff00, .5f,
             {Blend::opaque, Depth::disabled, Cull::none, Sampling::point_clamp,
              CustomBlend{BlendFactor::blend_factor, BlendFactor::inv_blend_factor},
              {.75f, .25f, .5f, 1}});
        expect_pixel(r, 8, 8, {64, 64, 0, 255}, "Custom constant-factor blend matches water's pair");
        r.clear({0, 0, 0, 1});
        quad(r, 0, 0, 64, 64, 0xffff0000);
        quad(r, 0, 0, 64, 64, 0xff00ff00, .5f,
             {Blend::opaque, Depth::disabled, Cull::none, Sampling::point_clamp,
              CustomBlend{BlendFactor::one, BlendFactor::zero}});
        expect_pixel(r, 8, 8, {0, 255, 0, 255}, "Custom one/zero blend fully overwrites");
        try {
            r.bones(bones, 257);
            throw std::runtime_error("Bone limit was not checked");
        } catch (const std::invalid_argument&) {
        }
        r.resize(48, 32);
        r.clear({1, 0, 1, 1});
        expect_pixel(r, 47, 31, {255, 0, 255, 255}, "Resize re-creates complete targets");
        r.present();
        r.check("Presentation");
        std::puts("Native rendering checks passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
