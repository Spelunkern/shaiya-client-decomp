#include "appearance.h"
#include <cstring>
#include <stdexcept>
namespace genesis {
namespace {
class Decoder {
  public:
    explicit Decoder(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}
    AppearanceTable read() {
        if (std::memcmp(take(3), "ML2", 3))
            throw std::runtime_error("Unsupported appearance table version");
        AppearanceTable table;
        table.models = names();
        table.textures = names();
        auto count = u32();
        require(std::size_t(count) * 12);
        table.appearances.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            AppearanceEntry entry{u32(), u32(), u32()};
            if (entry.model >= table.models.size() || entry.texture >= table.textures.size())
                throw std::runtime_error("Appearance index exceeds name catalog");
            table.appearances.push_back(entry);
        }
        if (offset_ != bytes_.size())
            throw std::runtime_error("Unconsumed appearance table records");
        return table;
    }

  private:
    std::vector<std::string> names() {
        auto count = u32();
        require(std::size_t(count) * 4);
        std::vector<std::string> result;
        result.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            auto size = u32();
            if (size > 256)
                throw std::runtime_error("Appearance name exceeds client capacity");
            auto p = take(size);
            auto length = size;
            if (length && p[length - 1] == 0)
                --length;
            std::string name(reinterpret_cast<const char*>(p), length);
            if (name.find('\0') != std::string::npos)
                throw std::runtime_error("Embedded NUL in appearance name");
            result.push_back(std::move(name));
        }
        return result;
    }
    void require(std::size_t count) const {
        if (count > bytes_.size() - offset_)
            throw std::runtime_error("Truncated appearance table");
    }
    const std::uint8_t* take(std::size_t count) {
        require(count);
        auto p = bytes_.data() + offset_;
        offset_ += count;
        return p;
    }
    std::uint32_t u32() {
        auto p = take(4);
        return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 |
               std::uint32_t(p[3]) << 24;
    }
    const std::vector<std::uint8_t>& bytes_;
    std::size_t offset_ = 0;
};
} // namespace
AppearanceTable decode_appearance(const std::vector<std::uint8_t>& bytes) {
    return Decoder(bytes).read();
}
} // namespace genesis
