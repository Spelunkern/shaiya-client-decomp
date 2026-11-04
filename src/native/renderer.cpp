#include "renderer.h"
#include "render_shaders.h"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <string>

namespace genesis {
namespace {
[[noreturn]] void sdl_error(const char* operation) {
    throw std::runtime_error(std::string(operation) + ": " + SDL_GetError());
}
GLenum blend_factor_gl(BlendFactor factor) {
    switch (factor) {
    case BlendFactor::zero:
        return GL_ZERO;
    case BlendFactor::one:
        return GL_ONE;
    case BlendFactor::src_color:
        return GL_SRC_COLOR;
    case BlendFactor::inv_src_color:
        return GL_ONE_MINUS_SRC_COLOR;
    case BlendFactor::src_alpha:
        return GL_SRC_ALPHA;
    case BlendFactor::inv_src_alpha:
        return GL_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::dest_alpha:
        return GL_DST_ALPHA;
    case BlendFactor::inv_dest_alpha:
        return GL_ONE_MINUS_DST_ALPHA;
    case BlendFactor::dest_color:
        return GL_DST_COLOR;
    case BlendFactor::inv_dest_color:
        return GL_ONE_MINUS_DST_COLOR;
    case BlendFactor::src_alpha_sat:
        return GL_SRC_ALPHA_SATURATE;
    case BlendFactor::blend_factor:
        return GL_CONSTANT_COLOR;
    case BlendFactor::inv_blend_factor:
        return GL_ONE_MINUS_CONSTANT_COLOR;
    }
    throw std::invalid_argument("Invalid blend factor");
}
// native_render_passes.cpp's alpha_factor(): a color-channel factor samples
// its matching alpha-only variant; symmetric factors (ZERO, ONE, the *_ALPHA
// pair, the constant-factor pair) are reused unchanged.
GLenum blend_alpha_factor_gl(BlendFactor factor) {
    switch (factor) {
    case BlendFactor::src_color:
        return GL_SRC_ALPHA;
    case BlendFactor::inv_src_color:
        return GL_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::dest_color:
        return GL_DST_ALPHA;
    case BlendFactor::inv_dest_color:
        return GL_ONE_MINUS_DST_ALPHA;
    case BlendFactor::src_alpha_sat:
        return GL_ONE;
    default:
        return blend_factor_gl(factor);
    }
}
GLuint shader(GL& gl, GLenum type, const char* body) {
    GLuint id = gl.CreateShader(type);
    const char* source[] = {"#version 450 core\n", glsl_uniforms, body};
    gl.ShaderSource(id, 3, source, nullptr);
    gl.CompileShader(id);
    GLint ok = 0;
    gl.GetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char message[8192] = {};
        gl.GetShaderInfoLog(id, sizeof(message), nullptr, message);
        gl.DeleteShader(id);
        throw std::runtime_error(std::string("GLSL compilation: ") + message);
    }
    return id;
}
std::size_t stride(Layout layout) {
    switch (layout) {
    case Layout::ui:
        return sizeof(UiVertex);
    case Layout::ui_dual:
        return sizeof(UiDualVertex36);
    case Layout::mesh:
        return sizeof(MeshVertex);
    case Layout::lightmap:
        return sizeof(LightmapVertex);
    case Layout::model:
        return sizeof(ModelVertex);
    case Layout::skin:
        return sizeof(SkinVertex);
    case Layout::world:
        return sizeof(WorldVertex);
    case Layout::textured_world:
        return sizeof(TexturedWorldVertex);
    case Layout::multi28:
        return sizeof(TexturedWorldMulti28);
    case Layout::multi36:
        return sizeof(TexturedWorldMulti36);
    case Layout::water32:
        return sizeof(WaterVertex32);
    case Layout::water56:
        return sizeof(WaterVertex56);
    }
    throw std::invalid_argument("Unknown vertex layout");
}
} // namespace

