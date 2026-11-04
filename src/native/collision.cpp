#include "collision.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace genesis {
namespace {
struct Plane {
    float n[3], d;
};
// 005a0de0
Plane plane_from_triangle(const Vec3f& p0, const Vec3f& p1, const Vec3f& p2) {
    float x = (p1[1] - p0[1]) * (p2[2] - p0[2]) - (p1[2] - p0[2]) * (p2[1] - p0[1]);
    float y = (p1[2] - p0[2]) * (p2[0] - p0[0]) - (p2[2] - p0[2]) * (p1[0] - p0[0]);
    float z = (p2[1] - p0[1]) * (p1[0] - p0[0]) - (p1[1] - p0[1]) * (p2[0] - p0[0]);
    float inverse = 1.f / std::sqrt(z * z + y * y + x * x);
    Plane plane;
    plane.n[0] = inverse * x;
    plane.n[1] = inverse * y;
    plane.n[2] = inverse * z;
    plane.d = -(plane.n[0] * p0[0] + p0[1] * plane.n[1] + p0[2] * plane.n[2]);
    return plane;
}
// 005a0080: smallest root of a*t^2 + b*t + c in (0, max).
bool lowest_root(float a, float b, float c, float max, float& root) {
    float determinant = b * b - a * c * 4.f;
    if (!(determinant >= 0.f))
        return false;
    float sq = std::sqrt(determinant);
    float r1 = (-b - sq) / (a + a), r2 = (sq - b) / (a + a);
    if (r2 < r1)
        std::swap(r1, r2);
    if (0.f < r1 && r1 < max) {
        root = r1;
        return true;
    }
    if (r2 > 0.f && r2 < max) {
        root = r2;
        return true;
    }
    return false;
}
// 005a0170: the sign-bit barycentric test.
bool point_in_triangle(const float* p, const Vec3f& a, const Vec3f& b, const Vec3f& c) {
    float e0x = b[0] - a[0], e0y = b[1] - a[1], e0z = b[2] - a[2];
    float e1x = c[0] - a[0], e1y = c[1] - a[1], e1z = c[2] - a[2];
    float aa = e0x * e0x + e0y * e0y + e0z * e0z;
    float bb = e1x * e0x + e1y * e0y + e1z * e0z;
    float cc = e1x * e1x + e1y * e1y + e1z * e1z;
    float v0 = (p[0] - a[0]) * e0x + (p[1] - a[1]) * e0y + (p[2] - a[2]) * e0z;
    float v1 = (p[0] - a[0]) * e1x + (p[1] - a[1]) * e1y + (p[2] - a[2]) * e1z;
    float x = v0 * cc - v1 * bb;
    float y = v1 * aa - v0 * bb;
    auto bits = [](float f) {
        std::uint32_t u;
        std::memcpy(&u, &f, 4);
        return u;
    };
    return (~(bits(y) | bits(x)) & bits((y + x) - (cc * aa - bb * bb)) & 0x80000000u) != 0;
}
} // namespace

