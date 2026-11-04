#include "archive.h"
#include "image.h"
#include "renderer.h"
#include "model.h"
#include "dungeon.h"
#include "dungeon_resource.h"
#include "terrain_resource.h"
#include "skin_resource.h"
#include "cloth.h"
#include <set>
#include "animation.h"
#include "animation_pose.h"
#include "character_resource.h"
#include "appearance.h"
#include "action.h"
#include "asset_path.h"
#include "math.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

using namespace genesis;
namespace {
bool texture_path(const std::string& path) {
    std::string extension = asset_path::extension(path);
    for (auto& c : extension)
        if (c >= 'A' && c <= 'Z')
            c += 32;
    return extension == ".dds" || extension == ".tga" || extension == ".bmp";
}
} // namespace
int main(int argc, char** argv) {
    try {
        std::filesystem::path assets = "assets", capture;
        std::string resource, model_path, dungeon_path, terrain_path, skin_path, skin_texture,
            animation_path, character_path;
        float animation_frame = 0;
        std::uint32_t action_slot = 0;
        bool has_action = false, validate_actions = false;
        bool fixed_animation_frame = false;
        std::string blend_animation;
        float blend_phase = 0;
        bool has_blend_phase = false;
        int frames = 0;
        bool validate_animations = false, validate_appearances = false;
        bool validate = false, validate_models = false, validate_dungeons = false,
             validate_skins = false, validate_cloth = false, validate_terrain = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--assets" && i + 1 < argc)
                assets = argv[++i];
            else if (arg == "--texture" && i + 1 < argc)
                resource = argv[++i];
            else if (arg == "--model" && i + 1 < argc)
                model_path = argv[++i];
            else if (arg == "--dungeon" && i + 1 < argc)
                dungeon_path = argv[++i];
            else if (arg == "--terrain" && i + 1 < argc)
                terrain_path = argv[++i];
            else if (arg == "--skin" && i + 1 < argc)
                skin_path = argv[++i];
            else if (arg == "--character" && i + 1 < argc)
                character_path = argv[++i];
            else if (arg == "--action" && i + 1 < argc) {
                auto value = std::stoull(argv[++i]);
                if (value > 0xffffffffu)
                    throw std::invalid_argument("Action slot exceeds 32 bits");
                action_slot = std::uint32_t(value);
                has_action = true;
            } else if (arg == "--material" && i + 1 < argc)
                skin_texture = argv[++i];
            else if (arg == "--animation" && i + 1 < argc)
                animation_path = argv[++i];
            else if (arg == "--blend-animation" && i + 1 < argc)
                blend_animation = argv[++i];
            else if (arg == "--blend-phase" && i + 1 < argc) {
                blend_phase = std::stof(argv[++i]);
                has_blend_phase = true;
            } else if (arg == "--animation-frame" && i + 1 < argc) {
                animation_frame = std::stof(argv[++i]);
                fixed_animation_frame = true;
            } else if (arg == "--frames" && i + 1 < argc) {
                frames = std::stoi(argv[++i]);
                if (frames < 1)
                    throw std::invalid_argument("--frames must be positive");
            } else if (arg == "--capture" && i + 1 < argc)
                capture = argv[++i];
            else if (arg == "--validate-textures")
                validate = true;
            else if (arg == "--validate-models")
                validate_models = true;
            else if (arg == "--validate-dungeons")
                validate_dungeons = true;
            else if (arg == "--validate-terrain")
                validate_terrain = true;
            else if (arg == "--validate-cloth")
                validate_cloth = true;
            else if (arg == "--validate-skins")
                validate_skins = true;
            else if (arg == "--validate-animations")
                validate_animations = true;
            else if (arg == "--validate-appearances")
                validate_appearances = true;
            else if (arg == "--validate-actions")
                validate_actions = true;
            else if (arg == "--help") {
                std::puts(
                    "Genesis native asset viewer (migration tool, not the game client)\n"
                    "--assets <directory> --texture <data/path> --model <data/path.SMOD>\n"
                    "--dungeon <data/path.dg>\n"
                    "--terrain <data/world/N.wld> renders a centered span of its FLD heightfield\n"
                    "--skin <data/path.3DC> [--material <data/path.dds>]\n"
                    "--animation <data/path.ANI> [--animation-frame <number>]\n"
                    "--blend-animation <data/path.ANI> --blend-phase <number>\n"
                    "--character <data/Character/race/prefix> (six default appearances)\n"
                    "--action <slot> selects the character's original AL3 animation entry\n"
                    "--validate-cloth: decode PC catalogs and their dynamic/static meshes\n"
                    "--validate-actions: decode AL3 action tables\n"
                    "--validate-appearances: decode ML2 appearance tables\n"
                    "--frames <count> --capture <new.ppm>\n"
                    "--validate-textures / --validate-models / --validate-dungeons / --validate-skins / "
                    "--validate-terrain: CPU decoding\n"
                    "--validate-animations: decode original ANI hierarchy and key channels\n"
                    "Model controls: A/D rotate; W/S elevate; mouse wheel zoom; Escape quit");
                return 0;
            } else
                throw std::invalid_argument("Unknown argument: " + arg);
        }
        if (unsigned(!resource.empty()) + unsigned(!model_path.empty()) +
                unsigned(!dungeon_path.empty()) + unsigned(!terrain_path.empty()) +
                unsigned(!skin_path.empty()) + unsigned(!character_path.empty()) >
            1)
            throw std::invalid_argument("Select one texture, model, dungeon or terrain");
        if (!skin_texture.empty() && skin_path.empty())
            throw std::invalid_argument("--material requires --skin");
        if (!animation_path.empty() && skin_path.empty() && character_path.empty())
            throw std::invalid_argument("--animation requires --skin or --character");
        if (has_action && (character_path.empty() || !animation_path.empty()))
            throw std::invalid_argument("--action requires --character and replaces --animation");
        if (!blend_animation.empty() || has_blend_phase) {
            if (blend_animation.empty() || !has_blend_phase || !std::isfinite(blend_phase) ||
                (animation_path.empty() && !has_action))
                throw std::invalid_argument(
                    "Blending requires source animation/action, target animation and finite phase");
        }
        if (fixed_animation_frame &&
            ((animation_path.empty() && !has_action) || !std::isfinite(animation_frame)))
            throw std::invalid_argument(
                "--animation-frame requires an animation and a finite frame");
        auto archive = shaiya::portable::Archive::open(assets / "data.sah", assets / "data.saf");
        std::set<std::string> cloth_catalogs, cloth_dynamic, cloth_static;
        if (validate_cloth) {
            for (const auto& path : archive.paths()) {
                if (path.compare(0, 11, "data/cloak/") || asset_path::extension(path) != ".pc")
                    continue;
                cloth_catalogs.insert(path);
                auto catalog = decode_cloth_catalog(archive.read(path));
                auto parent = asset_path::parent(path);
                for (const auto& mesh : catalog.dynamic_meshes)
                    cloth_dynamic.insert(
                        asset_path::join(asset_path::join(parent, "3DC"), mesh.name));
                for (const auto& mesh : catalog.static_meshes)
                    cloth_static.insert(
                        asset_path::join(asset_path::join(parent, "3DC/Static"), mesh.name));
            }
            // Resolve every catalog reference before iterating the archive;
            // otherwise missing resources could disappear from a success count.
            for (const auto* paths : {&cloth_dynamic, &cloth_static})
                for (const auto& path : *paths)
                    if (!archive.find(path))
                        throw std::runtime_error("Missing cloth catalog resource: " + path);
        }
        if (validate || validate_models || validate_dungeons || validate_skins || validate_cloth ||
            validate_animations || validate_appearances || validate_actions || validate_terrain) {
            unsigned passed = 0, failed = 0, skipped = 0;
            for (const auto& path : archive.paths()) {
                std::string extension = asset_path::extension(path);
                for (auto& c : extension)
                    if (c >= 'A' && c <= 'Z')
                        c += 32;
                if (!((validate && texture_path(path)) ||
                      (validate_models && extension == ".smod") ||
                      (validate_dungeons && extension == ".dg") ||
                      (validate_terrain && extension == ".wld") ||
                      (validate_skins && extension == ".3dc") ||
                      (validate_animations && extension == ".ani") ||
                      (validate_appearances && extension == ".mlt") ||
                      (validate_actions && extension == ".alt") ||
                      (validate_cloth && (cloth_catalogs.count(path) || cloth_dynamic.count(path) ||
                                          cloth_static.count(path)))))
                    continue;
                if (validate_terrain && extension == ".wld") {
                    // A .wld descriptor links either an outdoor FLD field or an
                    // indoor DUN dungeon; only the former carries field terrain.
                    auto bytes = archive.read(path);
                    if (bytes.size() < 4 || bytes[0] != 'F' || bytes[1] != 'L' || bytes[2] != 'D' ||
                        bytes[3]) {
                        ++skipped;
                        continue;
                    }
                    try {
                        auto field = decode_terrain_field(bytes, default_terrain_scale().horizontal);
                        (void)decode_terrain_materials(
                            bytes, field.prefix.bytes_consumed + field.grid.bytes_consumed);
                        ++passed;
                    } catch (const std::exception& e) {
                        std::fprintf(stderr, "%s: %s\n", path.c_str(), e.what());
                        ++failed;
                    }
                    continue;
                }
                try {
                    if (validate_cloth && cloth_catalogs.count(path))
                        (void)decode_cloth_catalog(archive.read(path));
                    else if (validate_cloth && cloth_dynamic.count(path))
                        (void)decode_cloth_mesh(archive.read(path));
                    else if (extension == ".alt")
                        (void)decode_actions(archive.read(path));
                    else if (extension == ".mlt")
                        (void)decode_appearance(archive.read(path));
                    else if (extension == ".ani")
                        (void)decode_animation(archive.read(path));
                    else if (extension == ".3dc")
                        (void)decode_skin(archive.read(path));
                    else if (extension == ".dg")
                        (void)decode_dungeon(archive.read(path));
                    else if (extension == ".smod")
                        (void)decode_smod(archive.read(path));
                    else
                        (void)decode_image(archive.read(path));
                    ++passed;
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "%s: %s\n", path.c_str(), e.what());
                    ++failed;
                }
            }
            std::printf("Decoded resources: %u passed, %u failed\n", passed, failed);
            if (skipped)
                std::printf("Skipped %u dungeon-linked .wld descriptor(s) with no field terrain\n",
                            skipped);
            return failed ? 1 : 0;
        }
        if (resource.empty() && model_path.empty() && dungeon_path.empty() &&
            terrain_path.empty() && skin_path.empty() && character_path.empty())
            for (const auto& path : archive.paths())
                if (texture_path(path)) {
                    resource = path;
                    break;
                }
        if (resource.empty() && model_path.empty() && dungeon_path.empty() &&
            terrain_path.empty() && skin_path.empty() && character_path.empty())
            throw std::runtime_error("No supported texture found in archive");
        Image image;
        if (has_action) {
            auto table = decode_actions(archive.read(character_path + "_action.alt"));
            if (action_slot >= table.slots.size() || table.slots[action_slot].animation.empty())
                throw std::invalid_argument("Action slot is missing or empty");
            animation_path =
                asset_path::join(asset_path::join(asset_path::parent(character_path), "ANI"),
                                 table.slots[action_slot].animation);
            std::printf("AL3 action %u: %s (mode %u, next %u)\n", action_slot,
                        animation_path.c_str(), unsigned(table.slots[action_slot].mode()),
                        table.slots[action_slot].next);
        }
        StaticModel model;
        if (model_path.empty() && dungeon_path.empty() && terrain_path.empty() &&
            skin_path.empty() && character_path.empty())
            image = decode_image(archive.read(resource));
        else if (!model_path.empty())
            model = decode_smod(archive.read(model_path));
        Renderer renderer(1024, 768, frames > 0);
        const auto selected_path =
            !character_path.empty() ? character_path
            : !skin_path.empty()
                ? skin_path
                : (!dungeon_path.empty()
                       ? dungeon_path
                       : (!terrain_path.empty() ? terrain_path
                                                : (model_path.empty() ? resource : model_path)));
        std::string title = "Genesis — native asset viewer — " + selected_path;
        SDL_SetWindowTitle(renderer.window(), title.c_str());
        GLuint texture = 0;
        std::unique_ptr<DungeonResource> dungeon;
        std::unique_ptr<TerrainResource> terrain;
        std::unique_ptr<SkinResource> skin;
        std::unique_ptr<CharacterResource> character;
        if (!character_path.empty())
            character = std::make_unique<CharacterResource>(
                renderer, archive, default_character_parts(character_path));
        std::unique_ptr<AnimationPose> pose;
        std::unique_ptr<AnimationPose> blend_target;
        if (!blend_animation.empty())
            blend_target =
                std::make_unique<AnimationPose>(decode_animation(archive.read(blend_animation)));
        if (!animation_path.empty())
            pose = std::make_unique<AnimationPose>(decode_animation(archive.read(animation_path)));
        if (!skin_path.empty())
            skin = std::make_unique<SkinResource>(renderer, archive, skin_path, skin_texture);
        if (!dungeon_path.empty())
            dungeon = std::make_unique<DungeonResource>(renderer, archive, dungeon_path);
        if (!terrain_path.empty())
            terrain = std::make_unique<TerrainResource>(renderer, archive, terrain_path);
        if (model_path.empty() && dungeon_path.empty() && terrain_path.empty() &&
            skin_path.empty() && character_path.empty())
            texture = renderer.texture_rgba(static_cast<int>(image.width),
                                            static_cast<int>(image.height), image.pixels.data());
        std::vector<GLuint> model_textures;
        std::vector<Renderer::MeshId> model_meshes;
        std::unordered_map<std::string, GLuint> loaded;
        for (const auto& mesh : model.meshes) {
            model_meshes.push_back(
                mesh.vertices.empty() || mesh.indices.empty()
                    ? 0
                    : renderer.create_mesh(Layout::mesh, mesh.vertices.data(), mesh.vertices.size(),
                                           mesh.indices.data(), mesh.indices.size()));
            if (mesh.texture.empty()) {
                model_textures.push_back(0);
                continue;
            }
            std::string path = "data/Entity/Texture/" + mesh.texture;
            if (!archive.find(path)) {
                path = asset_path::replace_extension(path, ".dds");
            }
            if (!archive.find(path))
                throw std::runtime_error("Missing model texture: " + path);
            auto it = loaded.find(path);
            if (it == loaded.end()) {
                const auto decoded = decode_image(archive.read(path));
                it = loaded
                         .emplace(path, renderer.texture_rgba(static_cast<int>(decoded.width),
                                                              static_cast<int>(decoded.height),
                                                              decoded.pixels.data()))
                         .first;
            }
            model_textures.push_back(it->second);
        }
        renderer.bind_images({texture, 0, 0, 0});
        std::printf("%s; OpenGL %s; %zu-bit executable; %zu meshes\n", selected_path.c_str(),
                    renderer.version(), sizeof(void*) * 8,
                    character ? character->part_count()
                              : (skin ? 1
                                      : (dungeon ? dungeon->mesh_count()
                                                 : (terrain ? terrain->mesh_count()
                                                           : model.meshes.size()))));
        bool running = true;
        int rendered = 0;
        float angle = 0, elevation = .25f, distance = 3;
        double animation_seconds = 0;
        auto previous = std::chrono::steady_clock::now();
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT ||
                    (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
                    running = false;
                if (event.type == SDL_WINDOWEVENT &&
                    event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    int width = 0, height = 0;
                    SDL_GL_GetDrawableSize(renderer.window(), &width, &height);
                    renderer.resize(width, height);
                }
                if (event.type == SDL_MOUSEWHEEL)
                    distance = std::clamp(distance - event.wheel.y * .2f, .5f, 10.f);
            }
            auto now = std::chrono::steady_clock::now();
            float elapsed = std::min(.1f, std::chrono::duration<float>(now - previous).count());
            previous = now;
            animation_seconds += elapsed;
            const auto* keys = SDL_GetKeyboardState(nullptr);
            angle += (keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A]) * elapsed;
            elevation = std::clamp(
                elevation + (keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S]) * elapsed, -1.4f, 1.4f);
            renderer.clear({.025f, .025f, .04f, 1});
            if (!model_path.empty() || dungeon || terrain || skin || character) {
                Vec3 eye = {distance * std::sin(angle) * std::cos(elevation),
                            distance * std::sin(elevation),
                            -distance * std::cos(angle) * std::cos(elevation)};
                SceneUniforms scene;
                scene.view = look_at_lh(eye, {0, 0, 0}, {0, 1, 0});
                scene.view_projection = multiply(
                    scene.view,
                    perspective_lh(1.f, float(renderer.width()) / renderer.height(), .01f, 100.f));
                scene.camera = {eye[0], eye[1], eye[2], 1};
                renderer.scene(scene);
                auto draw_actor = [&](auto& actor) {
                    if (pose) {
                        auto frame = animation_frame;
                        if (!fixed_animation_frame) {
                            // Viewer preview clock only. Gameplay state timing
                            // is a separate recovered contract, not set here.
                            auto duration = double(pose->data().end) - pose->data().start;
                            frame = float(
                                pose->data().start +
                                (duration > 0 ? std::fmod(animation_seconds * 30, duration) : 0));
                        }
                        actor.draw(actor.fitted_world(),
                                   blend_target
                                       ? pose->blend_to_rest(frame, *blend_target, blend_phase)
                                       : pose->sample(frame));
                    } else
                        actor.draw_bind_pose(actor.fitted_world());
                };
                if (character)
                    draw_actor(*character);
                else if (skin)
                    draw_actor(*skin);
                else if (dungeon)
                    dungeon->draw(dungeon->fitted_world());
                else if (terrain)
                    terrain->draw(terrain->fitted_world());
                else {
                    ObjectUniforms object;
                    float radius = std::max(model.sphere[3], .001f);
                    object.world.m[0][0] = object.world.m[1][1] = object.world.m[2][2] = 1 / radius;
                    for (int i = 0; i < 3; ++i)
                        object.world.m[3][i] = -model.sphere[i] / radius;
                    renderer.object(object);
                    for (std::size_t i = 0; i < model.meshes.size(); ++i) {
                        const auto& mesh = model.meshes[i];
                        if (mesh.vertices.empty() || mesh.indices.empty())
                            continue;
                        renderer.bind_images({model_textures[i], 0, 0, 0});
                        renderer.draw_mesh(
                            model_meshes[i], VertexPass::mesh, PixelPass::surface,
                            {Blend::alpha, Depth::write, Cull::none, Sampling::anisotropic_wrap});
                    }
                }
            } else {
                const float scale = std::min(float(renderer.width()) / image.width,
                                             float(renderer.height()) / image.height);
                const float w = image.width * scale, h = image.height * scale,
                            x = (renderer.width() - w) * .5f, y = (renderer.height() - h) * .5f;
                UiVertex vertices[] = {{x, y, 0, 1, 0xffffffff, 0, 0},
                                       {x + w, y, 0, 1, 0xffffffff, 1, 0},
                                       {x + w, y + h, 0, 1, 0xffffffff, 1, 1},
                                       {x, y + h, 0, 1, 0xffffffff, 0, 1}};
                const std::uint16_t indices[] = {0, 1, 2, 0, 2, 3};
                renderer.draw(Layout::ui, VertexPass::ui, PixelPass::ui, vertices, 4,
                              {Blend::alpha, Depth::disabled, Cull::none, Sampling::linear_clamp},
                              indices, 6);
            }
            renderer.present();
            if (frames && ++rendered >= frames)
                running = false;
            if (SDL_GetWindowFlags(renderer.window()) & SDL_WINDOW_MINIMIZED)
                SDL_Delay(50);
        }
        if (!capture.empty()) {
            if (std::filesystem::exists(capture))
                throw std::runtime_error("Capture output already exists");
            auto pixels = renderer.read_pixels();
            std::ofstream out(capture, std::ios::binary);
            out << "P6\n" << renderer.width() << ' ' << renderer.height() << "\n255\n";
            for (std::size_t i = 0; i < pixels.size(); i += 4)
                out.write(reinterpret_cast<const char*>(pixels.data() + i), 3);
            out.close();
            if (!out)
                throw std::runtime_error("Cannot write capture");
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Genesis: %s\n", e.what());
        return 1;
    }
}