Renderer::Renderer(int width, int height, bool hidden) {
    SDL_SetMainReady();
    if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
        sdl_error("SDL initialization");
    try {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        window_ = SDL_CreateWindow("Genesis — native OpenGL 4.5", SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED, width, height,
                                   SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                       (hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN));
        if (!window_)
            sdl_error("Window creation");
        context_ = SDL_GL_CreateContext(window_);
        if (!context_)
            sdl_error("OpenGL context creation");
        gl_.load();
        GLint major = 0, minor = 0, profile = 0;
        gl_.GetIntegerv(GL_MAJOR_VERSION, &major);
        gl_.GetIntegerv(GL_MINOR_VERSION, &minor);
        gl_.GetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
        if ((major < 4 || (major == 4 && minor < 5)) || !(profile & GL_CONTEXT_CORE_PROFILE_BIT))
            throw std::runtime_error("An OpenGL 4.5 core context is required");
        gl_.ClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
        gl_.Enable(GL_CLIP_DISTANCE0);
        gl_.GenVertexArrays(1, &vao_);
        gl_.BindVertexArray(vao_);
        gl_.GenBuffers(1, &vertex_buffer_);
        gl_.GenBuffers(1, &index_buffer_);
        gl_.GenBuffers(1, &scene_buffer_);
        gl_.GenBuffers(1, &object_buffer_);
        gl_.GenBuffers(1, &skin_buffer_);
        gl_.GenBuffers(1, &water_buffer_);
        gl_.GenSamplers(static_cast<GLsizei>(samplers_.size()), samplers_.data());
        for (unsigned i = 0; i < samplers_.size(); ++i) {
            GLint filter = i == 0 ? GL_NEAREST : GL_LINEAR;
            GLint wrap = i < 2 ? GL_CLAMP_TO_EDGE : (i == 4 ? GL_MIRRORED_REPEAT : GL_REPEAT);
            gl_.SamplerParameteri(samplers_[i], GL_TEXTURE_MIN_FILTER, filter);
            gl_.SamplerParameteri(samplers_[i], GL_TEXTURE_MAG_FILTER, filter);
            gl_.SamplerParameteri(samplers_[i], GL_TEXTURE_WRAP_S, wrap);
            gl_.SamplerParameteri(samplers_[i], GL_TEXTURE_WRAP_T, wrap);
        }
        if (SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic") ||
            SDL_GL_ExtensionSupported("GL_ARB_texture_filter_anisotropic")) {
            GLfloat maximum = 1;
            gl_.GetFloatv(0x84ff, &maximum);
            gl_.SamplerParameterf(samplers_[3], 0x84fe, std::min(16.f, maximum));
        }
        gl_.GenFramebuffers(1, &target_);
        gl_.GenTextures(1, &color_);
        gl_.GenTextures(1, &depth_);
        resize(width, height);
        const unsigned char white[] = {255, 255, 255, 255};
        white_ = texture_rgba(1, 1, white);
        object({});
        water({});
        bones(nullptr, 0);
        bind_images({});
        SDL_GL_SetSwapInterval(hidden ? 0 : 1);
        check("Renderer initialization");
    } catch (...) {
        shutdown();
        throw;
    }
}
Renderer::~Renderer() {
    shutdown();
}
void Renderer::shutdown() {
    if (context_) {
        SDL_GL_MakeCurrent(window_, context_);
        for (auto& row : programs_)
            for (auto id : row)
                if (id)
                    gl_.DeleteProgram(id);
        for (auto id : textures_)
            gl_.DeleteTextures(1, &id);
        for (auto id : samplers_)
            if (id)
                gl_.DeleteSamplers(1, &id);
        for (auto id : {color_, depth_})
            if (id)
                gl_.DeleteTextures(1, &id);
        for (auto id : {vertex_buffer_, index_buffer_, scene_buffer_, object_buffer_, skin_buffer_,
                        water_buffer_})
            if (id)
                gl_.DeleteBuffers(1, &id);
        if (vao_)
            gl_.DeleteVertexArrays(1, &vao_);
        if (target_)
            gl_.DeleteFramebuffers(1, &target_);
        for (auto& target : targets_) {
            gl_.DeleteTextures(1, &target.color);
            gl_.DeleteTextures(1, &target.depth);
            gl_.DeleteFramebuffers(1, &target.framebuffer);
        }
        for (auto& mesh : meshes_) {
            gl_.DeleteBuffers(1, &mesh.vertices);
            gl_.DeleteBuffers(1, &mesh.indices);
            gl_.DeleteVertexArrays(1, &mesh.vao);
        }
        SDL_GL_DeleteContext(context_);
        context_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_TIMER);
}
const char* Renderer::version() const {
    return reinterpret_cast<const char*>(gl_.GetString(GL_VERSION));
}
void Renderer::check(const char* operation) {
    GLenum error = gl_.GetError();
    if (error != GL_NO_ERROR)
        throw std::runtime_error(std::string(operation) + ": OpenGL error " +
                                 std::to_string(error));
}
void Renderer::resize(int width, int height) {
    if (width <= 0 || height <= 0)
        return; // minimized window
    main_target_ = {target_, color_, depth_, main_width_, main_height_};
    allocate_target(main_target_, width, height);
    main_width_ = width;
    main_height_ = height;
    bind_target(current_target_);
    check("Resize");
}
void Renderer::allocate_target(Target& target, int width, int height) {
    GLint maximum = 0;
    gl_.GetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    if (width <= 0 || height <= 0 || width > maximum || height > maximum)
        throw std::invalid_argument("Invalid render target dimensions");
    target.width = width;
    target.height = height;
    gl_.BindTexture(GL_TEXTURE_2D, target.color);
    gl_.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                   nullptr);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_.BindTexture(GL_TEXTURE_2D, target.depth);
    gl_.TexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL,
                   GL_UNSIGNED_INT_24_8, nullptr);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl_.BindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    gl_.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.color, 0);
    gl_.FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                             target.depth, 0);
    if (gl_.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Incomplete OpenGL render target");
    check("Render target allocation");
}
Renderer::TargetId Renderer::create_target(int width, int height) {
    targets_.push_back({});
    auto& value = targets_.back();
    gl_.GenFramebuffers(1, &value.framebuffer);
    gl_.GenTextures(1, &value.color);
    gl_.GenTextures(1, &value.depth);
    try {
        allocate_target(value, width, height);
    } catch (...) {
        gl_.DeleteFramebuffers(1, &value.framebuffer);
        gl_.DeleteTextures(1, &value.color);
        gl_.DeleteTextures(1, &value.depth);
        targets_.pop_back();
        bind_target(current_target_);
        throw;
    }
    bind_target(current_target_);
    return targets_.size();
}
Renderer::Target Renderer::target(TargetId id) const {
    if (id > targets_.size())
        throw std::invalid_argument("Invalid render target handle");
    return id ? targets_[id - 1] : main_target_;
}
void Renderer::bind_target(TargetId id) {
    const auto& value = target(id);
    current_target_ = id;
    width_ = value.width;
    height_ = value.height;
    gl_.BindFramebuffer(GL_FRAMEBUFFER, value.framebuffer);
    gl_.Viewport(0, 0, width_, height_);
    reset_scissor();
    scene(scene_);
}
void Renderer::clear(Color color) {
    gl_.BindFramebuffer(GL_FRAMEBUFFER, target(current_target_).framebuffer);
    gl_.Disable(GL_SCISSOR_TEST);
    gl_.DepthMask(GL_TRUE);
    gl_.ClearColor(color.r, color.g, color.b, color.a);
    gl_.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}
