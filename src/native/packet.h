#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace genesis::net {
using Bytes = std::vector<std::uint8_t>;
struct Packet {
    std::uint16_t opcode = 0;
    Bytes body;
};
class Reader {
  public:
    explicit Reader(const Bytes& data) : data_(data) {}
    std::size_t remaining() const {
        return data_.size() - offset_;
    }
    const std::uint8_t* take(std::size_t count) {
        if (count > remaining())
            throw std::runtime_error("Truncated network packet");
        auto pointer = data_.data() + offset_;
        offset_ += count;
        return pointer;
    }
    std::uint8_t u8() {
        return *take(1);
    }
    std::uint16_t u16() {
        auto p = take(2);
        return std::uint16_t(p[0]) | std::uint16_t(p[1]) << 8;
    }
    std::uint32_t u32() {
        auto p = take(4);
        return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 |
               std::uint32_t(p[3]) << 24;
    }
    template <std::size_t N> std::array<std::uint8_t, N> fixed() {
        std::array<std::uint8_t, N> result;
        auto p = take(N);
        std::copy_n(p, N, result.begin());
        return result;
    }
    std::string text(std::size_t size) {
        const auto* p = take(size);
        std::size_t length = 0;
        while (length < size && p[length])
            ++length;
        return {reinterpret_cast<const char*>(p), length};
    }

  private:
    const Bytes& data_;
    std::size_t offset_ = 0;
};
class Writer {
  public:
    void u8(std::uint8_t value) {
        bytes_.push_back(value);
    }
    void u16(std::uint16_t value) {
        u8(std::uint8_t(value));
        u8(std::uint8_t(value >> 8));
    }
    void u32(std::uint32_t value) {
        u16(std::uint16_t(value));
        u16(std::uint16_t(value >> 16));
    }
    void append(const void* data, std::size_t count) {
        if (count) {
            const auto* p = static_cast<const std::uint8_t*>(data);
            bytes_.insert(bytes_.end(), p, p + count);
        }
    }
    void text(std::string_view text, std::size_t capacity) {
        if (text.size() > capacity || text.find('\0') != std::string_view::npos)
            throw std::invalid_argument("Network text exceeds field capacity or contains NUL");
        append(text.data(), text.size());
        bytes_.insert(bytes_.end(), capacity - text.size(), 0);
    }
    Bytes finish() {
        return std::move(bytes_);
    }

  private:
    Bytes bytes_;
};
} // namespace genesis::net
