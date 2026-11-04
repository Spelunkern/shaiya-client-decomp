#pragma once
#include "gl_api.h"
#include "graphics/render_types.h"
#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace genesis {
using namespace shaiya::graphics::native;
inline constexpr Matrix identity = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};

enum class Layout {
    ui,
    ui_dual,
    mesh,
    lightmap,
    model,
    skin,
    world,
    textured_world,
    multi28,
    multi36,
    water32,
    water56
};
enum class VertexPass {
    ui,
    ui_dual,
    mesh,
    lit,
    lightmap,
    model,
    skin,
    world,
    reflective_model,
    reflective_skin,
    textured_world,
    multi28,
    multi36,
    unlit_model,
    water32,
    water56,
    bump_reflect,
    fullscreen
};
enum class PixelPass {
    ui,
    mask,
    surface,
    lightmap,
    water,
    copy,
    eight_tap,
    gaussian_x,
    gaussian_y,
    gamma,
    reflection,
    projected_shadow,
    projected_effect,
    multi36,
    multi28,
    replace28,
    signed28,
    replace36,
    signed36,
    ui_dual,
    water56,
    bump_reflect
};
enum class IndexType { u16, u32 };

// Mirrors native_render_passes.cpp's blend_state(): when present, replaces
// the named Blend mode with the original client's explicit D3D11_BLEND
// source/destination factor pair (water's constant-factor blend, authored
// actor-effect catalog entries).
struct CustomBlend {
    BlendFactor source, destination;
};
struct Style {
    Blend blend = Blend::opaque;
    Depth depth = Depth::write;
    Cull cull = Cull::back;
    Sampling sampling = Sampling::linear_wrap;
    std::optional<CustomBlend> custom_blend;
    Color blend_factor = {1, 1, 1, 1};
};
// Match the original constant buffers byte-for-byte, independent of host ABI.
struct alignas(16) SceneUniforms {
    Matrix view_projection = identity, view = identity;
    Color ambient = {1, 1, 1, 1}, directional = {0, 0, 0, 1};
    std::array<float, 4> light_direction = {0, -1, 0, 0}, camera = {}, fog = {};
    Color fog_color = {};
    std::array<float, 4> clip_plane = {}, viewport = {};
    Color point_color = {};
    std::array<float, 4> point_position_range = {}, point_attenuation = {};
};
struct alignas(16) ObjectUniforms {
    Matrix world = identity, reflection = identity;
    Color tint = {1, 1, 1, 1}, diffuse = {1, 1, 1, 1}, ambient = {1, 1, 1, 1};
    std::array<float, 4> parameters = {7.5f / 255, 0, 0, 1}, uv = {};
    Color specular = {};
    std::array<float, 4> surface = {1, 1, 0, 0};
    Color emissive = {};
    Matrix environment = identity;
    Color environment_eye = {0, 0, 0, 1};
    Matrix texture_projection = identity;
    std::array<float, 4> projection_flags = {};
};
struct alignas(16) WaterUniforms {
    Matrix refraction = identity;
    Color color = {63.f / 255, 158.f / 255, 102.f / 255, 1};
    std::array<float, 4> parameters = {.002f, 240.f / 255, 114.f / 255, 0}, offsets = {};
};
static_assert(sizeof(SceneUniforms) == 304 && sizeof(ObjectUniforms) == 416 &&
              sizeof(WaterUniforms) == 112);

