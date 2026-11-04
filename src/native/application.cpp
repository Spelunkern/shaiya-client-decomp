#include "application.h"
#include <ctime>
#include "encoding.h"
#include "image.h"
#include "character_preview.h"
#include "terrain_resource.h"
#include "dungeon_resource.h"
#include "basic_water.h"
#include "world_objects.h"
#include "world_tail.h"
#include "movement.h"
#include "sdata.h"
#include "math.h"
#include <openssl/crypto.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <fstream>
#include <cstdlib>
#include <cstdio>

namespace genesis {
std::size_t Application::world_objects_drawn() const {
    return loaded_objects_ ? loaded_objects_->drawn_last() : 0;
}
std::size_t Application::terrain_material_count() const {
    return loaded_terrain_ ? loaded_terrain_->data().materials.size() : 0;
}
std::optional<std::string> Application::terrain_surface_sound(float x, float z) const {
    if (!loaded_terrain_)
        return std::nullopt;
    const auto& data = loaded_terrain_->data();
    auto material = terrain_surface_at(data.grid, data.dimension, data.materials, x, z,
                                       default_terrain_scale().inverse_horizontal);
    return material ? std::optional<std::string>(material->sound) : std::nullopt;
}
namespace {
bool inside(float x, float y, float left, float top, float width, float height) {
    return x >= left && y >= top && x < left + width && y < top + height;
}
} // namespace
Application::Application(AppSettings settings)
    : settings_(std::move(settings)),
      renderer_(settings_.width, settings_.height, settings_.hidden),
      font_(settings_.font.empty() ? default_font_path() : settings_.font, 14),
      text_(renderer_, font_), archive_(shaiya::portable::Archive::open(
                                   settings_.assets / "data.sah", settings_.assets / "data.saf")),
      backdrop_(renderer_, archive_, "data/Entity/Building/Login_mainstructure.SMOD") {
    SDL_SetWindowTitle(renderer_.window(), "Genesis");
    SDL_SetWindowMinimumSize(renderer_.window(), 640, 480);
    login_image_ = load_image("data/interface/login.tga");
    server_image_ = load_image("data/interface/login_server.tga");
    character_image_ = load_image("data/interface/character_select.tga");
    ok_image_ = load_image("data/interface/ok_button.tga");
    if (settings_.preferences.empty()) {
#ifdef _WIN32
        if (const auto* root = std::getenv("APPDATA"))
            settings_.preferences = std::filesystem::path(root) / "Genesis" / "native-login-id.txt";
#else
        if (const auto* root = std::getenv("XDG_CONFIG_HOME"))
            settings_.preferences = std::filesystem::path(root) / "genesis" / "native-login-id.txt";
        else if (const auto* root = std::getenv("HOME"))
            settings_.preferences =
                std::filesystem::path(root) / ".config" / "genesis" / "native-login-id.txt";
#endif
    }
    if (!settings_.preferences.empty()) {
        std::ifstream in(settings_.preferences, std::ios::binary);
        char value[98] = {};
        if (in.getline(value, sizeof(value)) || in.eof()) {
            try {
                std::string text(value);
                auto bytes = utf8_to_cp1252(text);
                if (!text.empty() && bytes.size() <= 32 &&
                    text.find_first_of("\r\n\t") == std::string::npos) {
                    username_ = std::move(text);
                    remember_id_ = true;
                }
            } catch (const std::invalid_argument&) {
            }
        }
    }
    SDL_StartTextInput();
}
Application::~Application() {
    clear_password();
    SDL_StopTextInput();
}
void Application::clear_password() {
    if (!password_.empty())
        OPENSSL_cleanse(password_.data(), password_.size());
    password_.clear();
}
void Application::save_preferences() {
    if (settings_.preferences.empty())
        return;
    try {
        if (!remember_id_) {
            std::filesystem::remove(settings_.preferences);
            return;
        }
        if (username_.empty())
            return;
        if (!settings_.preferences.parent_path().empty())
            std::filesystem::create_directories(settings_.preferences.parent_path());
        std::ofstream file(settings_.preferences, std::ios::binary | std::ios::trunc);
        file << username_ << '\n';
        file.close();
        if (!file)
            throw std::runtime_error("Preference write failed");
    } catch (const std::exception&) {
        status_ = "No se pudo guardar la preferencia del usuario.";
    }
}
Application::Image Application::load_image(std::string_view path) {
    const auto decoded = decode_image(archive_.read(path));
    return {renderer_.texture_rgba(static_cast<int>(decoded.width),
                                   static_cast<int>(decoded.height), decoded.pixels.data()),
            static_cast<int>(decoded.width), static_cast<int>(decoded.height)};
}
float Application::scale() const {
    return std::min(float(renderer_.width()) / 1024, float(renderer_.height()) / 768);
}
std::array<float, 2> Application::origin() const {
    auto s = scale();
    return {(renderer_.width() - 1024 * s) * .5f, (renderer_.height() - 768 * s) * .5f};
}
std::array<float, 2> Application::logical_mouse(int x, int y) const {
    int width = 0, height = 0;
    SDL_GetWindowSize(renderer_.window(), &width, &height);
    auto offset = origin();
    auto s = scale();
    return {(x * float(renderer_.width()) / std::max(width, 1) - offset[0]) / s,
            (y * float(renderer_.height()) / std::max(height, 1) - offset[1]) / s};
}
Application::Screen Application::screen() {
    using Phase = net::Connection::Phase;
    using Login = net::Login;
    using World = net::World;
    if (connection_.phase() == Phase::world) {
        auto state = connection_.world()->state();
        return state == World::State::characters || state == World::State::selecting_character
                   ? Screen::characters
                   : Screen::loading;
    }
    if (connection_.phase() == Phase::login) {
        auto state = connection_.login()->state();
        if (state == Login::State::authenticated || state == Login::State::selecting)
            return Screen::servers;
        if (state == Login::State::await_challenge || state == Login::State::authenticating)
            return Screen::connecting;
    }
    return Screen::login;
}
bool Application::can_confirm() {
    auto page = screen();
    if (page == Screen::login)
        return !pending_auth_ && !username_.empty() && !password_.empty();
    if (page == Screen::servers)
        return connection_.login()->state() == net::Login::State::authenticated &&
               !connection_.login()->servers().empty();
    if (page == Screen::characters) {
        auto world = connection_.world();
        auto selected = world->characters().find(selected_slot_);
        return world->state() == net::World::State::characters &&
               selected != world->characters().end() && selected->second.id &&
               !selected->second.pending_delete;
    }
    return false;
}
void Application::append_input(std::string_view utf8) {
    if (screen() != Screen::login)
        return;
    if (pending_auth_)
        return;
    for (auto point : decode_utf8(utf8))
        if (point < 32 || (point >= 127 && point <= 159))
            return;
    auto& field = focus_ ? password_ : username_;
    auto candidate = select_all_ ? std::string(utf8) : field + std::string(utf8);
    try {
        auto bytes = utf8_to_cp1252(candidate);
        if (bytes.size() > (focus_ ? 16u : 32u))
            return;
    } catch (const std::invalid_argument&) {
        status_ = "Este carácter no está disponible en el cliente.";
        return;
    }
    if (focus_ && select_all_)
        clear_password();
    field = std::move(candidate);
    select_all_ = false;
    status_.clear();
}
void Application::submit() {
    using Phase = net::Connection::Phase;
    if (pending_auth_)
        return;
    try {
        if (username_.empty() || password_.empty()) {
            status_ = "Introduce tu usuario y contraseña.";
            return;
        }
        (void)utf8_to_cp1252(username_);
        (void)utf8_to_cp1252(password_);
        if (connection_.phase() != Phase::login || !connection_.login() ||
            (connection_.login()->state() != net::Login::State::ready &&
             connection_.login()->state() != net::Login::State::rejected))
            connection_.connect(settings_.login_host, settings_.login_port, settings_.world_port);
        pending_auth_ = true;
        preferences_saved_ = false;
        status_ = "Conectando...";
    } catch (const std::exception&) {
        pending_auth_ = false;
        status_ = "No se pudo iniciar la conexión.";
    }
}
void Application::choose_server() {
    auto login = connection_.login();
    if (!login || login->state() != net::Login::State::authenticated || login->servers().empty())
        return;
    auto selected = std::find_if(login->servers().begin(), login->servers().end(),
                                 [&](const auto& server) { return server.id == selected_server_; });
    if (selected == login->servers().end())
        selected = login->servers().begin();
    login->select_server(selected->id, settings_.client_version);
    status_ = "Conectando al mundo...";
}
void Application::choose_character() {
    auto world = connection_.world();
    if (!world || world->state() != net::World::State::characters)
        return;
    auto found = world->characters().find(selected_slot_);
    if (found == world->characters().end() || !found->second.id) {
        status_ = "Selecciona un personaje.";
        return;
    }
    if (found->second.pending_delete) {
        status_ = "El personaje está pendiente de eliminación.";
        return;
    }
    world->select_character(found->second.id);
    status_ = "Entrando...";
}
void Application::handle_event(const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        running_ = false;
        return;
    }
    if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        int width = 0, height = 0;
        SDL_GL_GetDrawableSize(renderer_.window(), &width, &height);
        renderer_.resize(width, height);
        return;
    }
    if (event.type == SDL_TEXTINPUT) {
        append_input(event.text.text);
        return;
    }
    auto page = screen();
    if (event.type == SDL_KEYDOWN) {
        auto key = event.key.keysym.sym;
        if (key == SDLK_ESCAPE) {
            running_ = false;
            return;
        }
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            if (page == Screen::login)
                submit();
            else if (page == Screen::servers)
                choose_server();
            else if (page == Screen::characters)
                choose_character();
            return;
        }
        if (page == Screen::login && !pending_auth_) {
            if (key == SDLK_TAB) {
                focus_ = 1 - focus_;
                select_all_ = false;
            } else if (key == SDLK_a && (event.key.keysym.mod & KMOD_CTRL))
                select_all_ = true;
            else if (key == SDLK_v && (event.key.keysym.mod & KMOD_CTRL)) {
                char* value = SDL_GetClipboardText();
                if (value) {
                    append_input(value);
                    SDL_free(value);
                }
            } else if (key == SDLK_BACKSPACE) {
                auto& field = focus_ ? password_ : username_;
                if (select_all_) {
                    if (focus_)
                        clear_password();
                    else
                        field.clear();
                } else
                    erase_last_utf8(field);
                select_all_ = false;
            }
        }
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        auto point = logical_mouse(event.button.x, event.button.y);
        auto x = point[0], y = point[1];
        if (page == Screen::login) {
            if (inside(x, y, 480, 579, 126, 17)) {
                focus_ = 0;
                select_all_ = false;
            } else if (inside(x, y, 480, 603, 126, 17)) {
                focus_ = 1;
                select_all_ = false;
            } else if (inside(x, y, 546, 625, 18, 18)) {
                remember_id_ = !remember_id_;
                save_preferences();
            } else if (inside(x, y, 480, 682, 64, 32))
                submit();
        } else if (page == Screen::servers) {
            auto login = connection_.login();
            if (inside(x, y, 376, 236, 270, 300)) {
                auto row = std::size_t((y - 236) / 28);
                if (row < login->servers().size()) {
                    selected_server_ = login->servers()[row].id;
                    if (event.button.clicks >= 2)
                        choose_server();
                }
            } else if (inside(x, y, 480, 552, 64, 32))
                choose_server();
        } else if (page == Screen::characters) {
            if (inside(x, y, 52, 150, 216, 100)) {
                auto row = std::size_t((y - 150) / 20);
                auto world = connection_.world();
                if (row < world->characters().size()) {
                    auto found = world->characters().begin();
                    std::advance(found, static_cast<std::ptrdiff_t>(row));
                    selected_slot_ = found->first;
                    if (event.button.clicks >= 2)
                        choose_character();
                }
            } else if (inside(x, y, 480, 682, 64, 32))
                choose_character();
        }
    }
}
bool Application::frame() {
    auto now = std::chrono::steady_clock::now();
    float seconds = std::chrono::duration<float>(now - previous_frame_).count();
    previous_frame_ = now;
    SDL_Event event;
    while (SDL_PollEvent(&event))
        handle_event(event);
    connection_.poll();
    if (pending_auth_) {
        auto login = connection_.login();
        if (login && (login->state() == net::Login::State::ready ||
                      login->state() == net::Login::State::rejected)) {
            try {
                auto password = utf8_to_cp1252(password_);
                login->authenticate(utf8_to_cp1252(username_), password);
                OPENSSL_cleanse(password.data(), password.size());
                clear_password();
                pending_auth_ = false;
                status_ = "Verificando cuenta...";
            } catch (const std::exception&) {
                clear_password();
                pending_auth_ = false;
                status_ = "No se pudo enviar el acceso.";
            }
        }
    }
    if (connection_.phase() == net::Connection::Phase::failed ||
        connection_.phase() == net::Connection::Phase::closed) {
        pending_auth_ = false;
        clear_password();
        status_ = "No se pudo mantener la conexión con el servidor.";
    } else if (auto login = connection_.login()) {
        if (login->state() == net::Login::State::rejected)
            status_ = "No se pudo acceder a la cuenta.";
        else if (login->state() == net::Login::State::authenticated && !preferences_saved_) {
            status_.clear();
            save_preferences();
            preferences_saved_ = true;
        }
    }
    if (auto world = connection_.world()) {
        if (world->state() == net::World::State::rejected) {
            connection_.disconnect();
            status_ = "El servidor no aceptó la conexión al mundo.";
            update_character_preview(seconds);
            render();
            return running_;
        }
        if (world->state() == net::World::State::characters) {
            status_ = world->status() ? "No se pudo seleccionar el personaje." : std::string();
            auto selected = world->characters().find(selected_slot_);
            if (selected == world->characters().end() || !selected->second.id) {
                for (const auto& entry : world->characters())
                    if (entry.second.id) {
                        selected_slot_ = entry.first;
                        break;
                    }
            }
        }
    }
    auto current_world = connection_.world();
    auto map_path = current_world ? current_world->selected_map_path() : std::nullopt;
    if (!map_path) {
        loaded_terrain_.reset();
        loaded_dungeon_.reset();
        loaded_water_.reset();
        loaded_objects_.reset();
        motion_.reset();
        fog_known_ = false;
        loaded_terrain_path_.clear();
    } else if (*map_path != loaded_terrain_path_) {
        loaded_terrain_.reset();
        loaded_dungeon_.reset();
        loaded_water_.reset();
        loaded_objects_.reset();
        loaded_terrain_path_ = *map_path;
        try {
            auto bytes = archive_.read(*map_path);
            if (decode_world_kind(bytes) == WorldKind::dungeon) {
                // 004518a0 DUN branch: the descriptor names the DG under data/world/dungeon.
                loaded_dungeon_ = std::make_unique<DungeonResource>(
                    renderer_, archive_, decode_dungeon_descriptor(bytes).path);
            } else {
                std::optional<std::array<float, 2>> center;
                if (auto* player = current_world->player())
                    center = std::array<float, 2>{player->position[0], player->position[2]};
                loaded_terrain_ = std::make_unique<TerrainResource>(
                    renderer_, archive_, *map_path, default_terrain_scale(), center);
                // 004518a0: the .wtr name follows the grid and material records.
                const auto& field = loaded_terrain_->data();
                auto water_name = decode_terrain_water_name(
                    bytes, field.prefix.bytes_consumed + field.grid.bytes_consumed + 4 +
                               516 * field.materials.size());
                try {
                    auto tail = decode_world_tail(
                        bytes,
                        field.prefix.bytes_consumed + field.grid.bytes_consumed + 4 +
                            516 * field.materials.size() + 0x100,
                        true);
                    loaded_objects_ = std::make_unique<WorldObjects>(renderer_, archive_, tail);
                    for (int channel = 0; channel < 3; ++channel) {
                        float value;
                        std::memcpy(&value, tail.environment.data() + 24 + 4 * channel, 4);
                        fog_color_[channel] = std::clamp(value, 0.f, 255.f) / 255.f;
                    }
                    fog_start_ = 50.f;
                    fog_end_ = 100.f;
                    fog_known_ = true;
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "World objects: %s\n", e.what());
                }
                if (!water_name.empty())
                    loaded_water_ = std::make_unique<BasicWater>(
                        renderer_, archive_, "data/entity/water/" + water_name);
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "Terrain loading: %s\n", e.what());
        }
    }
    if (loaded_water_)
        loaded_water_->advance(seconds);
    update_motion(seconds);
    update_character_preview(seconds);
    render();
    return running_;
}
void Application::update_motion(float seconds) {
    if (fog_known_) {
        fog_start_ = std::min(200.f, fog_start_ + 20.f * seconds);
        fog_end_ = std::min(400.f, fog_end_ + 20.f * seconds);
    }
    auto world = connection_.world();
    auto* player = world ? world->player() : nullptr;
    if ((!loaded_terrain_ && !loaded_dungeon_) || screen() != Screen::loading || !player ||
        !std::isfinite(player->position[0]) || !std::isfinite(player->position[2])) {
        motion_.reset();
        return;
    }
    const auto scale = default_terrain_scale();
    GroundHeight ground;
    CollisionWorld collision;
    if (loaded_terrain_)
        ground = [&](float x, float z) {
            return loaded_terrain_->heights().world_height(x, z, scale.inverse_horizontal,
                                                           scale.vertical, scale.offset,
                                                           scale.inverse_vertical);
        };
    if (loaded_terrain_ && loaded_objects_)
        collision = [&](CollisionPacket& packet) { loaded_objects_->collide(packet); };
    if (!loaded_terrain_) // dungeons have no height field: the vertical collision pass is the floor
        collision = [&](CollisionPacket& packet) { check_dungeon(packet, loaded_dungeon_->data()); };
    if (!motion_) {
        float y = loaded_terrain_ ? ground(player->position[0], player->position[2]) + .9f
                                  : player->position[1] + .9f;
        motion_ = initial_motion_state({player->position[0], y, player->position[2]},
                                       player->details ? player->details->orientation : 180);
    }
    // DirectInput scancodes of the default bindings (0070a4f0) -> SDL.
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    MotionKeys input;
    input.turn_left = keys[SDL_SCANCODE_A];
    input.turn_right = keys[SDL_SCANCODE_D];
    input.forward = keys[SDL_SCANCODE_W];
    input.backward = keys[SDL_SCANCODE_S];
    input.strafe_left = keys[SDL_SCANCODE_Q];
    input.strafe_right = keys[SDL_SCANCODE_E];
    bool jump = keys[SDL_SCANCODE_SPACE];
    input.jump_pressed = jump && !jump_down_;
    jump_down_ = jump;
    MotionParameters parameters;
    parameters.world_extent =
        loaded_terrain_ ? std::int32_t(loaded_terrain_->data().prefix.field) : 0;
    step_motion(*motion_, input, parameters, std::min(seconds, .1f), collision, ground);
    // 0043bfa0 (0x441b66 dispatch): forward plays walk (1) or run (2), S the back
    // step (3), Q/E the left/right step (4/5), a jump action 8, otherwise idle.
    if (character_preview_) {
        unsigned action = 0;
        if (!motion_->grounded || motion_->jumping)
            action = 8;
        else if (input.forward)
            action = parameters.running ? 2 : 1;
        else if (input.backward)
            action = 3;
        else if (input.strafe_left)
            action = 4;
        else if (input.strafe_right)
            action = 5;
        character_preview_->set_action(action);
    }
    // 00437050: while a movement key is held the position is sent every
    // 500 ms (state flag 0x80 clear); releasing sends one final packet with 0x80
    // set. 0x40 marks a grounded player.
    bool held = input.forward || input.backward || input.turn_left || input.turn_right ||
                input.strafe_left || input.strafe_right;
    auto now = std::chrono::steady_clock::now();
    bool send_now = (held && (!moving_ || now - last_move_sync_ >= std::chrono::milliseconds(500))) ||
                    (!held && moving_);
    if (send_now && world->state() == net::World::State::loading) {
        auto packet = net::encode_move_packet(
            net::movement_state_byte(held ? 0 : 4, motion_->grounded, !held),
            orientation_from_heading(motion_->heading), motion_->position);
        if (packet)
            world->send_packet(*packet);
        last_move_sync_ = now;
    }
    moving_ = held;
}
void Application::update_character_preview(float seconds) {
    auto clear = [&] {
        character_preview_.reset();
        character_preview_key_.clear();
        character_preview_failed_ = false;
    };
    if (screen() != Screen::characters &&
        !(screen() == Screen::loading && (loaded_terrain_ || loaded_dungeon_))) {
        clear();
        return;
    }
    auto world = connection_.world();
    auto found = world->characters().find(selected_slot_);
    if (found == world->characters().end() || !found->second.id) {
        clear();
        return;
    }
    const auto& slot = found->second;
    // Rigid weapons and shields still need their own attachment path; their
    // presence must not prevent the rest of the character from being shown.
    auto key = std::to_string(slot.id) + ":" + std::to_string(slot.race) + ":" +
               std::to_string(slot.job) + ":" + std::to_string(slot.gender) + ":" +
               std::to_string(slot.face) + ":" + std::to_string(slot.hair);
    for (unsigned i = 0; i < slot.equipment_types.size(); ++i)
        key += ":" + std::to_string(slot.equipment_types[i]) + ":" +
               std::to_string(slot.equipment_ids[i]);
    if (key != character_preview_key_) {
        character_preview_.reset();
        character_preview_key_ = std::move(key);
        character_preview_failed_ = false;
        try {
            if (!items_)
                items_ = std::make_unique<ItemTable>(
                    decode_items(decode_sdata(archive_.read("data/Item/Item.SData"))));
            auto appearance = character_appearance(*items_, slot.face, slot.hair,
                                                   {slot.equipment_types, slot.equipment_ids});
            character_preview_ = std::make_unique<CharacterPreview>(
                renderer_, archive_, slot.race, slot.job, slot.gender, appearance,
                character_cloak(*items_, {slot.equipment_types, slot.equipment_ids}));
        } catch (const std::exception& e) {
            character_preview_failed_ = true;
            std::fprintf(stderr, "Character preview: %s\n", e.what());
        }
    }
    if (character_preview_)
        character_preview_->advance(seconds);
    if (character_preview_failed_)
        status_ = "No se pudo cargar el personaje.";
}
void Application::rectangle(float x, float y, float width, float height, std::uint32_t argb) {
    auto offset = origin();
    auto s = scale();
    x = offset[0] + x * s;
    y = offset[1] + y * s;
    width *= s;
    height *= s;
    const UiVertex vertices[] = {{x, y, 0, 1, argb, 0, 0},
                                 {x + width, y, 0, 1, argb, 1, 0},
                                 {x + width, y + height, 0, 1, argb, 1, 1},
                                 {x, y, 0, 1, argb, 0, 0},
                                 {x + width, y + height, 0, 1, argb, 1, 1},
                                 {x, y + height, 0, 1, argb, 0, 1}};
    renderer_.object({});
    renderer_.bind_images({});
    renderer_.draw(Layout::ui, VertexPass::ui, PixelPass::ui, vertices, 6,
                   {Blend::alpha, Depth::disabled, Cull::none, Sampling::linear_clamp});
}
void Application::image(const Image& value, float x, float y, float width, float height, int sx,
                        int sy, int sw, int sh) {
    if (!sw)
        sw = value.width;
    if (!sh)
        sh = value.height;
    auto offset = origin();
    auto s = scale();
    x = offset[0] + x * s;
    y = offset[1] + y * s;
    width *= s;
    height *= s;
    float u = float(sx) / value.width, v = float(sy) / value.height,
          ur = float(sx + sw) / value.width, vb = float(sy + sh) / value.height;
    const UiVertex vertices[] = {{x, y, 0, 1, 0xffffffff, u, v},
                                 {x + width, y, 0, 1, 0xffffffff, ur, v},
                                 {x + width, y + height, 0, 1, 0xffffffff, ur, vb},
                                 {x, y, 0, 1, 0xffffffff, u, v},
                                 {x + width, y + height, 0, 1, 0xffffffff, ur, vb},
                                 {x, y + height, 0, 1, 0xffffffff, u, vb}};
    renderer_.object({});
    renderer_.bind_images({value.texture, 0, 0, 0});
    renderer_.draw(Layout::ui, VertexPass::ui, PixelPass::ui, vertices, 6,
                   {Blend::alpha, Depth::disabled, Cull::none, Sampling::linear_clamp});
}
void Application::label(std::string_view value, float x, float y, std::uint32_t color) {
    auto offset = origin();
    auto s = scale();
    text_.draw(value, offset[0] + x * s, offset[1] + y * s, color, s);
}
bool Application::draw_world_actor() {
    auto world = connection_.world();
    auto* player = world ? world->player() : nullptr;
    if (!player || (!loaded_terrain_ && !loaded_dungeon_) || !character_preview_)
        return false;
    const auto terrain_scale = default_terrain_scale();
    const float x = motion_ ? motion_->position[0] : player->position[0],
                z = motion_ ? motion_->position[2] : player->position[2];
    if (!std::isfinite(x) || !std::isfinite(z))
        return false;
    const float y = motion_ ? motion_->position[1] - .9f
                    : !loaded_terrain_ ? player->position[1]
                            : loaded_terrain_->heights().world_height(
        x, z, terrain_scale.inverse_horizontal, terrain_scale.vertical, terrain_scale.offset,
        terrain_scale.inverse_vertical);
    const float radius = std::max(character_preview_->radius(), .01f);
    const Vec3 feet = {x, y, z};
    // Third-person framing in true world units: the character is not
    // normalized, so terrain, shadow and actor share one coordinate system.
    SceneUniforms scene;
    scene.view = motion_ ? look_at_lh({motion_->eye[0], motion_->eye[1], motion_->eye[2]},
                                      {motion_->target[0], motion_->target[1], motion_->target[2]},
                                      {0, 1, 0})
                         : look_at_lh({x, y + radius * 1.3f, z - radius * 3.4f},
                                      {x, y + radius * .55f, z}, {0, 1, 0});
    scene.view_projection =
        multiply(scene.view, perspective_lh(.9f, float(renderer_.width()) / renderer_.height(),
                                            radius * .05f, radius * 600));
    Matrix placement = motion_ ? WorldObjects::placement_matrix(feet, {-motion_->heading[0], 0, -motion_->heading[2]}, {0, 1, 0}) // actor models face local -z
                               : identity;
    placement.m[3][0] = x;
    placement.m[3][1] = y;
    placement.m[3][2] = z;
    if (loaded_terrain_)
        actor_shadow_.render(feet, radius, [&] { character_preview_->draw_shadow_caster_at(placement); });
    if (fog_known_) {
        scene.fog = {fog_start_, fog_end_, 1.f, 0.f};
        scene.fog_color = {fog_color_[0], fog_color_[1], fog_color_[2], 1.f};
    }
    renderer_.scene(scene);
    if (loaded_dungeon_)
        loaded_dungeon_->draw(identity);
    if (loaded_terrain_)
        loaded_terrain_->draw(identity);
    if (loaded_objects_)
        loaded_objects_->draw({x, z}, 350.f, identity);
    if (loaded_water_)
        loaded_water_->draw(identity, y + radius * 1.3f);
    if (loaded_terrain_ && actor_shadow_.valid())
        loaded_terrain_->draw_shadow(identity, actor_shadow_.texture(), actor_shadow_.sampling());
    character_preview_->draw_at(placement);
    return true;
}
void Application::render() {
    auto page = screen();
    // The original's frame clear colour (renderer +0x1624) is only ever written
    // as opaque black (`00543852`); the native UI keeps its own backdrop.
    renderer_.clear(fog_known_ && page == Screen::loading ? Color{0, 0, 0, 1}
                                                          : Color{.025f, .035f, .06f, 1});
    SceneUniforms scene;
    bool show_terrain = bool(loaded_terrain_) && page == Screen::loading;
    bool show_dungeon = bool(loaded_dungeon_) && page == Screen::loading;
    Vec3 eye = character_preview_ && page == Screen::characters ? Vec3{0, .3f, -2.6f}
             : show_terrain                                     ? Vec3{0, 1.1f, -1.9f}
                                                                 : Vec3{0, .4f, -2.4f};
    scene.view = look_at_lh(eye, {0, 0, 0}, {0, 1, 0});
    scene.view_projection = multiply(
        scene.view, perspective_lh(1.f, float(renderer_.width()) / renderer_.height(), .01f, 100));
    renderer_.scene(scene);
    if (character_preview_ && page == Screen::characters)
        character_preview_->draw();
    else if ((show_terrain || show_dungeon) && character_preview_ && draw_world_actor())
        ;
    else if (show_terrain)
        loaded_terrain_->draw(loaded_terrain_->fitted_world());
    else if (show_dungeon)
        loaded_dungeon_->draw(loaded_dungeon_->fitted_world());
    else {
        auto world = backdrop_.fitted_world();
        world.m[3][1] += .2f;
        backdrop_.draw(world);
    }
    // Original post-effect order: 3D scene, glow composite, then interface.
    if (settings_.glow_quality)
        glow_.apply(2, GlowEffect::strength(0, settings_.glow_quality));
    if (page == Screen::login || page == Screen::connecting) {
        image(login_image_, 400, 524, 224, 200, 0, 0, 224, 200);
        if (remember_id_) {
            rectangle(550, 633, 4, 3, 0xffd6bd86);
            rectangle(554, 629, 6, 4, 0xffd6bd86);
        }
        auto selected = focus_ ? 603.f : 579.f;
        rectangle(480, selected, 126, 17, 0x503e5265);
        auto offset = origin();
        auto s = scale();
        renderer_.scissor(int(offset[0] + 481 * s), int(offset[1] + 579 * s), int(124 * s),
                          int(17 * s));
        label(username_, 482, 577);
        renderer_.reset_scissor();
        renderer_.scissor(int(offset[0] + 481 * s), int(offset[1] + 603 * s), int(124 * s),
                          int(17 * s));
        label(std::string(decode_utf8(password_).size(), '*'), 482, 601);
        renderer_.reset_scissor();
        if (page == Screen::login)
            image(ok_image_, 480, 682, 64, 32, 0, 0, 64, 32);
    } else if (page == Screen::servers) {
        image(server_image_, 356, 163, 312, 442, 0, 0, 312, 442);
        auto login = connection_.login();
        for (std::size_t i = 0; i < login->servers().size() && i < 10; ++i) {
            const auto& server = login->servers()[i];
            float y = 236 + float(i) * 28;
            if (server.id == selected_server_ || (!selected_server_ && i == 0))
                rectangle(376, y, 270, 26, 0x60435468);
            label(cp1252_to_utf8(server.name), 382, y + 3);
            label(std::to_string(server.users) + "/" + std::to_string(server.capacity), 560, y + 3,
                  0xffdfc797);
        }
        image(ok_image_, 480, 552, 64, 32, 0, 0, 64, 32);
    } else if (page == Screen::characters) {
        image(character_image_, 32, 95, 256, 584, 0, 0, 256, 584);
        label("Personajes", 105, 130, 0xffdfc797);
        auto game = connection_.world();
        unsigned row = 0;
        for (const auto& entry : game->characters()) {
            if (row >= 5)
                break;
            const auto& slot = entry.second;
            float y = 150 + row++ * 20.f;
            if (slot.slot == selected_slot_)
                rectangle(52, y, 216, 20, 0x60435468);
            label(slot.id ? cp1252_to_utf8(slot.name) : "—", 60, y + 1,
                  slot.pending_delete ? 0xff888888 : 0xffffffff);
        }
        auto found = game->characters().find(selected_slot_);
        if (found != game->characters().end() && found->second.id) {
            const auto& selected = found->second;
            label(cp1252_to_utf8(selected.name), 60, 350, 0xffdfc797);
            label("Nivel " + std::to_string(selected.level), 60, 380);
            label("Clase " + std::to_string(selected.job), 60, 405);
            label("HP  " + std::to_string(selected.resources[0]), 60, 440);
            label("MP  " + std::to_string(selected.resources[1]), 60, 465);
            label("SP  " + std::to_string(selected.resources[2]), 60, 490);
        }
        image(ok_image_, 480, 682, 64, 32, 0, 0, 64, 32);
    } else {
        rectangle(362, 620, 300, 48, 0xc0101218);
        label("Cargando...", 470, 634, 0xffdfc797);
    }
    if (!status_.empty()) {
        rectangle(220, 735, 584, 25, 0xd0000000);
        label(status_, 230, 738, 0xffdfc797);
    }
    renderer_.present();
}
void Application::capture(const std::filesystem::path& output) {
    if (std::filesystem::exists(output))
        throw std::runtime_error("Capture output already exists");
    auto pixels = renderer_.read_pixels();
    std::ofstream out(output, std::ios::binary);
    out << "P6\n" << renderer_.width() << ' ' << renderer_.height() << "\n255\n";
    for (std::size_t i = 0; i < pixels.size(); i += 4)
        out.write(reinterpret_cast<const char*>(pixels.data() + i), 3);
    out.close();
    if (!out)
        throw std::runtime_error("Cannot write application capture");
}
} // namespace genesis
