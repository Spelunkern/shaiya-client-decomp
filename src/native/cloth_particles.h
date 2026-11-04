#pragma once
#include "cloth.h"
#include "cloth_body.h"
#include <cstddef>
namespace genesis {
struct ClothParticle {
    float mass = 0, inverse_mass = 0;
    std::array<float, 3> position{}, velocity{}, acceleration{}, force{}, initial{};
    std::array<float, 2> uv{};
    bool pinned = false;
};
struct ClothContact {
    std::ptrdiff_t vertex = -1; // Inactive slot, like the original row=-1 sentinel.
    std::array<float, 3> normal{};
};
struct ClothCollisionRecord {
    std::ptrdiff_t vertex = -1;
    std::array<float, 3> normal_or_center{};
    float radius = -1; // -1: near-contact normal; otherwise penetration sphere.
};
enum class ClothCorrection { scaled, unscaled };
struct ClothDetection {
    int status = 0; // Original return: 0 clear, 1 near contact, -1 penetration.
    std::vector<ClothCollisionRecord> records;
};
struct ClothSpring {
    std::size_t first = 0, second = 0;
    float stiffness = 0, damping = 0, rest_length = 0;
};
// Portable particle owner with environmental forces and the 00581770 integrator.
// Actor attachments and rendering integration are remaining consumers.
struct ClothScene {
    ClothBody body{};
    float floor = -100, wind_strength = 0;
    std::array<float, 3> wind_direction{}, correction_direction{};
};
// 0041626d..004162b6: actor direction also drives wind. The caller provides
// terrain height and the original 15-bit random sample, without global RNG.
ClothScene cloth_scene(std::uint8_t kind,
                       const std::vector<shaiya::graphics::native::Matrix>& skeleton_finals,
                       const shaiya::graphics::native::Matrix& world, float floor,
                       std::uint32_t random_sample);
struct ClothTickStats {
    std::uint32_t substeps = 0, attempts = 0, resets = 0;
};
class ClothParticles {
  public:
    ClothParticles(const ClothSetup& setup, const ClothMesh& mesh);
    const std::vector<ClothParticle>& particles() const {
        return particles_;
    }
    const std::vector<ClothSpring>& springs() const {
        return springs_;
    }
    // 00582d90 spring stage, after external_forces. Retain the original
    // length comparison: zero-length springs propagate IEEE NaNs with success.
    bool spring_forces();
    // 00582d90 environmental stage, before spring forces: clears prior force,
    // then applies gravity, quadratic drag and the scene wind vector*strength.
    void external_forces(std::array<float, 3> wind_direction, float wind_strength);
    void forces(const std::vector<std::array<float, 3>>& values);
    void pin(const std::vector<ClothPin>& pins);
    void integrate(float seconds);
    // 00583370 traversal: mutates only floor-clamped positions. Phase 1/2
    // skips traversal; shared near/penetration record overwrite order is kept.
    ClothDetection detect_contacts(const ClothBody& body, float floor, std::uint32_t phase = 0);
    // 00581890: process contacts in order, including repeated/pinned vertices.
    // Normals come from detection; do not renormalize or gate on velocity sign.
    void respond_contacts(const std::vector<ClothContact>& contacts);
    // 005845b0 / 00584853: process the penetration prefix up to radius -1.
    // Preserve global direction, absolute gap and the two float-store policies.
    void correct_penetrations(const std::vector<ClothCollisionRecord>& records,
                              std::array<float, 3> direction, ClothCorrection correction);
    // 005843e0 / 005849a0: one solver step for the mesh's mode.
    bool step(float seconds, const ClothScene& scene, std::uint32_t phase = 0);
    // 00584ba0: frame phase, fixed substeps, retries and reset policy.
    ClothTickStats advance(float seconds, std::uint32_t phase,
                           std::array<float, 3> reset_translation, const ClothScene& scene);
    const std::vector<std::array<float, 3>>& committed_positions() const {
        return committed_;
    }
    // 005819d0: zero velocity/force, restore initial+translation; retain pins.
    void reset(std::array<float, 3> translation);

  private:
    std::vector<ClothParticle> particles_;
    std::vector<ClothSpring> springs_;
    std::uint32_t mode_ = 0;
    std::size_t columns_ = 0;
    std::vector<double> vertical_limits_;
    std::vector<std::array<float, 3>> committed_;
    bool basic_step(float seconds, const ClothScene& scene, std::uint32_t phase);
    bool span_ok() const;
    void commit_positions();
    void constrain_rows();
};
} // namespace genesis
