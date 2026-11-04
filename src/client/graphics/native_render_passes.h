#pragma once

#include <d3d11.h>
#include <stdint.h>
#include "graphics/render_types.h"

namespace shaiya::graphics::native {
struct NativeRenderer;

struct DrawStyle {
    Blend blend = Blend::opaque;
    Depth depth = Depth::write;
    Cull cull = Cull::back;
    Sampling sampling = Sampling::linear_wrap;
    Color tint = {1, 1, 1, 1};
    float alpha_cutoff = 7.5f / 255.0f;
    // Zero-valued selectors use the preset. Other values are native D3D11 factors.
    D3D11_BLEND source_blend = D3D11_BLEND(0), destination_blend = D3D11_BLEND(0);
    Color blend_factor = {1, 1, 1, 1};
};
struct MeshRange {
    ID3D11Buffer* vertices = nullptr;
    ID3D11Buffer* indices = nullptr;
    DXGI_FORMAT index_format = DXGI_FORMAT_R16_UINT;
    UINT first_index = 0, index_count = 0;
    INT base_vertex = 0;
    UINT vertex_byte_offset = 0;
};
struct Material {
    ID3D11ShaderResourceView* diffuse = nullptr;
    ID3D11ShaderResourceView* environment = nullptr;
    DrawStyle style;
    Color diffuse_color = {1, 1, 1, 1};
    Color ambient_color = {1, 1, 1, 1};
    Color specular_color = {0, 0, 0, 1};
    Color emissive_color = {0, 0, 0, 1};
    float specular_power = 1;
    Matrix environment_transform = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
    Color environment_eye = {0, 0, 0, 1};
    bool explicit_environment_frame = false;
    Matrix texture_projection = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
    bool projected_texture = false;
};
enum class UiPrimitive : uint8_t { triangles, quad, lines };
struct UiDraw {
    const UiVertex* vertices = nullptr;
    UINT vertex_count = 0;
    UiPrimitive primitive = UiPrimitive::triangles;
    ID3D11ShaderResourceView* image = nullptr;
    ID3D11ShaderResourceView* mask = nullptr;
    DrawStyle style = {Blend::alpha, Depth::disabled, Cull::none, Sampling::linear_clamp};
    const D3D11_RECT* scissor = nullptr;
};
struct UiDualDraw {
    const UiDualVertex36* vertices = nullptr;
    UINT vertex_count = 0;
    UiPrimitive primitive = UiPrimitive::quad;
    ID3D11ShaderResourceView *image = nullptr, *mask = nullptr;
    bool use_mask_rgb = false;
    DrawStyle style = {Blend::alpha, Depth::disabled, Cull::none, Sampling::linear_clamp};
    const D3D11_RECT* scissor = nullptr;
};
struct StaticDraw {
    // Native resources and one explicit authored layout are supplied per pass.
    MeshRange mesh;
    Matrix world;
    Material material;
};
struct LightmapDraw : StaticDraw {
    ID3D11ShaderResourceView* lightmap = nullptr;
};
struct SkinDraw : StaticDraw {
    const Matrix* bones = nullptr;
    UINT bone_count = 0;
};
struct WorldDraw {
    const WorldVertex* vertices = nullptr;
    UINT vertex_count = 0;
    Matrix world;
    Material material;
};
struct IndexedWorldDraw : WorldDraw {
    const uint16_t* indices = nullptr;
    UINT index_count = 0;
    bool texture_only = false;
};
struct WaterDraw : StaticDraw {
    ID3D11ShaderResourceView* reflection = nullptr;
    Matrix reflection_projection;
    float uv_offset[2] = {};
    float distortion = 0.002f;
    float reflectivity = 240.0f / 255.0f;
    ID3D11ShaderResourceView* refraction = nullptr;
    ID3D11ShaderResourceView* normal_second = nullptr;
    Matrix refraction_projection;
    Color water_color = {63.0f / 255, 158.0f / 255, 102.0f / 255, 1};
    float opacity = 114.0f / 255;
    float second_uv_offset[2] = {};
    bool normal_signed = false;
};
struct ShadowDraw : StaticDraw {
    ID3D11ShaderResourceView* shadow = nullptr;
    Matrix projector;
    float opacity = 90.0f / 255.0f;
};
struct BumpReflectDraw : StaticDraw {
    ID3D11ShaderResourceView* normal = nullptr;
    ID3D11ShaderResourceView* environment = nullptr;
    Matrix inverse_view;
    float light_position[4] = {0, 1, 0, 1};
    float uv_offset[2] = {};
};
enum class SecondaryBlend { multiply, replace, add_signed };
struct MultiTextureDraw : StaticDraw {
    ID3D11ShaderResourceView* secondary = nullptr;
    ID3D11ShaderResourceView* detail = nullptr;
    bool use_diffuse_rgb = true;
    SecondaryBlend secondary_blend = SecondaryBlend::multiply;
};
enum class GlowFilter : uint8_t { copy, eight_tap, gaussian_x, gaussian_y };
struct GlowDraw {
    ID3D11ShaderResourceView* source = nullptr;
    ID3D11RenderTargetView* target = nullptr;
    UINT width = 0, height = 0;
    float source_texel_size[2] = {};
    float alpha = 1;
    Blend blend = Blend::opaque;
    GlowFilter filter = GlowFilter::copy;
};

HRESULT passes_initialize(NativeRenderer& renderer);
void passes_shutdown(NativeRenderer& renderer);
void passes_reset_bindings(NativeRenderer& renderer);
HRESULT begin_scene(NativeRenderer& renderer, const Scene& scene);
HRESULT draw_ui(NativeRenderer& renderer, const UiDraw& draw);
HRESULT draw_ui_dual36(NativeRenderer& renderer, const UiDualDraw& draw);
HRESULT draw_static(NativeRenderer& renderer, const StaticDraw& draw);
HRESULT draw_lit(NativeRenderer& renderer, const StaticDraw& draw);
HRESULT draw_model(NativeRenderer& renderer, const StaticDraw& draw);
HRESULT draw_unlit_model(NativeRenderer& renderer, const StaticDraw& draw);
HRESULT draw_reflective_model(NativeRenderer& renderer, const StaticDraw& draw);
HRESULT draw_lightmap(NativeRenderer& renderer, const LightmapDraw& draw);
HRESULT draw_skinned(NativeRenderer& renderer, const SkinDraw& draw);
HRESULT draw_reflective_skinned(NativeRenderer& renderer, const SkinDraw& draw);
HRESULT draw_water(NativeRenderer& renderer, const WaterDraw& draw);
HRESULT draw_water32(NativeRenderer& renderer, const WaterDraw& draw);
HRESULT draw_water56(NativeRenderer& renderer, const WaterDraw& draw);
HRESULT draw_bump_reflect_world24(NativeRenderer& renderer, const BumpReflectDraw& draw);
HRESULT draw_world_triangles(NativeRenderer& renderer, const WorldDraw& draw);
HRESULT draw_projected_world_triangles(NativeRenderer& renderer, const WorldDraw& draw,
                                       ID3D11ShaderResourceView* shadow, const Matrix& projector,
                                       float opacity = 90.0f / 255.0f);
HRESULT draw_world_indexed_triangles(NativeRenderer& renderer, const IndexedWorldDraw& draw);
HRESULT draw_world_indexed(NativeRenderer& renderer, const StaticDraw& draw);
HRESULT draw_textured_world_indexed(NativeRenderer& renderer, const StaticDraw& draw);
HRESULT draw_textured_world_indexed_multi28(NativeRenderer& renderer, const MultiTextureDraw& draw);
HRESULT draw_textured_world_indexed_multi36(NativeRenderer& renderer, const MultiTextureDraw& draw);
HRESULT draw_projected_mesh_shadow(NativeRenderer& renderer, const ShadowDraw& draw);
HRESULT draw_projected_world_shadow(NativeRenderer& renderer, const ShadowDraw& draw);
HRESULT draw_projected_world_effect(NativeRenderer& renderer, const ShadowDraw& draw);
HRESULT draw_projected_model_shadow(NativeRenderer& renderer, const ShadowDraw& draw);
HRESULT draw_glow(NativeRenderer& renderer, const GlowDraw& draw);
HRESULT draw_gamma(NativeRenderer& renderer, ID3D11ShaderResourceView* source,
                   ID3D11RenderTargetView* target, float gamma);
} // namespace shaiya::graphics::native
