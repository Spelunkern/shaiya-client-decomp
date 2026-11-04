#pragma once
#include "packet.h"
#include "crypto/bootstrap.h"
#include <optional>

namespace genesis::net {
class Crypto {
  public:
    Crypto() = default;
    Crypto(const Crypto&) = delete;
    Crypto& operator=(const Crypto&) = delete;
    Crypto(Crypto&& other) noexcept;
    Crypto& operator=(Crypto&& other) noexcept;
    ~Crypto();
    // A101 body is the fixed 1+1+1+64+128-byte recovered challenge layout.
    Bytes challenge_reply(const Bytes& challenge, std::uint32_t prng_seed);
    void aes_material(const std::array<std::uint8_t, 32>& material);
    void enable() {
        inbound_ = outbound_ = true;
    }
    void disable() {
        inbound_ = outbound_ = alternate_ = false;
    }
    void reset_nonce();
    void xor_seed(std::uint32_t mode, const std::array<std::uint8_t, 16>& seed);
    void alternate_inbound(bool enabled) {
        alternate_ = enabled;
    }
    void enter_world_mode(); // recovered 00401570
    void leave_world_mode(); // recovered 004015a0
    void outbound(void* data, std::size_t count);
    void inbound(void* data, std::size_t count);

  private:
    ClientCryptoState state_ = {};
    bool inbound_ = false, outbound_ = false, alternate_ = false;
    std::uint32_t mode_ = 0;
    Bytes xor_table_;
};
class Wire {
  public:
    Wire() = default;
    Wire(const Wire&) = delete;
    Wire& operator=(const Wire&) = delete;
    Wire(Wire&& other) noexcept;
    Wire& operator=(Wire&& other) noexcept;
    Crypto crypto;
    Bytes encode(const Packet& packet);
    void feed(const void* data, std::size_t count);
    // Decode one packet at a time: dispatch may change crypto before the next
    // packet already received in the same TCP read (A101 -> encrypted A102).
    std::optional<Packet> pop();
    void reset();

  private:
    Bytes received_;
    std::size_t offset_ = 0;
};
} // namespace genesis::net
