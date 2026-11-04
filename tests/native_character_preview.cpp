#include "character_preview.h"
#include "math.h"
#include "sdata.h"
#include "appearance.h"
#include <cstdio>
#include <fstream>
using namespace genesis;
int main(int argc, char** argv) {
    if (argc < 2)
        return 2;
    try {
        const std::filesystem::path root(argv[1]);
        auto archive = shaiya::portable::Archive::open(root / "data.sah", root / "data.saf");
        Renderer renderer(640, 480, true);
        SceneUniforms scene;
        scene.view = look_at_lh({0, .3f, -2.6f}, {0, 0, 0}, {0, 1, 0});
        scene.view_projection = multiply(scene.view, perspective_lh(1, 640.f / 480, .01f, 100));
        renderer.scene(scene);
        auto items = decode_items(decode_sdata(archive.read("data/Item/Item.SData")));
        CharacterEquipment equipment;
        // The preview consumer does not re-check item eligibility by class/slot.
        // Find a real nonzero Model and feed it to each clothing slot, exactly
        // as the recovered selector consumes the supplied type/id arrays.
        bool found = false;
        for (unsigned type = 1; type <= items.groups.size() && !found; ++type)
            for (unsigned id = 1; id <= items.groups[type - 1].size(); ++id)
                if (items.find(std::uint8_t(type), std::uint8_t(id))->model() == 1) {
                    equipment.types.fill(std::uint8_t(type));
                    equipment.ids.fill(std::uint8_t(id));
                    found = true;
                    break;
                }
        if (!found)
            throw std::runtime_error("No actual item with Model one");
        unsigned count = 0;
        for (unsigned race = 0; race < 4; ++race)
            for (unsigned gender = 0; gender < 2; ++gender)
                for (unsigned role = 0; role < 2; ++role) {
                    auto job = role ? 4 : (race == 1 || race == 2 ? 2 : 0);
                    CharacterPreview preview(renderer, archive, std::uint8_t(race),
                                             std::uint8_t(job), std::uint8_t(gender), 0, 0);
                    renderer.clear({0, 0, 0, 1});
                    preview.draw();
                    auto first = renderer.read_pixels();
                    std::size_t visible = 0;
                    for (std::size_t i = 0; i < first.size(); i += 4)
                        if (first[i] > 40 || first[i + 1] > 40 || first[i + 2] > 40)
                            ++visible;
                    if (visible < 2000)
                        throw std::runtime_error(
                            "Character profile preview is missing visible surfaces: " +
                            preview.profile());
                    auto equipped = character_appearance(items, 0, 0, equipment);
                    CharacterPreview clothed(renderer, archive, std::uint8_t(race),
                                             std::uint8_t(job), std::uint8_t(gender), equipped);
                    renderer.clear({0, 0, 0, 1});
                    clothed.draw();
                    auto clothed_pixels = renderer.read_pixels();
                    if (clothed_pixels == first)
                        throw std::runtime_error("Equipped appearances did not alter preview");
                    std::size_t equipped_visible = 0;
                    for (std::size_t i = 0; i < clothed_pixels.size(); i += 4)
                        if (clothed_pixels[i] > 40 || clothed_pixels[i + 1] > 40 ||
                            clothed_pixels[i + 2] > 40)
                            ++equipped_visible;
                    if (equipped_visible < 2000)
                        throw std::runtime_error("Equipped character has missing surfaces");
                    CharacterPreview caped(renderer, archive, std::uint8_t(race), std::uint8_t(job),
                                           std::uint8_t(gender), equipped,
                                           character_cloak(items, equipment));
                    auto rear = scene;
                    rear.view = look_at_lh({0, .3f, 2.6f}, {0, 0, 0}, {0, 1, 0});
                    rear.view_projection =
                        multiply(rear.view, perspective_lh(1, 640.f / 480, .01f, 100));
                    renderer.scene(rear);
                    renderer.clear({0, 0, 0, 1});
                    clothed.draw();
                    auto without_cape = renderer.read_pixels();
                    renderer.clear({0, 0, 0, 1});
                    caped.draw();
                    auto with_cape = renderer.read_pixels();
                    if (argc > 2 && count == 0) {
                        const std::filesystem::path capture(argv[2]);
                        if (std::filesystem::exists(capture))
                            throw std::runtime_error("Preview capture already exists");
                        std::ofstream output(capture, std::ios::binary);
                        output << "P6\n640 480\n255\n";
                        for (std::size_t i = 0; i < with_cape.size(); i += 4)
                            output.write(reinterpret_cast<const char*>(with_cape.data() + i), 3);
                        if (!output)
                            throw std::runtime_error("Preview capture failed");
                    }
                    if (with_cape == without_cape)
                        throw std::runtime_error("Equipped cape missing from character preview: " +
                                                 caped.profile());
                    caped.advance(.4f);
                    renderer.clear({0, 0, 0, 1});
                    caped.draw();
                    if (with_cape == renderer.read_pixels())
                        throw std::runtime_error("Cape character preview animation is static");
                    renderer.scene(scene);
                    for (int frame = 1; frame <= preview.end_frame(); ++frame) {
                        preview.advance(1.f / 30);
                        if (preview.frame() != float(frame))
                            throw std::runtime_error("Normal-action preview frame rate");
                    }
                    preview.advance(1.f / 30);
                    if (preview.frame() != 0)
                        throw std::runtime_error(
                            "Normal action failed to reset strictly after clip end");
                    preview.advance(1000.25f);
                    if (preview.frame() != 0)
                        throw std::runtime_error(
                            "Preview retained overshoot instead of native mode-one reset");
                    ++count;
                }
        CharacterPreview animated(renderer, archive, 0, 0, 0, 0, 0);
        renderer.clear({0, 0, 0, 1});
        animated.draw();
        auto initial = renderer.read_pixels();
        animated.advance(1);
        renderer.clear({0, 0, 0, 1});
        animated.draw();
        if (initial == renderer.read_pixels())
            throw std::runtime_error("Integrated normal-action preview is static");
        std::printf(
            "Native %u base/equipped/caped character profile previews and original normal-action clock passed\n",
            count);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
