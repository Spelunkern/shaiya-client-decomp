#pragma once

#include <cstdint>

// Serialized vertex fields are fixed-width; runtime pointers never belong here.
namespace shaiya::graphics::native {
struct Matrix {
    float m[4][4];
};
struct Color {
    float r, g, b, a;
};
enum class Blend : uint8_t { opaque, alpha, additive, color_add, multiply, inverse_alpha };
// Mirrors the D3D11_BLEND factors the recovered client actually authors
// (water's constant-factor blend and the actor effect catalog's 13-entry
// table); SRC1_* dual-source factors are never chosen by either and are
// deliberately not represented here.
enum class BlendFactor : uint8_t {
    zero,
    one,
    src_color,
    inv_src_color,
    src_alpha,
    inv_src_alpha,
    dest_alpha,
    inv_dest_alpha,
    dest_color,
    inv_dest_color,
    src_alpha_sat,
    blend_factor,
    inv_blend_factor
};
enum class Depth : uint8_t { disabled, read, write };
enum class Cull : uint8_t { none, front, back };
enum class Sampling : uint8_t {
    point_clamp,
    linear_clamp,
    linear_wrap,
    anisotropic_wrap,
    linear_mirror
};

struct PointLight {
    Color diffuse = {0, 0, 0, 1};
    float position[3] = {};
    float range = 2;
    float attenuation[3] = {1, 0, 0};
    bool enabled = false;
};
struct Scene {
    Matrix view_projection;
    Matrix view;
    Color ambient = {1, 1, 1, 1};
    Color directional = {0, 0, 0, 1};
    // Direction in which the light's rays travel, in world coordinates.
    float light_direction[4] = {0, -1, 0, 0};
    float camera[4] = {};
    float fog_start = 0, fog_end = 1;
    bool fog_enabled = false;
    Color fog_color = {};
    float clip_plane[4] = {};
    PointLight point_light;
};

// Each layout describes authored asset data, never an FVF or mutable declaration.
struct UiVertex {
    float x, y, z, rhw;
    uint32_t argb;
    float u, v;
};
struct UiDualVertex36 {
    float x, y, z, rhw;
    uint32_t argb;
    float u, v, mask_u, mask_v;
};
static_assert(sizeof(UiDualVertex36) == 36);
struct MeshVertex {
    float position[3], normal[3];
    uint32_t argb;
    float uv[2];
};
struct LightmapVertex {
    float position[3], normal[3];
    uint32_t argb;
    float uv[2], lightmap_uv[2];
};
struct ModelVertex {
    float position[3], normal[3], uv[2];
};
struct SkinVertex {
    float position[3], weight;
    uint8_t bones[4];
    float normal[3], uv[2];
};
struct WorldVertex {
    float position[3];
    uint32_t argb;
    float uv[2];
};
struct TexturedWorldVertex {
    float position[3], uv[2];
};
struct TexturedWorldMulti28 {
    float position[3], uv[2], secondary_uv[2];
};
struct TexturedWorldMulti36 {
    float position[3], uv[2], secondary_uv[2], detail_uv[2];
};
struct WaterVertex32 {
    float position[3];
    uint32_t argb;
    float uv[2], reflection_uv[2];
};
struct WaterVertex56 {
    float position[3], normal[3], uv[2], uv1[2], uv2[2], uv3[2];
};
static_assert(sizeof(UiVertex) == 28);
static_assert(sizeof(MeshVertex) == 36);
static_assert(sizeof(LightmapVertex) == 44);
static_assert(sizeof(ModelVertex) == 32);
static_assert(sizeof(SkinVertex) == 40);
static_assert(sizeof(WorldVertex) == 24);
static_assert(sizeof(TexturedWorldVertex) == 20);
static_assert(sizeof(TexturedWorldMulti28) == 28);
static_assert(sizeof(TexturedWorldMulti36) == 36);
static_assert(sizeof(WaterVertex32) == 32);
static_assert(sizeof(WaterVertex56) == 56);

} // namespace shaiya::graphics::native
