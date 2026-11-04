#include "cloth_particles.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace genesis {
ClothScene cloth_scene(std::uint8_t kind,
                       const std::vector<shaiya::graphics::native::Matrix>& finals,
                       const shaiya::graphics::native::Matrix& world, float floor,
                       std::uint32_t random_sample) {
    if (!std::isfinite(floor) || random_sample > 32767)
        throw std::invalid_argument("Invalid actor cloth scene input");
    ClothScene result;
    result.body = cloth_body(kind, finals, world);
    result.floor = floor;
    result.correction_direction = cloth_correction_direction(finals, world);
    result.wind_direction = result.correction_direction;
    // Original FILD/FMUL/FMUL/FSTP: no intermediate float store.
    std::uint32_t scale_bits = 0x38000100;
    float scale;
    std::memcpy(&scale, &scale_bits, sizeof(scale));
    result.wind_strength = float(static_cast<long double>(random_sample) * scale * .1f);
    return result;
}
namespace {
void finite(const std::array<float, 3>& v) {
    for (auto f : v)
        if (!std::isfinite(f))
            throw std::invalid_argument("Non-finite cloth state");
}
} // namespace
ClothParticles::ClothParticles(const ClothSetup& setup, const ClothMesh& mesh) {
    if (setup.mode > 1 || !setup.rows || !setup.columns)
        throw std::invalid_argument("Unsupported cloth particle grid/mode");
    // Match signed x86 grid arithmetic without allowing allocation overflow.
    if (setup.rows > 0x7fffffff || setup.columns > 0x7fffffff ||
        std::uint64_t(setup.rows) * setup.columns > 0x3fffffff ||
        (std::uint64_t(setup.rows) + 1) * (std::uint64_t(setup.columns) + 1) !=
            mesh.vertices.size())
        throw std::invalid_argument("Cloth grid does not match mesh");
    mode_ = setup.mode;
    columns_ = setup.columns;
    particles_.reserve(mesh.vertices.size());
    for (std::uint32_t row = 0; row <= setup.rows; ++row)
        for (std::uint32_t column = 0; column <= setup.columns; ++column) {
            // 00580e30: diagonal corners 1, opposite corners 2. Only the
            // FIRST/LAST ROW edges use 3; other non-corners use 6, even
            // on column edges. Preserve this asymmetric original rule.
            unsigned weight;
            if ((row == 0 && column == 0) || (row == setup.rows && column == setup.columns))
                weight = 1;
            else if ((row == setup.rows && column == 0) || (row == 0 && column == setup.columns))
                weight = 2;
            else if (row == 0 || row == setup.rows)
                weight = 3;
            else
                weight = 6;
            long double base = .5L;
            if (setup.mode == 0)
                base *= static_cast<long double>(.025f);
            else
                base /= static_cast<long double>(2 * std::uint64_t(setup.rows) * setup.columns);
            ClothParticle particle;
            particle.mass = float(base * weight * static_cast<long double>(0x1.555556p-2f));
            particle.inverse_mass = float(1.L / particle.mass);
            const auto& vertex = mesh.vertices[particles_.size()];
            finite(vertex.position);
            particle.position = particle.initial = vertex.position;
            particle.uv = vertex.uv;
            particles_.push_back(particle);
        }
    vertical_limits_.resize(particles_.size());
    for (std::size_t i = setup.columns + 1; i < particles_.size(); ++i) {
        const auto& parent = particles_[i - setup.columns - 1].initial;
        const auto& child = particles_[i].initial;
        auto x = static_cast<long double>(parent[0]) - child[0];
        auto y = static_cast<long double>(parent[1]) - child[1];
        auto z = static_cast<long double>(parent[2]) - child[2];
        vertical_limits_[i] = double(std::sqrt((x * x + y * y) + z * z) * (mode_ ? 10.L : 1.5L));
    }
    commit_positions();
    // 00581190 order and strict >1 predecessor rules are intentional.
    const auto width = std::size_t(setup.columns) + 1;
    auto add = [&](std::uint32_t row, std::uint32_t col, std::uint32_t other_row,
                   std::uint32_t other_col, bool z_first) {
        auto first = std::size_t(row) * width + col,
             second = std::size_t(other_row) * width + other_col;
        const auto& a = particles_[first].initial;
        const auto& b = particles_[second].initial;
        auto x = static_cast<long double>(a[0]) - b[0];
        auto y = static_cast<long double>(a[1]) - b[1];
        auto z = static_cast<long double>(a[2]) - b[2];
        auto squared = z_first ? (z * z + y * y) + x * x : (x * x + z * z) + y * y;
        springs_.push_back({first, second, mode_ == 0 ? 600.f : 500.f, mode_ == 0 ? 2.f : 1.f,
                            float(std::sqrt(squared))});
    };
    for (std::uint32_t row = 0; row <= setup.rows; ++row)
        for (std::uint32_t col = 0; col <= setup.columns; ++col) {
            if (col < setup.columns)
                add(row, col, row, col + 1, true);
            if (col > 1)
                add(row, col, row, col - 1, false);
            if (row < setup.rows)
                add(row, col, row + 1, col, false);
            if (row > 1)
                add(row, col, row - 1, col, false);
            if (row > 1 && col > 1)
                add(row, col, row - 1, col - 1, false);
            if (row < setup.rows && col > 1)
                add(row, col, row + 1, col - 1, true);
            if (row > 1 && col < setup.columns)
                add(row, col, row - 1, col + 1, true);
            if (row < setup.rows && col < setup.columns)
                add(row, col, row + 1, col + 1, true);
        }
}
bool ClothParticles::spring_forces() {
    for (const auto& spring : springs_) {
        auto& a = particles_[spring.first];
        auto& b = particles_[spring.second];
        auto x = static_cast<long double>(a.position[0]) - b.position[0];
        auto y = static_cast<long double>(
            float(static_cast<long double>(a.position[1]) - b.position[1]));
        auto z = static_cast<long double>(
            float(static_cast<long double>(a.position[2]) - b.position[2]));
        auto length = std::sqrt(mode_ == 1 ? (z * z + y * y) + x * x : (x * x + z * z) + y * y);
        if (length < 0)
            return false;
        float stored_length = float(length);
        auto inverse = 1.L / stored_length;
        auto nx = x * inverse;
        float ny = float(y * inverse), nz = float(z * inverse);
        std::array<float, 3> velocity{};
        for (unsigned i = 0; i < 3; ++i)
            velocity[i] = float(static_cast<long double>(a.velocity[i]) - b.velocity[i]);
        auto projection = mode_ == 1 ? (static_cast<long double>(velocity[2]) * z +
                                        static_cast<long double>(velocity[1]) * y) +
                                           static_cast<long double>(velocity[0]) * x
                                     : (static_cast<long double>(velocity[0]) * x +
                                        static_cast<long double>(velocity[2]) * z) +
                                           static_cast<long double>(velocity[1]) * y;
        auto damping = projection * inverse * spring.damping;
        auto stretch =
            (static_cast<long double>(stored_length) - spring.rest_length) * spring.stiffness;
        float magnitude = float(-(damping + stretch));
        auto fx = nx * magnitude;
        float fy = float(static_cast<long double>(ny) * magnitude),
              fz = float(static_cast<long double>(nz) * magnitude);
        if (!a.pinned) {
            a.force[0] = float(static_cast<long double>(a.force[0]) + fx);
            a.force[1] = float(static_cast<long double>(a.force[1]) + fy);
            a.force[2] = float(static_cast<long double>(a.force[2]) + fz);
        }
        if (!b.pinned) {
            b.force[0] = float(static_cast<long double>(b.force[0]) + float(-fx));
            b.force[1] = float(static_cast<long double>(b.force[1]) - fy);
            b.force[2] = float(static_cast<long double>(b.force[2]) - fz);
        }
    }
    return true;
}
void ClothParticles::external_forces(std::array<float, 3> wind, float strength) {
    finite(wind);
    if (!std::isfinite(strength))
        throw std::invalid_argument("Non-finite cloth wind strength");
    constexpr float epsilon = 0x1.a36e2ep-14f; // 006cd288 = float 0.0001
    for (auto& p : particles_) {
        p.force = {};
        if (p.pinned)
            continue;
        p.force[1] = float(static_cast<long double>(p.mass) * -100.f);
        const auto x = static_cast<long double>(p.velocity[0]);
        const auto y = static_cast<long double>(p.velocity[1]);
        const auto z = static_cast<long double>(p.velocity[2]);
        auto length = std::sqrt((x * x + y * y) + z * z);
        auto inverse = length > epsilon ? 1.L / length : 1.L;
        std::array<float, 3> direction{};
        for (unsigned axis = 0; axis < 3; ++axis) {
            auto component = -static_cast<long double>(p.velocity[axis]) * inverse;
            direction[axis] = float(component);
            // 00582540 compares X before its final rounding; Y/Z reload floats.
            if (std::fabs(axis ? static_cast<long double>(direction[axis]) : component) < epsilon)
                direction[axis] = 0;
        }
        auto speed = std::sqrt((x * x + z * z) + y * y);
        auto speed_squared = speed * speed;
        for (unsigned axis = 0; axis < 3; ++axis) {
            auto drag = static_cast<long double>(direction[axis]) * speed_squared;
            if (axis)
                drag = float(drag); // Original Y/Z temporary float stores.
            drag *= static_cast<long double>(.01f);
            if (axis == 2)
                drag = float(drag);
            p.force[axis] = float(static_cast<long double>(p.force[axis]) + drag);
            auto wind_force = static_cast<long double>(wind[axis]) * strength;
            if (axis == 0)
                wind_force = float(wind_force);
            p.force[axis] = float(static_cast<long double>(p.force[axis]) + wind_force);
        }
    }
}
void ClothParticles::forces(const std::vector<std::array<float, 3>>& values) {
    if (values.size() != particles_.size())
        throw std::invalid_argument("Cloth force count mismatch");
    for (const auto& v : values)
        finite(v);
    for (std::size_t i = 0; i < values.size(); ++i)
        particles_[i].force = values[i];
}
void ClothParticles::pin(const std::vector<ClothPin>& pins) {
    for (const auto& pin : pins) {
        if (pin.vertex >= particles_.size())
            throw std::invalid_argument("Cloth pin exceeds grid");
        finite(pin.position);
    }
    for (const auto& pin : pins) {
        auto& particle = particles_[pin.vertex];
        particle.pinned = true;
        particle.position = pin.position;
    }
}
void ClothParticles::integrate(float seconds) {
    if (!std::isfinite(seconds) || seconds < 0)
        throw std::invalid_argument("Invalid cloth delta");
    // Semi-implicit Euler: acceleration=force/mass, then velocity, then position.
    // Keep x87 intermediates extended until the original float stores.
    for (auto& p : particles_) {
        if (p.pinned)
            continue;
        for (unsigned axis = 0; axis < 3; ++axis) {
            auto acceleration = static_cast<long double>(p.force[axis]) * p.inverse_mass;
            p.acceleration[axis] = float(acceleration);
            if (axis == 0)
                acceleration = p.acceleration[axis]; // Original fstp/reload of X.
            auto increment = acceleration * seconds;
            if (axis < 2)
                increment = float(increment);
            p.velocity[axis] = float(static_cast<long double>(p.velocity[axis]) + increment);
            auto displacement = static_cast<long double>(p.velocity[axis]) * seconds;
            if (axis == 0)
                displacement = float(displacement);
            p.position[axis] = float(static_cast<long double>(p.position[axis]) + displacement);
        }
    }
}
ClothDetection ClothParticles::detect_contacts(const ClothBody& body, float floor,
                                               std::uint32_t phase) {
    ClothDetection result;
    if (phase == 1 || phase == 2)
        return result;
    if (!std::isfinite(floor))
        throw std::invalid_argument("Non-finite cloth collision floor");
    for (const auto& sphere : body) {
        finite(sphere.center);
        if (!std::isfinite(sphere.radius))
            throw std::invalid_argument("Non-finite cloth collision radius");
    }
    std::size_t near_count = 0, deep_count = 0;
    auto record = [&](std::size_t index) -> ClothCollisionRecord& {
        if (index >= result.records.size())
            result.records.resize(index + 1);
        return result.records[index];
    };
    for (std::size_t vertex = 0; vertex < particles_.size(); ++vertex) {
        auto& p = particles_[vertex];
        if (p.pinned)
            continue;
        if (p.position[1] < floor)
            p.position[1] = float(static_cast<long double>(floor) + .005f);
        for (int index = 3; index >= 0; --index) {
            const auto& sphere = body[std::size_t(index)];
            std::array<float, 3> delta{};
            for (unsigned axis = 0; axis < 3; ++axis)
                delta[axis] =
                    float(static_cast<long double>(p.position[axis]) - sphere.center[axis]);
            // Exact center is ignored even for a positive-radius sphere.
            if (delta == std::array<float, 3>{})
                continue;
            auto x = static_cast<long double>(delta[0]), y = static_cast<long double>(delta[1]),
                 z = static_cast<long double>(delta[2]);
            auto length = std::sqrt((x * x + y * y) + z * z);
            auto gap = length - sphere.radius;
            if (gap < static_cast<long double>(-.0001f)) {
                auto& entry = record(deep_count);
                entry = {std::ptrdiff_t(vertex), sphere.center, sphere.radius};
                result.status = -1;
                // Original fourth sphere omits the counter increment and
                // immediately continues to the next particle (00583d7c).
                if (index == 3)
                    break;
                ++deep_count;
            } else if (gap < static_cast<long double>(.01f) && result.status != -1) {
                auto& entry = record(near_count++);
                entry.vertex = std::ptrdiff_t(vertex);
                auto inverse = 1.L / length;
                entry.normal_or_center = {float(x * inverse), float(y * inverse),
                                          float(z * inverse)};
                result.status = 1;
            }
        }
    }
    return result;
}
void ClothParticles::respond_contacts(const std::vector<ClothContact>& contacts) {
    for (const auto& contact : contacts) {
        if (contact.vertex == -1)
            continue;
        if (contact.vertex < 0 || std::size_t(contact.vertex) >= particles_.size())
            throw std::invalid_argument("Cloth contact exceeds grid");
        finite(contact.normal);
    }
    for (const auto& contact : contacts) {
        if (contact.vertex == -1)
            continue;
        auto& velocity = particles_[std::size_t(contact.vertex)].velocity;
        const auto x = static_cast<long double>(velocity[0]);
        const auto y = static_cast<long double>(velocity[1]);
        const auto z = static_cast<long double>(velocity[2]);
        const auto nx = static_cast<long double>(contact.normal[0]);
        const auto ny = static_cast<long double>(contact.normal[1]);
        const auto nz = static_cast<long double>(contact.normal[2]);
        float dot = float((z * nz + y * ny) + x * nx);
        float normal_x = float(nx * dot);
        auto normal_y = ny * dot, normal_z = nz * dot;
        float tangent_x = float((x - normal_x) * .5L);
        float tangent_y = float((y - normal_y) * .5L);
        float tangent_z = float(static_cast<long double>(float(z - normal_z)) * .5L);
        float reflected_x =
            float(static_cast<long double>(normal_x) * static_cast<long double>(-.35f));
        float reflected_y = float(normal_y * static_cast<long double>(-.35f));
        auto reflected_z = normal_z * static_cast<long double>(-.35f);
        velocity = {float(static_cast<long double>(reflected_x) + tangent_x),
                    float(static_cast<long double>(reflected_y) + tangent_y),
                    float(reflected_z + tangent_z)};
    }
}
void ClothParticles::correct_penetrations(const std::vector<ClothCollisionRecord>& records,
                                          std::array<float, 3> direction,
                                          ClothCorrection correction) {
    finite(direction);
    if (correction != ClothCorrection::scaled && correction != ClothCorrection::unscaled)
        throw std::invalid_argument("Unknown cloth correction policy");
    for (const auto& record : records) {
        if (record.radius == -1)
            break;
        if (record.vertex < 0 || std::size_t(record.vertex) >= particles_.size())
            throw std::invalid_argument("Cloth penetration exceeds grid");
        finite(record.normal_or_center);
        if (!std::isfinite(record.radius))
            throw std::invalid_argument("Non-finite cloth penetration radius");
    }
    for (const auto& record : records) {
        if (record.radius == -1)
            break;
        auto& position = particles_[std::size_t(record.vertex)].position;
        auto x = static_cast<long double>(
            float(static_cast<long double>(position[0]) - record.normal_or_center[0]));
        auto y = static_cast<long double>(position[1]) - record.normal_or_center[1];
        auto z = static_cast<long double>(position[2]) - record.normal_or_center[2];
        auto distance = std::sqrt((z * z + y * y) + x * x);
        auto amount = std::fabs(static_cast<long double>(record.radius) - distance);
        if (correction == ClothCorrection::scaled)
            amount *= 1.5L;
        for (unsigned axis = 0; axis < 3; ++axis) {
            auto delta = static_cast<long double>(direction[axis]) * amount;
            if ((correction == ClothCorrection::scaled && axis != 0) ||
                (correction == ClothCorrection::unscaled && axis != 1))
                delta = float(delta);
            position[axis] = float(static_cast<long double>(position[axis]) + delta);
        }
    }
}
void ClothParticles::commit_positions() {
    committed_.resize(particles_.size());
    for (std::size_t i = 0; i < particles_.size(); ++i)
        committed_[i] = particles_[i].position;
}
bool ClothParticles::span_ok() const {
    const auto& first = committed_[0];
    const auto& next = committed_[columns_ + 1];
    auto x = static_cast<long double>(float(static_cast<long double>(first[0]) - next[0]));
    auto y = static_cast<long double>(first[1]) - next[1];
    auto z = static_cast<long double>(first[2]) - next[2];
    return !(std::sqrt((z * z + y * y) + x * x) > 20.f);
}
void ClothParticles::constrain_rows() {
    auto width = columns_ + 1;
    for (std::size_t i = width; i < particles_.size(); ++i) {
        auto& p = particles_[i];
        if (p.pinned)
            continue;
        const auto& parent = particles_[i - width].position;
        float x = float(static_cast<long double>(parent[0]) - p.position[0]);
        float y = float(static_cast<long double>(parent[1]) - p.position[1]);
        auto z = static_cast<long double>(parent[2]) - p.position[2];
        float stored_z = float(z);
        float length = float(std::sqrt((z * stored_z + static_cast<long double>(y) * y) +
                                       static_cast<long double>(x) * x));
        if (!(length > vertical_limits_[i]))
            continue;
        auto excess = static_cast<long double>(length) - vertical_limits_[i];
        auto inverse = 1.L / length;
        auto dx = static_cast<long double>(x) * inverse * excess;
        float dy = float(static_cast<long double>(float(inverse * y)) * excess);
        float dz = float(static_cast<long double>(float(inverse * stored_z)) * excess);
        p.position = {float(static_cast<long double>(p.position[0]) + dx),
                      float(static_cast<long double>(p.position[1]) + dy),
                      float(static_cast<long double>(p.position[2]) + dz)};
    }
}
bool ClothParticles::basic_step(float seconds, const ClothScene& scene, std::uint32_t phase) {
    if (seconds > 0) {
        external_forces(scene.wind_direction, scene.wind_strength);
        if (!spring_forces())
            return false;
        integrate(seconds);
        auto detection = detect_contacts(scene.body, scene.floor, phase);
        if (detection.status == -1)
            correct_penetrations(detection.records, scene.correction_direction,
                                 ClothCorrection::unscaled);
        else if (detection.status == 1) {
            std::vector<ClothContact> contacts;
            for (const auto& record : detection.records)
                contacts.push_back({record.vertex, record.normal_or_center});
            respond_contacts(contacts);
        }
    }
    commit_positions();
    return true;
}
bool ClothParticles::step(float seconds, const ClothScene& scene, std::uint32_t phase) {
    if (!std::isfinite(seconds) || seconds < 0)
        throw std::invalid_argument("Invalid cloth step delta");
    if (mode_ == 0) {
        if (!basic_step(seconds, scene, phase))
            return false;
        constrain_rows();
        return span_ok(); // Check the pre-constraint committed pair (005849a0).
    }
    external_forces(scene.wind_direction, scene.wind_strength);
    if (!spring_forces())
        return false;
    auto width = columns_ + 1;
    for (std::size_t i = width; i < particles_.size(); ++i) {
        auto& p = particles_[i];
        if (p.pinned)
            continue;
        const auto& parent = particles_[i - width].position;
        auto x = static_cast<long double>(parent[0]) - p.position[0];
        float y = float(static_cast<long double>(parent[1]) - p.position[1]);
        auto z = static_cast<long double>(parent[2]) - p.position[2];
        float stored_z = float(z);
        auto distance = std::sqrt((z * stored_z + static_cast<long double>(y) * y) + x * x);
        if (distance > static_cast<long double>(vertical_limits_[i]) * 5.L) {
            p.force = {};
            p.velocity = {};
            p.position = {
                float(static_cast<long double>(p.position[0]) + x * .5f),
                float(static_cast<long double>(p.position[1]) + static_cast<long double>(y) * .5f),
                float(static_cast<long double>(p.position[2]) +
                      float(static_cast<long double>(stored_z) * .5f))};
        } else {
            p.force[0] =
                float(static_cast<long double>(p.force[0]) + x * static_cast<long double>(.2f));
            p.force[1] =
                float(static_cast<long double>(p.force[1]) + static_cast<long double>(y) * .2f);
            p.force[2] = float(static_cast<long double>(p.force[2]) +
                               float(static_cast<long double>(stored_z) * .2f));
        }
    }
    integrate(seconds);
    auto detection = detect_contacts(scene.body, scene.floor, phase);
    if (detection.status == -1)
        correct_penetrations(detection.records, scene.correction_direction,
                             ClothCorrection::scaled);
    else
        for (unsigned iteration = 0; detection.status == 1 && iteration < 10; ++iteration) {
            std::vector<ClothContact> contacts;
            for (const auto& record : detection.records)
                contacts.push_back({record.vertex, record.normal_or_center});
            respond_contacts(contacts);
            detection = detect_contacts(scene.body, scene.floor, phase);
        }
    commit_positions();
    return span_ok();
}
ClothTickStats ClothParticles::advance(float seconds, std::uint32_t phase,
                                       std::array<float, 3> reset_translation,
                                       const ClothScene& scene) {
    if (!std::isfinite(seconds) || seconds < 0)
        throw std::invalid_argument("Invalid cloth frame delta");
    ClothTickStats stats;
    if (phase == 2) {
        reset(reset_translation);
        stats.resets = 1;
        return stats;
    }
    if (phase > 2)
        return stats;
    float count = float(static_cast<long double>(seconds) * 200.f);
    if (count > 0x1p24f)
        throw std::invalid_argument("Cloth frame exceeds original float counter range");
    float completed = 0;
    do {
        ++stats.substeps;
        for (unsigned attempt = 0; attempt < (mode_ == 1 ? 3u : 1u); ++attempt) {
            ++stats.attempts;
            if (!step(0x1.0624dep-11f, scene, phase)) { // 0x3a03126f.
                reset(reset_translation);
                ++stats.resets;
            }
        }
        completed = float(static_cast<long double>(completed) + 1.f);
    } while (completed < count);
    return stats;
}
void ClothParticles::reset(std::array<float, 3> translation) {
    finite(translation);
    for (auto& p : particles_) {
        for (unsigned axis = 0; axis < 3; ++axis)
            p.position[axis] = float(static_cast<long double>(p.initial[axis]) + translation[axis]);
        p.velocity = {};
        p.force = {};
    }
    commit_positions();
}
} // namespace genesis