void Renderer::present() {
    int width = 0, height = 0;
    SDL_GL_GetDrawableSize(window_, &width, &height);
    if (width <= 0 || height <= 0)
        return;
    gl_.Disable(GL_SCISSOR_TEST);
    gl_.BindFramebuffer(GL_READ_FRAMEBUFFER, target_);
    gl_.BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    // Our FBO uses top-left texture coordinates. Only presentation flips Y.
    gl_.BlitFramebuffer(0, 0, main_width_, main_height_, 0, height, width, 0, GL_COLOR_BUFFER_BIT,
                        GL_LINEAR);
    SDL_GL_SwapWindow(window_);
    gl_.BindFramebuffer(GL_FRAMEBUFFER, target(current_target_).framebuffer);
}
void Renderer::uniform(GLuint buffer, GLuint binding, const void* data, std::size_t size) {
    gl_.BindBuffer(GL_UNIFORM_BUFFER, buffer);
    gl_.BufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(size), data, GL_DYNAMIC_DRAW);
    gl_.BindBufferBase(GL_UNIFORM_BUFFER, binding, buffer);
}
void Renderer::scene(const SceneUniforms& data) {
    scene_ = data;
    scene_.viewport = {float(width_), float(height_), 0, 0};
    uniform(scene_buffer_, 0, &scene_, sizeof(scene_));
}
void Renderer::object(const ObjectUniforms& data) {
    uniform(object_buffer_, 1, &data, sizeof(data));
}
void Renderer::water(const WaterUniforms& data) {
    uniform(water_buffer_, 3, &data, sizeof(data));
}
void Renderer::bones(const Matrix* matrices, std::size_t count) {
    if (count > 256 || (count && !matrices))
        throw std::invalid_argument("Invalid bone palette");
    std::array<Matrix, 256> palette;
    palette.fill(identity);
    if (count)
        std::copy_n(matrices, count, palette.begin());
    uniform(skin_buffer_, 2, palette.data(), sizeof(palette));
}
GLuint Renderer::texture_rgba(int width, int height, const void* pixels, bool mipmaps) {
    GLint maximum = 0;
    gl_.GetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    if (width <= 0 || height <= 0 || width > maximum || height > maximum || !pixels)
        throw std::invalid_argument("Invalid texture dimensions or pixels");
    GLuint id = 0;
    gl_.GenTextures(1, &id);
    textures_.push_back(id);
    gl_.BindTexture(GL_TEXTURE_2D, id);
    gl_.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl_.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                      mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (mipmaps)
        gl_.GenerateMipmap(GL_TEXTURE_2D);
    check("Texture upload");
    return id;
}
GLuint Renderer::texture_levels(const std::vector<Image>& levels) {
    GLint maximum = 0;
    gl_.GetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    if (levels.empty() || levels.size() > 15)
        throw std::invalid_argument("Invalid texture level count");
    std::uint32_t width = levels[0].width, height = levels[0].height;
    std::size_t complete = 1;
    for (auto size = std::max(width, height); size > 1; size /= 2)
        ++complete;
    if (levels.size() > complete)
        throw std::invalid_argument("More texture levels than a complete chain");
    for (const auto& level : levels) {
        if (level.width != width || level.height != height || !width || !height ||
            width > std::uint32_t(maximum) || height > std::uint32_t(maximum) ||
            level.pixels.size() != std::size_t(width) * height * 4)
            throw std::invalid_argument("Invalid texture level");
        width = width > 1 ? width / 2 : 1;
        height = height > 1 ? height / 2 : 1;
    }
    GLuint id = 0;
    gl_.GenTextures(1, &id);
    textures_.push_back(id);
    gl_.BindTexture(GL_TEXTURE_2D, id);
    gl_.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (std::size_t i = 0; i < levels.size(); ++i)
        gl_.TexImage2D(GL_TEXTURE_2D, GLint(i), GL_RGBA8, GLsizei(levels[i].width),
                       GLsizei(levels[i].height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                       levels[i].pixels.data());
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, GLint(levels.size() - 1));
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                      levels.size() > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    check("Texture level upload");
    return id;
}
GLuint Renderer::texture_rg_snorm(int width, int height, const std::int8_t* pixels) {
    GLint maximum = 0;
    gl_.GetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    if (width <= 0 || height <= 0 || width > maximum || height > maximum || !pixels)
        throw std::invalid_argument("Invalid snorm texture");
    GLuint id = 0;
    gl_.GenTextures(1, &id);
    textures_.push_back(id);
    gl_.BindTexture(GL_TEXTURE_2D, id);
    gl_.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl_.TexImage2D(GL_TEXTURE_2D, 0, GL_RG8_SNORM, width, height, 0, GL_RG, GL_BYTE, pixels);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    check("Snorm texture upload");
    return id;
}
GLuint Renderer::load_texture(const std::vector<std::uint8_t>& encoded,
                              const TextureLoadOptions& options) {
    return texture_levels(decode_texture_levels(encoded, options));
}
void Renderer::bind_images(std::array<GLuint, 4> images) {
    images_ = images;
}
void Renderer::update_texture(GLuint texture, int x, int y, int width, int height,
                              const void* pixels) {
    if (std::find(textures_.begin(), textures_.end(), texture) == textures_.end() ||
        texture == white_)
        throw std::invalid_argument("Texture is not an updatable resource");
    if (x < 0 || y < 0 || width <= 0 || height <= 0 || !pixels)
        throw std::invalid_argument("Invalid texture update");
    gl_.BindTexture(GL_TEXTURE_2D, texture);
    GLint w = 0, h = 0;
    gl_.GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
    gl_.GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
    if (x > w || y > h || width > w - x || height > h - y)
        throw std::invalid_argument("Texture update exceeds bounds");
    gl_.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl_.TexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    check("Texture update");
}
void Renderer::release_texture(GLuint texture) {
    if (!texture)
        return;
    auto it = std::find(textures_.begin(), textures_.end(), texture);
    if (it == textures_.end() || texture == white_)
        throw std::invalid_argument("Texture is not a releasable resource");
    for (auto& image : images_)
        if (image == texture)
            image = 0;
    gl_.DeleteTextures(1, &texture);
    textures_.erase(it);
}
GLuint Renderer::program(VertexPass vertex, PixelPass pixel) {
    auto v = static_cast<std::size_t>(vertex), p = static_cast<std::size_t>(pixel);
    if (v >= programs_.size() || p >= programs_[0].size())
        throw std::invalid_argument("Invalid shader pass");
    GLuint& id = programs_[v][p];
    if (!id) {
        GLuint vs = shader(gl_, GL_VERTEX_SHADER, glsl_vertex), fs = 0;
        try {
            fs = shader(gl_, GL_FRAGMENT_SHADER, glsl_fragment);
        } catch (...) {
            gl_.DeleteShader(vs);
            throw;
        }
        id = gl_.CreateProgram();
        gl_.AttachShader(id, vs);
        gl_.AttachShader(id, fs);
        gl_.LinkProgram(id);
        gl_.DeleteShader(vs);
        gl_.DeleteShader(fs);
        GLint ok = 0;
        gl_.GetProgramiv(id, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[8192] = {};
            gl_.GetProgramInfoLog(id, sizeof(log), nullptr, log);
            gl_.DeleteProgram(id);
            id = 0;
            throw std::runtime_error(std::string("GLSL link: ") + log);
        }
    }
    gl_.UseProgram(id);
    gl_.Uniform1i(gl_.GetUniformLocation(id, "vertexPass"), static_cast<GLint>(v));
    gl_.Uniform1i(gl_.GetUniformLocation(id, "pixelPass"), static_cast<GLint>(p));
    return id;
}
void Renderer::apply(const Style& style, PixelPass pixel) {
    auto sampling = static_cast<std::size_t>(style.sampling);
    if (sampling >= samplers_.size())
        throw std::invalid_argument("Invalid sampling mode");
    if (style.depth == Depth::disabled)
        gl_.Disable(GL_DEPTH_TEST);
    else
        gl_.Enable(GL_DEPTH_TEST);
    gl_.DepthFunc(GL_LEQUAL);
    gl_.DepthMask(style.depth == Depth::write ? GL_TRUE : GL_FALSE);
    if (style.cull == Cull::none)
        gl_.Disable(GL_CULL_FACE);
    else {
        gl_.Enable(GL_CULL_FACE);
        gl_.CullFace(style.cull == Cull::back ? GL_BACK : GL_FRONT);
    }
    gl_.FrontFace(GL_CW); // D3D11's default clockwise front face
    if (style.custom_blend) {
        gl_.Enable(GL_BLEND);
        gl_.BlendEquation(GL_FUNC_ADD);
        gl_.BlendColor(style.blend_factor.r, style.blend_factor.g, style.blend_factor.b,
                       style.blend_factor.a);
        auto color_factor = blend_factor_gl(style.custom_blend->source);
        auto color_factor_dst = blend_factor_gl(style.custom_blend->destination);
        gl_.BlendFuncSeparate(color_factor, color_factor_dst,
                              blend_alpha_factor_gl(style.custom_blend->source),
                              blend_alpha_factor_gl(style.custom_blend->destination));
    } else if (style.blend == Blend::opaque)
        gl_.Disable(GL_BLEND);
    else {
        gl_.Enable(GL_BLEND);
        gl_.BlendEquation(GL_FUNC_ADD);
        GLenum src = GL_SRC_ALPHA, dst = GL_ONE_MINUS_SRC_ALPHA;
        switch (style.blend) {
        case Blend::additive:
            dst = GL_ONE;
            break;
        case Blend::color_add:
            src = GL_ONE;
            dst = GL_ONE;
            break;
        case Blend::multiply:
            src = GL_DST_COLOR;
            dst = GL_ZERO;
            break;
        case Blend::inverse_alpha:
            src = GL_ONE;
            break;
        default:
            break;
        }
        gl_.BlendFuncSeparate(src, dst, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    for (unsigned i = 0; i < images_.size(); ++i) {
        if (images_[i] && (images_[i] == target(current_target_).color ||
                           images_[i] == target(current_target_).depth))
            throw std::invalid_argument("Cannot sample the active render target");
        gl_.ActiveTexture(GL_TEXTURE0 + i);
        gl_.BindTexture(GL_TEXTURE_2D, images_[i] ? images_[i] : white_);
        // A sampler belongs to a texture unit, not to the texture. The same
        // resource may be sampled with wrapping and clamping in a single pass.
        auto sampler = samplers_[sampling];
        bool layers = pixel == PixelPass::multi28 || pixel == PixelPass::multi36 ||
                      pixel == PixelPass::replace28 || pixel == PixelPass::replace36 ||
                      pixel == PixelPass::signed28 || pixel == PixelPass::signed36;
        if (i == 1 && layers)
            sampler = samplers_[2];
        if ((i == 1 && (pixel == PixelPass::lightmap || pixel == PixelPass::water ||
                        pixel == PixelPass::water56)) ||
            (i == 2 && pixel == PixelPass::water56) ||
            (i == 0 &&
             (pixel == PixelPass::projected_shadow || pixel == PixelPass::projected_effect)))
            sampler = samplers_[1];
        gl_.BindSampler(i, sampler);
    }
    gl_.ActiveTexture(GL_TEXTURE0);
}
void Renderer::draw(Layout layout, VertexPass vertex, PixelPass pixel, const void* vertices,
                    std::size_t count, const Style& style, const std::uint16_t* indices,
                    std::size_t index_count) {
    if (!vertices || !count || count > INT_MAX || index_count > INT_MAX ||
        (index_count && !indices))
        throw std::invalid_argument("Invalid draw geometry");
    for (std::size_t i = 0; i < index_count; ++i)
        if (indices[i] >= count)
            throw std::invalid_argument("Mesh index out of range");
    program(vertex, pixel);
    apply(style, pixel);
    gl_.BindVertexArray(vao_);
    gl_.BindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    auto size = stride(layout);
    gl_.BufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(count * size), vertices,
                   GL_STREAM_DRAW);
    configure_layout(layout);
    if (index_count) {
        gl_.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
        gl_.BufferData(GL_ELEMENT_ARRAY_BUFFER,
                       static_cast<GLsizeiptr>(index_count * sizeof(std::uint16_t)), indices,
                       GL_STREAM_DRAW);
        gl_.DrawElementsBaseVertex(GL_TRIANGLES, static_cast<GLsizei>(index_count),
                                   GL_UNSIGNED_SHORT, nullptr, 0);
    } else
        gl_.DrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(count));
    check("Draw");
}
void Renderer::configure_layout(Layout layout) {
    auto size = stride(layout);
    for (unsigned i = 0; i < 8; ++i)
        gl_.DisableVertexAttribArray(i);
    auto attr = [&](GLuint index, GLint components, std::size_t offset, GLenum type = GL_FLOAT,
                    bool integer = false) {
        gl_.EnableVertexAttribArray(index);
        auto pointer = reinterpret_cast<const void*>(offset);
        if (integer)
            gl_.VertexAttribIPointer(index, components, type, static_cast<GLsizei>(size), pointer);
        else
            gl_.VertexAttribPointer(index, components, type,
                                    type == GL_UNSIGNED_BYTE ? GL_TRUE : GL_FALSE,
                                    static_cast<GLsizei>(size), pointer);
    };
    attr(0, layout == Layout::ui || layout == Layout::ui_dual ? 4 : 3, 0);
    switch (layout) {
    case Layout::ui:
    case Layout::ui_dual:
        attr(2, 4, 16, GL_UNSIGNED_BYTE);
        attr(3, 2, 20);
        if (layout == Layout::ui_dual)
            attr(4, 2, 28);
        break;
    case Layout::mesh:
    case Layout::lightmap:
        attr(1, 3, 12);
        attr(2, 4, 24, GL_UNSIGNED_BYTE);
        attr(3, 2, 28);
        if (layout == Layout::lightmap)
            attr(4, 2, 36);
        break;
    case Layout::model:
        attr(1, 3, 12);
        attr(3, 2, 24);
        break;
    case Layout::skin:
        attr(6, 1, 12);
        attr(7, 4, 16, GL_UNSIGNED_BYTE, true);
        attr(1, 3, 20);
        attr(3, 2, 32);
        break;
    case Layout::world:
    case Layout::water32:
        attr(2, 4, 12, GL_UNSIGNED_BYTE);
        attr(3, 2, 16);
        if (layout == Layout::water32)
            attr(4, 2, 24);
        break;
    case Layout::textured_world:
    case Layout::multi28:
    case Layout::multi36:
        attr(3, 2, 12);
        if (layout != Layout::textured_world)
            attr(4, 2, 20);
        if (layout == Layout::multi36)
            attr(5, 2, 28);
        break;
    case Layout::water56:
        attr(1, 3, 12);
        attr(3, 2, 24);
        attr(4, 2, 32);
        break;
    }
}
Renderer::MeshId Renderer::create_mesh(Layout layout, const void* vertices, std::size_t count,
                                       const void* indices, std::size_t index_count,
                                       IndexType type) {
    if (!vertices || !count || count > INT_MAX || index_count > INT_MAX ||
        (index_count && !indices))
        throw std::invalid_argument("Invalid persistent mesh geometry");
    if (type != IndexType::u16 && type != IndexType::u32)
        throw std::invalid_argument("Invalid mesh index type");
    Mesh mesh;
    mesh.layout = layout;
    mesh.vertex_count = count;
    mesh.index_type = type;
    const auto* bytes = static_cast<const std::uint8_t*>(vertices);
    mesh.asset_vertices.assign(bytes, bytes + count * stride(layout));
    mesh.asset_indices.resize(index_count);
    for (std::size_t i = 0; i < index_count; ++i) {
        auto index = type == IndexType::u16 ? static_cast<const std::uint16_t*>(indices)[i]
                                            : static_cast<const std::uint32_t*>(indices)[i];
        if (index >= count)
            throw std::invalid_argument("Mesh index out of range");
        mesh.asset_indices[i] = index;
    }
    meshes_.push_back(std::move(mesh));
    auto& value = meshes_.back();
    gl_.GenVertexArrays(1, &value.vao);
    gl_.BindVertexArray(value.vao);
    gl_.GenBuffers(1, &value.vertices);
    gl_.BindBuffer(GL_ARRAY_BUFFER, value.vertices);
    gl_.BufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(value.asset_vertices.size()), vertices,
                   GL_STATIC_DRAW);
    configure_layout(layout);
    if (index_count) {
        gl_.GenBuffers(1, &value.indices);
        gl_.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, value.indices);
        gl_.BufferData(GL_ELEMENT_ARRAY_BUFFER,
                       static_cast<GLsizeiptr>(index_count * (type == IndexType::u16 ? 2 : 4)),
                       indices, GL_STATIC_DRAW);
    }
    check("Persistent mesh upload");
    return meshes_.size();
}
void Renderer::draw_mesh(MeshId id, VertexPass vertex, PixelPass pixel, const Style& style,
                         std::size_t first_index, std::size_t index_count, int base_vertex) {
    if (!id || id > meshes_.size() || !meshes_[id-1].vao)
        throw std::invalid_argument("Invalid mesh handle");
    const auto& mesh = meshes_[id - 1];
    if (mesh.asset_indices.empty()) {
        if (first_index || index_count || base_vertex)
            throw std::invalid_argument("Indexed range on non-indexed mesh");
        program(vertex, pixel);
        apply(style, pixel);
        gl_.BindVertexArray(mesh.vao);
        gl_.DrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.vertex_count));
    } else {
        if (first_index > mesh.asset_indices.size())
            throw std::invalid_argument("Invalid first mesh index");
        if (!index_count)
            index_count = mesh.asset_indices.size() - first_index;
        if (index_count > mesh.asset_indices.size() - first_index)
            throw std::invalid_argument("Invalid mesh index range");
        // CPU bytes remain available for collision and exact range validation.
        if (base_vertex)
            for (std::size_t i = first_index; i < first_index + index_count; ++i) {
                auto index = std::int64_t(mesh.asset_indices[i]) + base_vertex;
                if (index < 0 || std::uint64_t(index) >= mesh.vertex_count)
                    throw std::invalid_argument("Mesh base vertex exceeds buffer bounds");
            }
        program(vertex, pixel);
        apply(style, pixel);
        gl_.BindVertexArray(mesh.vao);
        auto offset = first_index * (mesh.index_type == IndexType::u16 ? 2 : 4);
        gl_.DrawElementsBaseVertex(GL_TRIANGLES, static_cast<GLsizei>(index_count),
                                   mesh.index_type == IndexType::u16 ? GL_UNSIGNED_SHORT
                                                                     : GL_UNSIGNED_INT,
                                   reinterpret_cast<const void*>(offset), base_vertex);
    }
    check("Persistent mesh draw");
}
void Renderer::release_mesh(MeshId id) {
    if(!id) return;
    if(id>meshes_.size() || !meshes_[id-1].vao) throw std::invalid_argument("Invalid mesh handle");
    auto& mesh=meshes_[id-1];gl_.DeleteBuffers(1,&mesh.vertices);gl_.DeleteBuffers(1,&mesh.indices);
    gl_.DeleteVertexArrays(1,&mesh.vao);mesh=Mesh{};
}
Renderer::ResourceCounts Renderer::resource_counts() const {
    ResourceCounts counts;
    counts.textures = textures_.size() - 1; // white_
    for (const auto& mesh : meshes_)
        counts.meshes += mesh.vao != 0;
    counts.targets = targets_.size();
    return counts;
}
void Renderer::fullscreen(PixelPass pixel, const Style& style) {
    program(VertexPass::fullscreen, pixel);
    apply(style, pixel);
    gl_.BindVertexArray(vao_);
    gl_.DrawArrays(GL_TRIANGLES, 0, 3);
    check("Fullscreen draw");
}
void Renderer::scissor(int x, int y, int width, int height) {
    if (width < 0 || height < 0)
        throw std::invalid_argument("Invalid scissor");
    gl_.Enable(GL_SCISSOR_TEST);
    gl_.Scissor(x, y, width, height);
}
void Renderer::reset_scissor() {
    gl_.Disable(GL_SCISSOR_TEST);
}
std::vector<std::uint8_t> Renderer::read_pixels() {
    std::vector<std::uint8_t> pixels(std::size_t(width_) * height_ * 4);
    gl_.BindFramebuffer(GL_READ_FRAMEBUFFER, target(current_target_).framebuffer);
    gl_.PixelStorei(GL_PACK_ALIGNMENT, 1);
    gl_.ReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    check("Framebuffer readback");
    return pixels;
}
} // namespace genesis