void check_triangle(CollisionPacket& packet, const Vec3f& p0, const Vec3f& p1, const Vec3f& p2) {
    const Plane plane = plane_from_triangle(p0, p1, p2);
    const auto& vn = packet.normalized_velocity;
    const auto& v = packet.velocity;
    const auto& base = packet.base;
    float facing = plane.n[0] * vn[0] + plane.n[2] * vn[2] + plane.n[1] * vn[1];
    if (facing > 0.f)
        return;
    float signed_distance =
        plane.n[0] * base[0] + plane.n[2] * base[2] + plane.n[1] * base[1] + plane.d;
    float normal_dot_velocity = plane.n[0] * v[0] + plane.n[2] * v[2] + plane.n[1] * v[1];
    bool embedded = false;
    long double t0;
    if (normal_dot_velocity == 0.f) {
        if (std::fabs(static_cast<double>(signed_distance)) >= 1.0)
            return;
        embedded = true;
        t0 = 0;
    } else {
        t0 = (-1.L - signed_distance) / normal_dot_velocity;
        long double t1 = (1.L - signed_distance) / normal_dot_velocity;
        if (t1 < t0)
            std::swap(t0, t1);
        if (t0 > 1.L || t1 < 0.L)
            return;
        if (0.L <= t0) {
            if (1.L < t0)
                t0 = 1.L;
        } else
            t0 = 0.L;
    }
    float t = 1.f;
    float hit[3] = {0, 0, 0};
    bool found = false, point_ready = false;
    if (!embedded) {
        float point[3] = {float(static_cast<long double>(base[0] - plane.n[0]) + v[0] * t0),
                          float(static_cast<long double>(v[1]) * t0 + (base[1] - plane.n[1])),
                          float(static_cast<long double>(v[2]) * t0 + (base[2] - plane.n[2]))};
        if (point_in_triangle(point, p0, p1, p2)) {
            t = float(t0);
            hit[0] = point[0];
            hit[1] = point[1];
            hit[2] = point[2];
            point_ready = true;
        }
    }
    if (!point_ready) {
        const float velocity_sq = v[1] * v[1] + v[2] * v[2] + v[0] * v[0];
        float root;
        const Vec3f* verts[3] = {&p0, &p1, &p2};
        for (const auto* vertex : verts) {
            const auto& p = *vertex;
            float dot = v[1] * (base[1] - p[1]) + v[2] * (base[2] - p[2]) + (base[0] - p[0]) * v[0];
            float c = ((p[2] - base[2]) * (p[2] - base[2]) + (p[1] - base[1]) * (p[1] - base[1]) +
                       (p[0] - base[0]) * (p[0] - base[0])) -
                      1.f;
            if (lowest_root(velocity_sq, dot + dot, c, t, root)) {
                t = root;
                hit[0] = p[0];
                hit[1] = p[1];
                hit[2] = p[2];
                found = true;
            }
        }
        const Vec3f* starts[3] = {&p0, &p1, &p2};
        const Vec3f* ends[3] = {&p1, &p2, &p0};
        for (int edge = 0; edge < 3; ++edge) {
            const auto& s = *starts[edge];
            const auto& e = *ends[edge];
            float ez = e[2] - s[2], ey = e[1] - s[1], ex = e[0] - s[0];
            float bz = s[2] - base[2], by = s[1] - base[1], bx = s[0] - base[0];
            float edge_sq = ez * ez + ey * ey + ex * ex;
            float edge_dot_v = v[1] * ey + v[2] * ez + ex * v[0];
            float edge_dot_b = bz * ez + by * ey + ex * bx;
            float a = edge_dot_v * edge_dot_v - edge_sq * velocity_sq;
            float vb = v[1] * by + v[2] * bz + v[0] * bx;
            float b = (vb + vb) * edge_sq - (edge_dot_b * edge_dot_v + edge_dot_b * edge_dot_v);
            float c = edge_dot_b * edge_dot_b + (1.f - (bz * bz + by * by + bx * bx)) * edge_sq;
            if (lowest_root(a, b, c, t, root)) {
                float f = (edge_dot_v * root - edge_dot_b) / edge_sq;
                if (0.f <= f && f < 1.f) {
                    t = root;
                    found = true;
                    hit[0] = ex * f + s[0];
                    hit[1] = ey * f + s[1];
                    hit[2] = ez * f + s[2];
                    if (edge == 2)
                        goto accept; // the original skips the "any hit" test here
                }
            }
        }
        if (!found)
            return;
    }
accept:
    float distance = std::sqrt(v[2] * v[2] + v[1] * v[1] + v[0] * v[0]) * t;
    if (packet.found && float(packet.nearest) <= distance)
        return;
    packet.nearest = distance;
    packet.intersection = {hit[0], hit[1], hit[2]};
    packet.found = true;
}

void check_world_triangles(CollisionPacket& packet, const std::vector<Vec3f>& vertices,
                           const std::vector<std::uint16_t>& indices) {
    const float ix = 1.f / packet.radius[0], iy = 1.f / packet.radius[1],
                iz = 1.f / packet.radius[2];
    auto scaled = [&](std::uint16_t i) {
        const auto& v = vertices.at(i);
        return Vec3f{v[0] * ix, v[1] * iy, v[2] * iz};
    };
    for (std::size_t i = 0; i + 2 < indices.size(); i += 3)
        check_triangle(packet, scaled(indices[i]), scaled(indices[i + 1]), scaled(indices[i + 2]));
}

