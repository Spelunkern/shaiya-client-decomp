#include "cloth_body.h"
#include "math.h"
#include <stdexcept>
namespace genesis {
using Matrix = shaiya::graphics::native::Matrix;
std::array<float, 3>
cloth_correction_direction(const shaiya::graphics::native::Matrix& orientation) {
    std::array<float, 3> forward = {orientation.m[2][0], orientation.m[2][1], orientation.m[2][2]};
    auto length = std::sqrt(dot(forward, forward));
    if (!(length > 0) || !std::isfinite(length))
        throw std::invalid_argument("Invalid cloth correction orientation");
    for (auto& value : forward)
        value = -(value / length);
    return forward;
}
std::array<float, 3> cloth_correction_direction(const std::vector<Matrix>& finals,
                                                const Matrix& world) {
    if (finals.size() < 5)
        throw std::invalid_argument("Cloth correction requires bone four");
    return cloth_correction_direction(multiply(finals[4], world));
}
ClothBody cloth_body(std::uint8_t kind, const std::vector<shaiya::graphics::native::Matrix>& finals,
                     const shaiya::graphics::native::Matrix& world) {
    if (kind > 15)
        throw std::invalid_argument("Unknown cloth actor kind");
    if (finals.size() < 6)
        throw std::invalid_argument("Cloth body requires bones one through five");
    // Branches 00583403..00583bec. Radii are authored constants, independent
    // of scale in the final/world matrices. Do not expand them by world scale.
    static constexpr float radii[16][4] = {
        {.25f, 0, .15f, 0},       {.25f, 0, .15f, 0},      {.25f, 0, .15f, 0},
        {.25f, 0, .15f, 0},       {.25f, 0, .15f, 0},      {.25f, 0, .15f, 0},
        {.22f, 0, .14f, 0},       {.22f, 0, .14f, 0},      {.28f, 0, .28f, 0},
        {.23f, .18f, .18f, .15f}, {.23f, 0, .23f, 0},      {.23f, .18f, .18f, .15f},
        {.2f, .15f, .15f, .15f},  {.2f, .15f, .15f, .15f}, {.2f, .15f, .15f, .15f},
        {.2f, .15f, .15f, .15f}};
    std::array<unsigned, 4> bones = {kind == 11 ? 2u : 1u, (kind >= 4 && kind <= 9) ? 2u : 3u, 4,
                                     5};
    ClothBody result;
    for (unsigned i = 0; i < 4; ++i) {
        auto matrix = multiply(finals[bones[i]], world);
        result[i] = {{matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]}, radii[kind][i]};
    }
    return result;
}
} // namespace genesis
