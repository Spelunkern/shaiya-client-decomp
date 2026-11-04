#include "item.h"
#include <algorithm>
#include <stdexcept>
namespace genesis {
const ItemRecord* ItemTable::find(std::uint8_t type, std::uint8_t id) const {
    if (!type || !id || type > groups.size() || id > groups[type - 1].size())
        return nullptr;
    return &groups[type - 1][id - 1];
}
ItemTable decode_items(const std::vector<std::uint8_t>& bytes) {
    std::size_t offset = 0;
    auto take = [&](std::size_t size) {
        if (size > bytes.size() - offset)
            throw std::runtime_error("Truncated item table");
        auto p = bytes.data() + offset;
        offset += size;
        return p;
    };
    auto u32 = [&]() {
        auto p = take(4);
        return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 |
               std::uint32_t(p[3]) << 24;
    };
    auto text = [&]() {
        auto size = u32();
        auto p = take(size);
        if (size && p[size - 1] == 0)
            --size;
        return std::string(reinterpret_cast<const char*>(p), size);
    };
    ItemTable table;
    auto count = u32();
    if (!count || count > 255)
        throw std::runtime_error("Invalid item group count");
    table.groups.resize(count);
    // 00460ffc..0046132d consumes 81 bytes after the two length-prefixed strings.
    // Two later fields overwrite runtime offset 8; preserve ALL disk bytes here.
    for (auto& group : table.groups) {
        auto rows = u32();
        if (rows > 255 || rows > (bytes.size() - offset) / 89)
            throw std::runtime_error("Invalid item row count");
        group.reserve(rows);
        for (std::uint32_t i = 0; i < rows; ++i) {
            ItemRecord item;
            item.name = text();
            item.description = text();
            auto p = take(item.payload.size());
            std::copy_n(p, item.payload.size(), item.payload.begin());
            group.push_back(std::move(item));
        }
    }
    if (offset != bytes.size())
        throw std::runtime_error("Unconsumed item table records");
    return table;
}
} // namespace genesis
