#include "action.h"
#include <cstring>
#include <limits>
#include <stdexcept>
namespace genesis {
namespace {
class Decoder {
  public:
    explicit Decoder(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}
    ActionTable read() {
        if (std::memcmp(take(3), "AL3", 3))
            throw std::runtime_error("Unsupported action-table version");
        auto count = u32();
        require(std::size_t(count) * 4);
        ActionTable table;
        table.slots.resize(count);
        for (auto& action : table.slots) {
            auto length = u32();
            if (!length)
                continue;
            if (length > 256)
                throw std::runtime_error("Action name exceeds client capacity");
            auto p = take(length);
            auto size = length;
            if (p[size - 1] == 0)
                --size;
            action.animation.assign(reinterpret_cast<const char*>(p), size);
            if (action.animation.empty() || action.animation.find('\0') != std::string::npos)
                throw std::runtime_error("Invalid action animation name");
            action.control = u32();
            action.next = u32();
            for (auto& value : action.parameters) {
                static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
                auto bits = u32();
                std::memcpy(&value, &bits, 4);
            }
        }
        if (offset_ != bytes_.size())
            throw std::runtime_error("Unconsumed action-table records");
        return table;
    }

  private:
    void require(std::size_t size) const {
        if (size > bytes_.size() - offset_)
            throw std::runtime_error("Truncated action table");
    }
    const std::uint8_t* take(std::size_t size) {
        require(size);
        auto p = bytes_.data() + offset_;
        offset_ += size;
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
ActionTable decode_actions(const std::vector<std::uint8_t>& bytes) {
    return Decoder(bytes).read();
}
} // namespace genesis