void check_dungeon(CollisionPacket& packet, const Dungeon& dungeon) {
    Vec3f lo, hi;
    for (int axis = 0; axis < 3; ++axis) {
        float a = packet.base[axis] * packet.radius[axis];
        float b = a + packet.velocity[axis] * packet.radius[axis];
        lo[axis] = std::min(a, b) - packet.radius[axis] - 1.f;
        hi[axis] = std::max(a, b) + packet.radius[axis] + 1.f;
    }
    for (const auto& node : dungeon.nodes) {
        if (node.collision_indices.empty())
            continue;
        bool touches = true;
        for (int axis = 0; axis < 3; ++axis)
            if (node.collision.maximum[axis] < lo[axis] || node.collision.minimum[axis] > hi[axis])
                touches = false;
        if (touches)
            check_world_triangles(packet, node.collision_vertices, node.collision_indices);
    }
}

Vec3f collide_and_slide(const CollisionWorld& world, const Vec3f& radius, Vec3f base,
                        Vec3f velocity, bool recurse, int& pass_counter) {
    if (5 < pass_counter)
        return base;
    CollisionPacket packet;
    packet.radius = radius;
    packet.base = base;
    packet.velocity = velocity;
    float inverse = 1.f / std::sqrt(velocity[2] * velocity[2] + velocity[1] * velocity[1] +
                                    velocity[0] * velocity[0]);
    packet.normalized_velocity = {inverse * velocity[0], inverse * velocity[1],
                                  inverse * velocity[2]};
    world(packet);
    if (!packet.found)
        return {velocity[0] + base[0], velocity[1] + base[1], velocity[2] + base[2]};
    const float dest_z = velocity[2] + base[2], dest_y = velocity[1] + base[1],
                dest_x = velocity[0] + base[0];
    Vec3f new_base = base;
    if (0.05 <= packet.nearest) {
        float length = float(static_cast<long double>(packet.nearest) - 0.05);
        float scale = length / std::sqrt(velocity[2] * velocity[2] + velocity[1] * velocity[1] +
                                         velocity[0] * velocity[0]);
        Vec3f shortened = {scale * velocity[0], scale * velocity[1], scale * velocity[2]};
        new_base = {shortened[0] + base[0], shortened[1] + base[1], shortened[2] + base[2]};
        float unit = 1.f / std::sqrt(shortened[2] * shortened[2] + shortened[1] * shortened[1] +
                                     shortened[0] * shortened[0]);
        Vec3f direction = {unit * shortened[0], unit * shortened[1], unit * shortened[2]};
        packet.intersection[0] -= direction[0] * 0.05f;
        packet.intersection[1] -= direction[1] * 0.05f;
        packet.intersection[2] -= direction[2] * 0.05f;
    }
    float nx = new_base[0] - packet.intersection[0], ny = new_base[1] - packet.intersection[1],
          nz = new_base[2] - packet.intersection[2];
    float normalise = 1.f / std::sqrt(nz * nz + ny * ny + nx * nx);
    nx *= normalise;
    ny *= normalise;
    nz *= normalise;
    const auto& origin = packet.intersection;
    float d = -(origin[2] * nz + origin[1] * ny + origin[0] * nx);
    float distance = nz * dest_z + ny * dest_y + nx * dest_x + d;
    Vec3f slide = {(dest_x - nx * distance) - origin[0], (dest_y - ny * distance) - origin[1],
                   (dest_z - nz * distance) - origin[2]};
    if (0.05f <= std::sqrt(slide[2] * slide[2] + slide[1] * slide[1] + slide[0] * slide[0]) &&
        recurse) {
        ++pass_counter;
        return collide_and_slide(world, radius, new_base, slide, recurse, pass_counter);
    }
    return new_base;
}

Vec3f move_with_collision(const CollisionWorld& world, const Vec3f& position,
                          const Vec3f& velocity, bool recurse, const Vec3f& radius) {
    int pass = 0;
    Vec3f scaled_position = {position[0] / radius[0], position[1] / radius[1],
                             position[2] / radius[2]};
    Vec3f horizontal = {velocity[0] / radius[0], 0.f, velocity[2] / radius[2]};
    Vec3f result = collide_and_slide(world, radius, scaled_position, horizontal, recurse, pass);
    if (result[1] < scaled_position[1])
        result = scaled_position;
    pass = 0;
    Vec3f vertical = {0.f, velocity[1] / radius[1], 0.f};
    Vec3f final_position = collide_and_slide(world, radius, result, vertical, false, pass);
    return {final_position[0] * radius[0], final_position[1] * radius[1],
            final_position[2] * radius[2]};
}
} // namespace genesis
