#include "cloth_resource.h"
#include "character_profile.h"
#include "animation_pose.h"
#include "action.h"
#include "asset_path.h"
#include "math.h"
#include <cstdio>
#include <fstream>
using namespace genesis;
void expect(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
void capture(Renderer& renderer, const std::filesystem::path& path) {
    if (std::filesystem::exists(path))
        throw std::runtime_error("Capture already exists");
    auto pixels = renderer.read_pixels();
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << renderer.width() << ' ' << renderer.height() << "\n255\n";
    for (std::size_t i = 0; i < pixels.size(); i += 4)
        out.write(reinterpret_cast<const char*>(pixels.data() + i), 3);
    if (!out)
        throw std::runtime_error("Cloth capture write failed");
}
std::size_t visible(const std::vector<std::uint8_t>& pixels) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 4)
        if (pixels[i] > 40 || pixels[i + 1] > 40 || pixels[i + 2] > 40)
            ++count;
    return count;
}
int main(int argc, char** argv) {
    if (argc < 2)
        return 2;
    try {
        std::filesystem::path root(argv[1]);
        auto archive = shaiya::portable::Archive::open(root / "data.sah", root / "data.saf");
        auto catalog = decode_cloth_catalog(archive.read("data/cloak/Character/Human/1_humf.pc"));
        const auto& setup = catalog.simulation.at(0);
        auto mesh = asset_path::join("data/cloak/Character/Human/3DC",
                                     catalog.dynamic_meshes.at(setup.dynamic_mesh).name);
        Renderer renderer(640, 480, true);
        SceneUniforms scene;
        scene.view = look_at_lh({0, 0, -2.6f}, {0, 0, 0}, {0, 1, 0});
        scene.view_projection = multiply(scene.view, perspective_lh(1, 640.f / 480, .01f, 100));
        renderer.scene(scene);
        auto texture_paths = cloth_texture_paths(0);
        auto textures = decode_cloth_textures(archive.read(texture_paths.catalog));
        ClothResource cloth(renderer, archive, setup, mesh,
                            asset_path::join(texture_paths.directory, textures.at(setup.texture)));
        auto world = cloth.fitted_world();
        renderer.clear({.02f, .03f, .04f, 1});
        cloth.draw(world);
        auto first = renderer.read_pixels();
        expect(visible(first) > 1000, "Cloth OpenGL draw has no visible surface");
        if (argc > 2)
            capture(renderer, std::string(argv[2]) + "-rest.ppm");
        auto actions = decode_actions(archive.read("data/Character/Human/humf_action.alt"));
        AnimationPose pose(decode_animation(archive.read(
            asset_path::join("data/Character/Human/ANI", actions.slots[0].animation))));
        for (unsigned frame = 0; frame < 30; ++frame) {
            auto finals = pose.sample(float(frame));
            cloth.pin(finals);
            auto environment = cloth_scene(0, finals, identity, -100, 16384);
            cloth.advance(1.f / 60, 0, {0, 0, 0}, environment);
        }
        renderer.clear({.02f, .03f, .04f, 1});
        cloth.draw(world);
        auto animated = renderer.read_pixels();
        expect(visible(animated) > 1000 && animated != first,
               "Simulated cloth failed to update GPU geometry");
        if (argc > 2)
            capture(renderer, std::string(argv[2]) + "-simulated.ppm");
        renderer.clear({.02f, .03f, .04f, 1});
        cloth.draw(world, true);
        auto fallback = renderer.read_pixels();
        expect(visible(fallback) > 1000 && fallback != animated,
               "Cloth column fallback path was not rendered");
        if (argc > 2)
            capture(renderer, std::string(argv[2]) + "-fallback.ppm");
        expect(renderer.api().GetError() == GL_NO_ERROR, "OpenGL cloth rendering error");
        std::puts(
            "Native OpenGL 4.5 cloth: textured rest, simulated committed stream and column fallback passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