class Renderer {
  public:
    using TargetId = std::size_t;
    using MeshId = std::size_t;
    struct Target {
        GLuint framebuffer = 0, color = 0, depth = 0;
        int width = 0, height = 0;
    };
    Renderer(int width, int height, bool hidden = false);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    GL& api() {
        return gl_;
    }
    const char* version() const;
    SDL_Window* window() const {
        return window_;
    }
    void resize(int width, int height);
    TargetId create_target(int width, int height);
    void bind_target(TargetId id); // 0 is the main surface
    TargetId current_target() const {
        return current_target_;
    }
    Target target(TargetId id) const;
    void clear(Color color);
    void present();
    void scene(const SceneUniforms& data);
    void object(const ObjectUniforms& data);
    void water(const WaterUniforms& data);
    void bones(const Matrix* matrices, std::size_t count);
    GLuint texture_rgba(int width, int height, const void* pixels, bool mipmaps = false);
    void update_texture(GLuint texture, int x, int y, int width, int height, const void* pixels);
    void release_texture(GLuint texture);
    void bind_images(std::array<GLuint, 4> images);
    MeshId create_mesh(Layout layout, const void* vertices, std::size_t count,
                       const void* indices = nullptr, std::size_t index_count = 0,
                       IndexType type = IndexType::u16);
    void draw_mesh(MeshId mesh, VertexPass vertex, PixelPass pixel, const Style& style = {},
                   std::size_t first_index = 0, std::size_t index_count = 0, int base_vertex = 0);
    void release_mesh(MeshId mesh);
    void draw(Layout layout, VertexPass vertex, PixelPass pixel, const void* vertices,
              std::size_t count, const Style& style = {}, const std::uint16_t* indices = nullptr,
              std::size_t index_count = 0);
    void fullscreen(PixelPass pixel, const Style& style = {});
    void scissor(int x, int y, int width, int height);
    void reset_scissor();
    std::vector<std::uint8_t> read_pixels(); // top-left origin, RGBA8
    int width() const {
        return width_;
    }
    int height() const {
        return height_;
    }
    void check(const char* operation);

  private:
    friend class TargetScope;
    void shutdown();
    GLuint program(VertexPass vertex, PixelPass pixel);
    void apply(const Style& style, PixelPass pixel);
    void uniform(GLuint buffer, GLuint binding, const void* data, std::size_t size);
    void allocate_target(Target& target, int width, int height);
    void configure_layout(Layout layout);
    struct Mesh {
        GLuint vao = 0, vertices = 0, indices = 0;
        Layout layout = Layout::mesh;
        IndexType index_type = IndexType::u16;
        std::size_t vertex_count = 0;
        std::vector<std::uint8_t> asset_vertices;
        std::vector<std::uint32_t> asset_indices;
    };
    SDL_Window* window_ = nullptr;
    SDL_GLContext context_ = nullptr;
    GL gl_;
    int width_ = 0, height_ = 0;
    GLuint vao_ = 0, vertex_buffer_ = 0, index_buffer_ = 0;
    GLuint scene_buffer_ = 0, object_buffer_ = 0, skin_buffer_ = 0, water_buffer_ = 0;
    GLuint target_ = 0, color_ = 0, depth_ = 0, white_ = 0;
    int main_width_ = 0, main_height_ = 0;
    Target main_target_;
    std::vector<Target> targets_;
    std::vector<Mesh> meshes_;
    TargetId current_target_ = 0;
    std::array<GLuint, 4> images_ = {};
    std::array<GLuint, 5> samplers_ = {};
    std::vector<GLuint> textures_;
    std::array<std::array<GLuint, 22>, 18> programs_ = {};
    SceneUniforms scene_;
};
class TargetScope {
  public:
    TargetScope(Renderer& renderer, Renderer::TargetId target)
        : renderer_(renderer), saved_(renderer.current_target()), scene_(renderer.scene_) {
        renderer_.api().GetIntegerv(GL_SCISSOR_TEST, &scissor_enabled_);
        renderer_.api().GetIntegerv(GL_SCISSOR_BOX, scissor_.data());
        renderer_.bind_target(target);
    }
    ~TargetScope() {
        renderer_.bind_target(saved_);
        renderer_.scene(scene_);
        renderer_.api().Scissor(scissor_[0], scissor_[1], scissor_[2], scissor_[3]);
        if (scissor_enabled_)
            renderer_.api().Enable(GL_SCISSOR_TEST);
    }
    TargetScope(const TargetScope&) = delete;
    TargetScope& operator=(const TargetScope&) = delete;

  private:
    Renderer& renderer_;
    Renderer::TargetId saved_;
    SceneUniforms scene_;
    GLint scissor_enabled_ = 0;
    std::array<GLint, 4> scissor_ = {};
};
} // namespace genesis
