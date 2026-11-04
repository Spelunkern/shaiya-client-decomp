#pragma once
#include "connection.h"
#include "model_resource.h"
#include "glow.h"
#include "shadow.h"
#include "text.h"
#include "player_motion.h"
#include <optional>
#include <chrono>

namespace genesis {
class CharacterPreview;
class TerrainResource;
class DungeonResource;
class BasicWater;
class WorldObjects;
struct ItemTable;
struct AppSettings {
    std::filesystem::path assets = "assets", font, preferences;
    std::string login_host = "127.0.0.1";
    std::uint16_t login_port = 30800, world_port = 30810;
    std::uint32_t client_version = 0x11122200;
    int width = 1024, height = 768;
    bool hidden = false;
    // Glow quality 0 (off, original default), 1 or 2; see GlowEffect::strength.
    unsigned glow_quality = 0;
};
// The native application entry is integrated here as the recovered engine's
// owners are migrated. It does not yet implement the entire gameplay runtime.
class Application {
  public:
    enum class Screen { login, connecting, servers, characters, loading };
    explicit Application(AppSettings settings);
    ~Application();
    void handle_event(const SDL_Event& event);
    bool frame();
    Screen screen();
    bool can_confirm();
    void capture(const std::filesystem::path& output);
    Renderer& renderer() {
        return renderer_;
    }
    bool character_preview_ready() const {
        return bool(character_preview_);
    }
    bool motion_active() const {
        return motion_.has_value();
    }
    std::array<float, 3> motion_position() const {
        return motion_ ? motion_->position : std::array<float, 3>{};
    }
    bool objects_ready() const {
        return bool(loaded_objects_);
    }
    std::size_t world_objects_drawn() const;
    bool water_ready() const {
        return bool(loaded_water_);
    }
    bool dungeon_ready() const {
        return bool(loaded_dungeon_);
    }
    bool terrain_ready() const {
        return bool(loaded_terrain_);
    }
    std::size_t terrain_material_count() const;
    bool world_actor_shadow_ready() const {
        return actor_shadow_.valid();
    }
    std::optional<std::string> terrain_surface_sound(float x, float z) const;

  private:
    struct Image {
        GLuint texture = 0;
        int width = 0, height = 0;
    };
    Image load_image(std::string_view path);
    void render();
    bool draw_world_actor();
    void label(std::string_view value, float x, float y, std::uint32_t argb = 0xffffffff);
    void rectangle(float x, float y, float width, float height, std::uint32_t argb);
    void image(const Image& image, float x, float y, float width, float height, int source_x = 0,
               int source_y = 0, int source_width = 0, int source_height = 0);
    void submit();
    void choose_server();
    void choose_character();
    void append_input(std::string_view utf8);
    void clear_password();
    void save_preferences();
    void update_character_preview(float seconds);
    std::array<float, 2> logical_mouse(int x, int y) const;
    float scale() const;
    std::array<float, 2> origin() const;
    AppSettings settings_;
    Renderer renderer_;
    GlowEffect glow_{renderer_};
    ActorShadow actor_shadow_{renderer_};
    Font font_;
    Text text_;
    shaiya::portable::Archive archive_;
    ModelResource backdrop_;
    Image login_image_, server_image_, character_image_, ok_image_;
    net::Connection connection_;
    std::unique_ptr<ItemTable> items_;
    std::unique_ptr<CharacterPreview> character_preview_;
    std::string character_preview_key_;
    bool character_preview_failed_ = false;
    std::unique_ptr<TerrainResource> loaded_terrain_;
    std::unique_ptr<DungeonResource> loaded_dungeon_;
    std::unique_ptr<BasicWater> loaded_water_;
    std::unique_ptr<WorldObjects> loaded_objects_;
    // Local exploration of the connected world with the original's default
    // movement keys (player_motion.h); not synchronised with the server.
    std::optional<MotionState> motion_;
    bool jump_down_ = false;
    void update_motion(float seconds);
    std::string loaded_terrain_path_;
    std::chrono::steady_clock::time_point previous_frame_ = std::chrono::steady_clock::now();
    std::string username_, password_, status_;
    bool running_ = true, pending_auth_ = false, select_all_ = false, remember_id_ = false,
         preferences_saved_ = false;
    unsigned focus_ = 0;
    std::uint8_t selected_server_ = 0, selected_slot_ = 0;
};
} // namespace genesis
