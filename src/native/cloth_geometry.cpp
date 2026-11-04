#include "cloth_geometry.h"
#include <stdexcept>
namespace genesis {
std::vector<shaiya::graphics::native::WorldVertex> cloth_vertices(const ClothParticles& state) {
    const auto& particles = state.particles();
    const auto& committed = state.committed_positions();
    std::vector<shaiya::graphics::native::WorldVertex> result(particles.size());
    for (std::size_t i = 0; i < result.size(); ++i) {
        for (unsigned c = 0; c < 3; ++c)
            result[i].position[c] = committed[i][c];
        result[i].argb = 0xffffffff;
        for (unsigned c = 0; c < 2; ++c)
            result[i].uv[c] = particles[i].uv[c];
    }
    return result;
}
std::vector<shaiya::graphics::native::WorldVertex>
cloth_fallback_vertices(const ClothMesh& authored, const ClothParticles& state,
                        std::uint32_t columns) {
    const auto& particles = state.particles();
    auto width = std::size_t(columns) + 1;
    if (authored.vertices.size() != particles.size() || width > particles.size() ||
        particles.size() % width)
        throw std::invalid_argument("Cloth fallback grid does not match mesh");
    std::vector<std::array<float, 3>> differences(width);
    for (std::size_t col = 0; col < width; ++col)
        for (unsigned c = 0; c < 3; ++c)
            differences[col][c] = float(static_cast<long double>(particles[col].position[c]) -
                                        authored.vertices[col].position[c]);
    std::vector<shaiya::graphics::native::WorldVertex> result(particles.size());
    for (std::size_t i = 0; i < result.size(); ++i) {
        for (unsigned c = 0; c < 3; ++c)
            result[i].position[c] =
                float(static_cast<long double>(authored.vertices[i].position[c]) +
                      differences[i % width][c]);
        result[i].argb = 0xffffffff;
        for (unsigned c = 0; c < 2; ++c)
            result[i].uv[c] = authored.vertices[i].uv[c];
    }
    return result;
}
} // namespace genesis
