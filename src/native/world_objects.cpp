#include "world_objects.h"
#include "math.h"
#include <cctype>
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
    if (tail.placements.size() > 6) {
        const auto& section = tail.placements[6];
        std::vector<std::string> all;
        for (const auto& name : section.names) {
            std::string path = "data/world/dungeon/" + name;
            if (!archive.find(path)) {
                if (all.empty())
                    all = archive.paths();
                std::string lower = name;
                for (auto& c : lower)
                    c = char(std::tolower((unsigned char)c));
                for (const auto& candidate : all) {
                    std::string low = candidate;
                    for (auto& c : low)
                        c = char(std::tolower((unsigned char)c));
                    if (low.size() > lower.size() &&
                        low.compare(low.size() - lower.size(), lower.size(), lower) == 0 &&
                        low[low.size() - lower.size() - 1] == '/') {
                        path = candidate;
                        break;
                    }
                }
            }
            dungeon_paths_.push_back(path);
        }
        for (const auto& record : section.records) {
            auto model = word_int(record, 0);
            if (model < 0 || std::size_t(model) >= dungeon_paths_.size())
                continue;
            dungeon_placements_.push_back(
                {std::uint32_t(model),
                 placement_matrix({word_float(record, 4), word_float(record, 8), word_float(record, 12)},
                                  {word_float(record, 16), word_float(record, 20), word_float(record, 24)},
                                  {word_float(record, 28), word_float(record, 32), word_float(record, 36)})});
        }
    }
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
void WorldObjects::collide(CollisionPacket& packet, float broad_phase) {
    collide_dungeons(packet);
    const float px = packet.base[0] * packet.radius[0], py = packet.base[1] * packet.radius[1],
                pz = packet.base[2] * packet.radius[2];
    const float limit = broad_phase * broad_phase;
    for (std::size_t index = 0; index < placements_.size(); ++index) {
        const auto& placement = placements_[index];
        float dx = placement.world.m[3][0] - px, dz = placement.world.m[3][2] - pz;
        if (dx * dx + dz * dz > limit)
            continue;
        std::uint32_t key = std::uint32_t(placement.list) << 24 | placement.model;
        auto& collider = colliders_[key];
        if (!collider.loaded) {
            collider.loaded = true;
            try {
                auto model = decode_smod(archive_.read(paths_[placement.list][placement.model]));
                collider.sphere = model.sphere;
                collider.mesh = decode_model_collision(model);
            } catch (const std::exception&) {
                collider.mesh = {};
            }
        }
        if (collider.mesh.indices.empty())
            continue;
        const auto& m = placement.world.m;
        const float cx = collider.sphere[0], cy = collider.sphere[1], cz = collider.sphere[2];
        float wx = cx * m[0][0] + cy * m[1][0] + cz * m[2][0] + m[3][0];
        float wy = cx * m[0][1] + cy * m[1][1] + cz * m[2][1] + m[3][1];
        float wz = cx * m[0][2] + cy * m[1][2] + cz * m[2][2] + m[3][2];
        float ex = wx - px, ey = wy - py, ez = wz - pz;
        float radius = collider.sphere[3] < 2.f ? 2.f : collider.sphere[3];
        if (std::sqrt(ex * ex + ey * ey + ez * ez) > radius + 1.f)
            continue;
        auto cached = world_vertices_.find(index);
        if (cached == world_vertices_.end()) {
            std::vector<Vec3f> vertices;
            vertices.reserve(collider.mesh.vertices.size());
            for (const auto& v : collider.mesh.vertices)
                vertices.push_back({v[0] * m[0][0] + v[1] * m[1][0] + v[2] * m[2][0] + m[3][0],
                                    v[0] * m[0][1] + v[1] * m[1][1] + v[2] * m[2][1] + m[3][1],
                                    v[0] * m[0][2] + v[1] * m[1][2] + v[2] * m[2][2] + m[3][2]});
            cached = world_vertices_.emplace(index, std::move(vertices)).first;
        }
        check_world_triangles(packet, cached->second, collider.mesh.indices);
    }
}
std::size_t WorldObjects::loaded_models() const {
    return models_.size();
}
void WorldObjects::collide_dungeons(CollisionPacket& packet) {
    const float px = packet.base[0] * packet.radius[0], pz = packet.base[2] * packet.radius[2];
    for (std::size_t index = 0; index < dungeon_placements_.size(); ++index) {
        const auto& placement = dungeon_placements_[index];
        auto loaded = dungeons_.find(placement.model);
        if (loaded == dungeons_.end())
            continue; // only dungeons that have been drawn (loaded) collide
        const auto& data = loaded->second->data();
        const auto& m = placement.world.m;
        auto cached = dungeon_world_.find(index);
        if (cached == dungeon_world_.end()) {
            DungeonWorld world;
            for (const auto& node : data.nodes) {
                std::vector<std::uint16_t> indices;
                auto base = std::uint16_t(world.vertices.size());
                if (node.collision_indices.empty() ||
                    world.vertices.size() + node.collision_vertices.size() > 65535)
                    continue;
                for (const auto& v : node.collision_vertices)
                    world.vertices.push_back(
                        {v[0] * m[0][0] + v[1] * m[1][0] + v[2] * m[2][0] + m[3][0],
                         v[0] * m[0][1] + v[1] * m[1][1] + v[2] * m[2][1] + m[3][1],
                         v[0] * m[0][2] + v[1] * m[1][2] + v[2] * m[2][2] + m[3][2]});
                for (auto i : node.collision_indices)
                    indices.push_back(std::uint16_t(base + i));
                world.indices.push_back(std::move(indices));
            }
            cached = dungeon_world_.emplace(index, std::move(world)).first;
        }
        float dx = m[3][0] - px, dz = m[3][2] - pz;
        if (dx * dx + dz * dz > 800.f * 800.f)
            continue;
        for (const auto& indices : cached->second.indices)
            check_world_triangles(packet, cached->second.vertices, indices);
    }
}
void WorldObjects::draw(const std::array<float, 2>& center, float distance, const Matrix& root) {
    drawn_ = 0;
    for (const auto& placement : dungeon_placements_) {
        float dx = placement.world.m[3][0] - center[0], dz = placement.world.m[3][2] - center[1];
        if (dx * dx + dz * dz > (distance + 200.f) * (distance + 200.f))
            continue;
        auto found = dungeons_.find(placement.model);
        if (found == dungeons_.end()) {
            if (dungeon_failed_.count(placement.model))
                continue;
            try {
                found = dungeons_
                            .emplace(placement.model,
                                     std::make_unique<DungeonResource>(
                                         renderer_, archive_, dungeon_paths_[placement.model]))
                            .first;
            } catch (const std::exception& e) {
                std::fprintf(stderr, "World dungeon %s: %s\n",
                             dungeon_paths_[placement.model].c_str(), e.what());
                dungeon_failed_[placement.model] = true;
                continue;
            }
        }
        found->second->draw(multiply(placement.world, root));
        ++drawn_;
    }
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
