#include "shadow.h"
#include <cstdio>
#include <stdexcept>

using namespace genesis;
namespace {
void expect(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
std::array<float, 4> project(const Matrix& m, Vec3 p) {
    std::array<float, 4> out{};
    for (int c = 0; c < 4; ++c)
        out[c] = p[0] * m.m[0][c] + p[1] * m.m[1][c] + p[2] * m.m[2][c] + m.m[3][c];
    return out;
}
} // namespace
int main() {
    try {
        const Vec3 actor = {10, 2, -5};
        auto shadow = make_shadow_projection(actor, 20, 1.5f, 2.f, .1f, 200.f);
        expect(shadow.world.m[3][0] == 10 && shadow.world.m[3][2] == -5, "World translation");
        auto center = project(shadow.sampling, actor);
        expect(std::abs(center[0] / center[3] - .5f) < 1e-4f &&
                   std::abs(center[1] / center[3] - .5f) < 1e-4f,
               "Actor origin samples the middle of the shadow map");
        auto right = project(shadow.sampling, {10 + 1.f, 2, -5});
        auto left = project(shadow.sampling, {10 - 1.f, 2, -5});
        expect(std::abs((right[0] / right[3] - .5f) + (left[0] / left[3] - .5f)) < 1e-4f &&
                   right[0] / right[3] != left[0] / left[3],
               "Horizontal extent is symmetric about the actor");
        expect(center[2] / center[3] > 0 && center[2] / center[3] < 1,
               "Actor is inside the depth range");
        try {
            (void)make_shadow_projection(actor, 20, 0, 1, .1f, 200.f);
            throw std::logic_error("Accepted zero width");
        } catch (const std::invalid_argument&) {
        }

        Renderer r(32, 32, true);
        ShadowMap map(r, 16);
        r.clear({1, 0, 0, 1});
        const auto before = r.resource_counts();
        map.begin(shadow);
        expect(r.current_target() == map.target() && r.width() == 16, "Shadow pass redirects");
        auto pixels = r.read_pixels();
        expect(pixels[3] == 0, "Shadow map clears to transparent");
        map.end();
        expect(r.current_target() == 0 && r.width() == 32, "Shadow pass restores target");
        expect(r.read_pixels()[0] == 255, "Main frame untouched by shadow pass");
        expect(r.resource_counts().targets == before.targets, "No resources created per pass");
        {
            // End to end: black caster -> ActorShadow map -> projected onto ground.
            Renderer g(64, 64, true);
            ActorShadow actor(g, 64);
            const ModelVertex caster[] = {{{-.5f, 1, -.5f}, {0, 1, 0}, {0, 0}},
                                          {{-.5f, 1, .5f}, {0, 1, 0}, {0, 1}},
                                          {{.5f, 1, -.5f}, {1, 0, 0}, {1, 0}},
                                          {{.5f, 1, .5f}, {0, 1, 0}, {1, 1}}};
            const std::uint16_t quad_indices[6] = {0, 1, 2, 2, 1, 3};
            actor.render({0, 0, 0}, .8f, [&] {
                ObjectUniforms object;
                object.tint = {0, 0, 0, 1};
                g.object(object);
                g.bind_images({});
                g.draw(Layout::model, VertexPass::model, PixelPass::surface, caster, 4,
                       ShadowMap::caster_style(), quad_indices, 6);
            });
            expect(actor.valid(), "Shadow rendered");
            SceneUniforms scene;
            scene.view_projection = {{{.5f, 0, 0, 0}, {0, 0, 0, 0}, {0, .5f, 0, 0}, {0, 0, .5f, 1}}};
            g.clear({0, 0, 0, 1});
            g.scene(scene);
            const std::uint32_t white = 0xffffffff;
            const WorldVertex ground[] = {{{-2, 0, -2}, white, {0, 0}}, {{-2, 0, 2}, white, {0, 1}},
                                          {{2, 0, -2}, white, {1, 0}}, {{2, 0, 2}, white, {1, 1}}};
            ObjectUniforms base;
            g.object(base);
            Style flat;
            flat.depth = Depth::write;
            flat.cull = Cull::none;
            g.draw(Layout::world, VertexPass::world, PixelPass::surface, ground, 4, flat,
                   quad_indices, 6);
            ObjectUniforms receiver;
            receiver.reflection = actor.sampling();
            receiver.parameters = {0, 0, 90.f / 255, 1};
            g.object(receiver);
            g.bind_images({actor.texture(), 0, 0, 0});
            Style cast;
            cast.blend = Blend::alpha;
            cast.depth = Depth::read;
            cast.cull = Cull::none;
            cast.sampling = Sampling::linear_clamp;
            g.draw(Layout::world, VertexPass::world, PixelPass::projected_shadow, ground, 4, cast,
                   quad_indices, 6);
            auto frame = g.read_pixels();
            const int center = frame[(32 * 64 + 32) * 4], corner = frame[(4 * 64 + 4) * 4];
            expect(corner == 255, "Ground outside the shadow stays lit");
            expect(center < 200 && center > 100, "Ground under the caster is darkened by opacity");
        }
        std::puts("Shadow projection checks passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
