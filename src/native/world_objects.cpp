#include "world_objects.h"
#include "math.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace genesis {
namespace {
float word_float(const std::vector<std::uint8_t>& record, std::size_t at) {
    float value;
    std::memcpy(&value, record.data() + at, 4);
    return value;
}
std::int32_t word_int(const std::vector<std::uint8_t>& record, std::size_t at) {
    std::int32_t value;
    std::memcpy(&value, record.data() + at, 4);
    return value;
}
const char* const directories[4] = {"data/entity/building", "data/entity/shape",
                                    "data/entity/tree", "data/entity/grass"};
} // namespace

Matrix WorldObjects::placement_matrix(const std::array<float, 3>& position,
                                      const std::array<float, 3>& forward,
                                      const std::array<float, 3>& up) {
    auto normalize = [](float x, float y, float z) {
        float inverse = 1.f / std::sqrt(x * x + y * y + z * z);
        return std::array<float, 3>{x * inverse, y * inverse, z * inverse};
    };
    auto f = normalize(forward[0], forward[1], forward[2]);
    auto r = normalize(f[2] * up[1] - f[1] * up[2], f[0] * up[2] - f[2] * up[0],
                       f[1] * up[0] - f[0] * up[1]);
    auto u = normalize(f[1] * r[2] - f[2] * r[1], f[2] * r[0] - f[0] * r[2],
                       f[0] * r[1] - f[1] * r[0]);
    Matrix m = identity;
    m.m[0][0] = r[0], m.m[0][1] = r[1], m.m[0][2] = r[2];
    m.m[1][0] = u[0], m.m[1][1] = u[1], m.m[1][2] = u[2];
    m.m[2][0] = f[0], m.m[2][1] = f[1], m.m[2][2] = f[2];
    m.m[3][0] = position[0], m.m[3][1] = position[1], m.m[3][2] = position[2];
    return m;
}

WorldObjects::WorldObjects(Renderer& renderer, const shaiya::portable::Archive& archive,
                           const WorldTail& tail)
    : renderer_(renderer), archive_(archive) {
    for (std::uint8_t list = 0; list < 4 && list < tail.placements.size(); ++list) {
        const auto& section = tail.placements[list];
        for (const auto& name : section.names)
            paths_[list].push_back(std::string(directories[list]) + "/" + name);
        for (const auto& record : section.records) {
            auto model = word_int(record, 0);
            if (model < 0 || std::size_t(model) >= paths_[list].size())
                continue; // the original also ignores out-of-range indices
            std::array<float, 3> position = {word_float(record, 4), word_float(record, 8),
                                             word_float(record, 12)};
            std::array<float, 3> forward = {word_float(record, 16), word_float(record, 20),
                                            word_float(record, 24)};
            std::array<float, 3> up = {word_float(record, 28), word_float(record, 32),
                                       word_float(record, 36)};
            placements_.push_back(
                {list, std::uint32_t(model), placement_matrix(position, forward, up)});
        }
    }
}
std::size_t WorldObjects::loaded_models() const {
    return models_.size();
}
void WorldObjects::draw(const std::array<float, 2>& center, float distance, const Matrix& root) {
    drawn_ = 0;
    const float limit = distance * distance;
    for (const auto& placement : placements_) {
        float dx = placement.world.m[3][0] - center[0], dz = placement.world.m[3][2] - center[1];
        if (dx * dx + dz * dz > limit)
            continue;
        std::uint32_t key = std::uint32_t(placement.list) << 24 | placement.model;
        auto found = models_.find(key);
        if (found == models_.end()) {
            if (failed_.count(key))
                continue;
            try {
                found = models_
                            .emplace(key, std::make_unique<ModelResource>(
                                              renderer_, archive_,
                                              paths_[placement.list][placement.model]))
                            .first;
            } catch (const std::exception& e) {
                std::fprintf(stderr, "World model %s: %s\n",
                             paths_[placement.list][placement.model].c_str(), e.what());
                failed_[key] = true;
                continue;
            }
        }
        found->second->draw(multiply(placement.world, root));
        ++drawn_;
    }
}
} // namespace genesis
