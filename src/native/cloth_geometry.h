#pragma once
#include "cloth_particles.h"
namespace genesis {
// 00581b30: committed positions, white ARGB and particle UVs, 24-byte vertices.
std::vector<shaiya::graphics::native::WorldVertex> cloth_vertices(const ClothParticles& particles);
// 00581d70 fallback: each authored column follows its current first-row delta.
std::vector<shaiya::graphics::native::WorldVertex>
cloth_fallback_vertices(const ClothMesh& authored, const ClothParticles& particles,
                        std::uint32_t columns);
} // namespace genesis
