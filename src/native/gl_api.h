#pragma once
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#include <stdexcept>
#include <string>

// Keep entry points with the context: WGL pointers are context-specific.
#define GENESIS_GL_FUNCTIONS(X)                                                                    \
    X(const GLubyte*, GetString, (GLenum))                                                         \
    X(void, GetIntegerv, (GLenum, GLint*))                                                         \
    X(GLenum, GetError, ())                                                                        \
    X(void, Enable, (GLenum))                                                                      \
    X(void, Disable, (GLenum))                                                                     \
    X(void, Viewport, (GLint, GLint, GLsizei, GLsizei))                                            \
    X(void, Scissor, (GLint, GLint, GLsizei, GLsizei))                                             \
    X(void, ClearColor, (GLfloat, GLfloat, GLfloat, GLfloat))                                      \
    X(void, Clear, (GLbitfield))                                                                   \
    X(void, DepthMask, (GLboolean))                                                                \
    X(void, DepthFunc, (GLenum))                                                                   \
    X(void, CullFace, (GLenum))                                                                    \
    X(void, FrontFace, (GLenum))                                                                   \
    X(void, BlendFuncSeparate, (GLenum, GLenum, GLenum, GLenum))                                   \
    X(void, BlendColor, (GLfloat, GLfloat, GLfloat, GLfloat))                                      \
    X(void, BlendEquation, (GLenum))                                                               \
    X(void, ClipControl, (GLenum, GLenum))                                                         \
    X(void, PixelStorei, (GLenum, GLint))                                                          \
    X(void, ReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*))                   \
    X(void, GenTextures, (GLsizei, GLuint*))                                                       \
    X(void, DeleteTextures, (GLsizei, const GLuint*))                                              \
    X(void, BindTexture, (GLenum, GLuint))                                                         \
    X(void, ActiveTexture, (GLenum))                                                               \
    X(void, TexParameteri, (GLenum, GLenum, GLint))                                                \
    X(void, TexImage2D,                                                                            \
      (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*))                \
    X(void, TexSubImage2D,                                                                         \
      (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*))                \
    X(void, GetTexLevelParameteriv, (GLenum, GLint, GLenum, GLint*))                               \
    X(void, GetTexParameteriv, (GLenum, GLenum, GLint*))                                           \
    X(void, GetTexImage, (GLenum, GLint, GLenum, GLenum, void*))                                   \
    X(void, GenerateMipmap, (GLenum))                                                              \
    X(void, GenSamplers, (GLsizei, GLuint*))                                                       \
    X(void, DeleteSamplers, (GLsizei, const GLuint*))                                              \
    X(void, BindSampler, (GLuint, GLuint))                                                         \
    X(void, SamplerParameteri, (GLuint, GLenum, GLint))                                            \
    X(void, SamplerParameterf, (GLuint, GLenum, GLfloat))                                          \
    X(void, GetFloatv, (GLenum, GLfloat*))                                                         \
    X(void, GenBuffers, (GLsizei, GLuint*))                                                        \
    X(void, DeleteBuffers, (GLsizei, const GLuint*))                                               \
    X(void, BindBuffer, (GLenum, GLuint))                                                          \
    X(void, BufferData, (GLenum, GLsizeiptr, const void*, GLenum))                                 \
    X(void, BindBufferBase, (GLenum, GLuint, GLuint))                                              \
    X(void, GenVertexArrays, (GLsizei, GLuint*))                                                   \
    X(void, DeleteVertexArrays, (GLsizei, const GLuint*))                                          \
    X(void, BindVertexArray, (GLuint))                                                             \
    X(void, EnableVertexAttribArray, (GLuint))                                                     \
    X(void, DisableVertexAttribArray, (GLuint))                                                    \
    X(void, VertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*))         \
    X(void, VertexAttribIPointer, (GLuint, GLint, GLenum, GLsizei, const void*))                   \
    X(GLuint, CreateShader, (GLenum))                                                              \
    X(void, ShaderSource, (GLuint, GLsizei, const GLchar* const*, const GLint*))                   \
    X(void, CompileShader, (GLuint))                                                               \
    X(void, GetShaderiv, (GLuint, GLenum, GLint*))                                                 \
    X(void, GetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*))                                \
    X(void, DeleteShader, (GLuint))                                                                \
    X(GLuint, CreateProgram, ())                                                                   \
    X(void, AttachShader, (GLuint, GLuint))                                                        \
    X(void, LinkProgram, (GLuint))                                                                 \
    X(void, GetProgramiv, (GLuint, GLenum, GLint*))                                                \
    X(void, GetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*))                               \
    X(void, DeleteProgram, (GLuint))                                                               \
    X(void, UseProgram, (GLuint))                                                                  \
    X(GLint, GetUniformLocation, (GLuint, const GLchar*))                                          \
    X(void, Uniform1i, (GLint, GLint))                                                             \
    X(void, DrawArrays, (GLenum, GLint, GLsizei))                                                  \
    X(void, DrawElementsBaseVertex, (GLenum, GLsizei, GLenum, const void*, GLint))                 \
    X(void, GenFramebuffers, (GLsizei, GLuint*))                                                   \
    X(void, DeleteFramebuffers, (GLsizei, const GLuint*))                                          \
    X(void, BindFramebuffer, (GLenum, GLuint))                                                     \
    X(void, FramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint))                         \
    X(GLenum, CheckFramebufferStatus, (GLenum))                                                    \
    X(void, BlitFramebuffer,                                                                       \
      (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum))

namespace genesis {
struct GL {
#define DECLARE_GL(result, name, args) result(APIENTRY* name) args = nullptr;
    GENESIS_GL_FUNCTIONS(DECLARE_GL)
#undef DECLARE_GL
    void load() {
#define LOAD_GL(result, name, args)                                                                \
    name = reinterpret_cast<decltype(name)>(SDL_GL_GetProcAddress("gl" #name));                    \
    if (!name)                                                                                     \
        throw std::runtime_error("Missing OpenGL entry point: gl" #name);
        GENESIS_GL_FUNCTIONS(LOAD_GL)
#undef LOAD_GL
    }
};
} // namespace genesis
