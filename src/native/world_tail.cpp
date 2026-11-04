#include "world_tail.h"
#include <algorithm>
#include <stdexcept>

namespace genesis {
namespace {
class Reader {
  public:
    Reader(const std::vector<std::uint8_t>& bytes, std::size_t offset)
        : bytes_(bytes), at_(offset) {
        if (offset > bytes.size())
            throw std::invalid_argument("World tail offset past the end");
    }
    std::size_t position() const {
        return at_;
    }
    const std::uint8_t* take(std::size_t n) {
        if (n > bytes_.size() - at_)
            throw std::invalid_argument("Truncated world descriptor");
        auto* p = bytes_.data() + at_;
        at_ += n;
        return p;
    }
    std::uint32_t count() {
        auto p = take(4);
        std::uint32_t v = p[0] | p[1] << 8 | p[2] << 16 | std::uint32_t(p[3]) << 24;
        if (v > (1u << 20))
            throw std::invalid_argument("Implausible world list count");
        return v;
    }
    std::string name() {
        auto p = take(0x100);
        auto end = std::find(p, p + 0x100, 0);
        if (end == p + 0x100)
            throw std::invalid_argument("Unterminated world list name");
        return std::string(p, end);
    }
    std::vector<std::uint8_t> bytes(std::size_t n) {
        auto p = take(n);
        return std::vector<std::uint8_t>(p, p + n);
    }
    void names(WorldSection& section) {
        auto n = count();
        for (std::uint32_t i = 0; i < n; ++i)
            section.names.push_back(name());
    }
    void records(WorldSection& section, std::uint32_t size) {
        section.record_size = size;
        auto n = count();
        for (std::uint32_t i = 0; i < n; ++i)
            section.records.push_back(bytes(size));
    }

  private:
    const std::vector<std::uint8_t>& bytes_;
    std::size_t at_;
};
} // namespace

WorldTail decode_world_tail(const std::vector<std::uint8_t>& bytes, std::size_t offset,
                            bool field) {
    Reader r(bytes, offset);
    WorldTail tail;
    for (unsigned i = 0; i < 8; ++i) {
        WorldSection section;
        r.names(section);
        r.records(section, i == 7 ? 44 : 40);
        tail.placements.push_back(std::move(section));
    }
    tail.effect_name = r.name();
    r.records(tail.effects, 40);
    r.names(tail.objects);
    r.records(tail.objects, 76);
    r.records(tail.object_pairs, 152);
    r.names(tail.animated);
    r.records(tail.animated, 40);
    r.names(tail.shape_names);
    r.records(tail.shape_records, 36);
    r.names(tail.route_names);
    {
        auto n = r.count();
        for (std::uint32_t i = 0; i < n; ++i) {
            auto route = r.bytes(0x18);
            auto entries = r.count();
            auto entry_bytes = r.bytes(std::size_t(entries) * 4);
            route.insert(route.end(), entry_bytes.begin(), entry_bytes.end());
            tail.routes.push_back(std::move(route));
        }
    }
    r.records(tail.markers, 20);
    r.records(tail.lights, 28);
    r.records(tail.volumes, 556);
    {
        auto n = r.count();
        for (std::uint32_t i = 0; i < n; ++i) {
            auto k = r.count();
            tail.zones.push_back(r.bytes(std::size_t(k) * 36));
        }
    }
    r.records(tail.triggers, 548);
    {
        // The loop bound shrinks by every record's inner count (005388f0).
        std::int64_t bound = r.count();
        for (std::int64_t i = 0; i < bound; ++i) {
            auto head = r.bytes(24);
            auto inner = r.count();
            auto points = r.bytes(std::size_t(inner) * 12);
            head.insert(head.end(), points.begin(), points.end());
            tail.paths.push_back(std::move(head));
            bound -= inner;
        }
    }
    if (field)
        for (auto& name : tail.field_names)
            name = r.name();
    auto environment = r.bytes(tail.environment.size());
    std::copy(environment.begin(), environment.end(), tail.environment.begin());
    tail.bytes_consumed = r.position();
    if (tail.bytes_consumed != bytes.size())
        throw std::invalid_argument("World descriptor has trailing or missing bytes");
    return tail;
}
} // namespace genesis
