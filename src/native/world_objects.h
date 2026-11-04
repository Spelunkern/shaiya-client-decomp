#pragma once
#include "archive.h"
#include "model_resource.h"
#include "world_tail.h"
#include "collision.h"
#include <memory>
#include <unordered_map>

namespace genesis {
// The static model placements of a world descriptor: the building, shape,
// tree and grass lists (`004462d0`), whose records are one model index plus
// position A, forward B and up C vectors. `005a7c00` turns them into a matrix
// with the look-at builder `005363d0` (right = up x forward, up' = forward x
// right, rows right/up'/forward/position).
class WorldObjects {
  public:
    struct Placement {
        std::uint8_t list;      // 0 building, 1 shape, 2 tree, 3 grass
        std::uint32_t model;    // index into the list's names
        Matrix world;
    };
    WorldObjects(Renderer& renderer, const shaiya::portable::Archive& archive,
                 const WorldTail& tail);
    // `distance` bounds the draw to placements near `center` (x, z): a native
    // performance limit, not a recovered rule.
    void draw(const std::array<float, 2>& center, float distance, const Matrix& root);
    const std::vector<Placement>& placements() const {
        return placements_;
    }
    std::size_t placement_count() const {
        return placements_.size();
    }
    std::size_t drawn_last() const {
        return drawn_;
    }
    std::size_t loaded_models() const;
    // 005a8490 for every placement: models whose world sphere (centre = the
    // SMOD sphere through the placement matrix) comes within radius + 1 of the
    // sweep's start position contribute their collision triangles, transformed
    // by the placement matrix, to the packet. Placements farther than
    // `broad_phase` units are skipped without decoding their model.
    void collide(CollisionPacket& packet, float broad_phase = 150.f);
    // 005363d0 on a position, forward and up triple.
    static Matrix placement_matrix(const std::array<float, 3>& position,
                                   const std::array<float, 3>& forward,
                                   const std::array<float, 3>& up);

  private:
    Renderer& renderer_;
    const shaiya::portable::Archive& archive_;
    std::array<std::vector<std::string>, 4> paths_;
    std::vector<Placement> placements_;
    std::unordered_map<std::uint32_t, std::unique_ptr<ModelResource>> models_;
    std::unordered_map<std::uint32_t, bool> failed_;
    struct Collider {
        bool loaded = false;
        std::array<float, 4> sphere{};
        ModelCollision mesh;
    };
    std::unordered_map<std::uint32_t, Collider> colliders_;
    std::unordered_map<std::size_t, std::vector<Vec3f>> world_vertices_;
    std::size_t drawn_ = 0;
};
} // namespace genesis
