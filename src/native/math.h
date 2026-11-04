#pragma once
#include "graphics/render_types.h"
#include <array>
#include <cmath>
#include <stdexcept>
namespace genesis {
using Vec3 = std::array<float, 3>;
inline float dot(Vec3 a, Vec3 b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
inline Vec3 normalized(Vec3 a) {
    float length = std::sqrt(dot(a, a));
    if (!(length > 0))
        throw std::invalid_argument("Zero-length direction");
    return {a[0] / length, a[1] / length, a[2] / length};
}
inline shaiya::graphics::native::Matrix look_at_lh(Vec3 eye, Vec3 target, Vec3 up) {
    auto z = normalized(Vec3{target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]});
    auto x = normalized(cross(up, z)), y = cross(z, x);
    return {{{x[0], y[0], z[0], 0},
             {x[1], y[1], z[1], 0},
             {x[2], y[2], z[2], 0},
             {-dot(x, eye), -dot(y, eye), -dot(z, eye), 1}}};
}
inline shaiya::graphics::native::Matrix perspective_lh(float fov, float aspect, float near_z,
                                                       float far_z) {
    if (!(fov > 0 && fov < 3.14159f && aspect > 0 && near_z > 0 && far_z > near_z))
        throw std::invalid_argument("Invalid perspective");
    float y = 1 / std::tan(fov * .5f), z = far_z / (far_z - near_z);
    return {{{y / aspect, 0, 0, 0}, {0, y, 0, 0}, {0, 0, z, 1}, {0, 0, -near_z * z, 0}}};
}
inline shaiya::graphics::native::Matrix multiply(const shaiya::graphics::native::Matrix& a,
                                                 const shaiya::graphics::native::Matrix& b) {
    shaiya::graphics::native::Matrix result = {};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                result.m[i][j] += a.m[i][k] * b.m[k][j];
    return result;
}
} // namespace genesis
