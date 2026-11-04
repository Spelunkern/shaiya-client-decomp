#pragma once
#include "archive.h"
#include "model.h"
#include "renderer.h"
namespace genesis {
// GPU resources have native ownership and may be unloaded between scenes.
class ModelResource {
public:
    ModelResource(Renderer& renderer,const shaiya::portable::Archive& archive,std::string_view path);
    ~ModelResource();
    ModelResource(const ModelResource&)=delete;
    ModelResource& operator=(const ModelResource&)=delete;
    Matrix fitted_world() const;
    void draw(const Matrix& world,const Style& style={Blend::alpha,Depth::write,Cull::none,Sampling::anisotropic_wrap});
private:
    void clear();
    Renderer& renderer_;
    StaticModel model_;
    std::vector<Renderer::MeshId> meshes_;
    std::vector<GLuint> images_,owned_images_;
};
}
