#include "character_resource.h"
#include "math.h"
#include <cstdio>
using namespace genesis;
int main(int argc, char** argv) {
    if (argc < 2)
        return 2;
    try {
        const std::filesystem::path root(argv[1]);
        auto archive = shaiya::portable::Archive::open(root / "data.sah", root / "data.saf");
        Renderer renderer(640, 480, true);
        CharacterResource character(renderer, archive,
                                    default_character_parts("data/Character/Human/humf"));
        if (character.part_count() != 6)
            throw std::runtime_error("Incomplete character assembly");
        AnimationPose idle(
            decode_animation(archive.read("data/Character/Human/ANI/humf_016_idle1.ANI")));
        SceneUniforms scene;
        scene.view = look_at_lh({0, .7f, -2.8f}, {0, 0, 0}, {0, 1, 0});
        scene.view_projection = multiply(scene.view, perspective_lh(1, 640.f / 480, .01f, 100));
        renderer.scene(scene);
        renderer.clear({0, 0, 0, 1});
        character.draw(character.fitted_world(), idle.sample(12));
        auto first = renderer.read_pixels();
        std::size_t visible = 0;
        for (std::size_t i = 0; i < first.size(); i += 4)
            if (first[i] > 80 || first[i + 1] > 80 || first[i + 2] > 80)
                ++visible;
        if (visible < 4000)
            throw std::runtime_error(
                "Character surface was missing or incorrectly treated texture reflectivity as transparency");
        renderer.clear({0, 0, 0, 1});
        character.draw(character.fitted_world(), idle.sample(40));
        if (first == renderer.read_pixels())
            throw std::runtime_error("Composite character ignored the shared ANI pose");
        auto parts = default_character_parts("data/Character/Human/humf");
        parts[1].appearance = 0xffffffffu;
        bool rejected = false;
        try {
            CharacterResource invalid(renderer, archive, parts);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        if (!rejected)
            throw std::runtime_error("Invalid character appearance accepted");
        renderer.clear({0, 0, 0, 1});
        character.draw(character.fitted_world(), idle.sample(12));
        if (first != renderer.read_pixels())
            throw std::runtime_error("Failed character load damaged live resources");
        std::puts("Native six-part ML2 character/shared ANI pose rendering passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
