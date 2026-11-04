#include "wire.h"
#include "crypto/sha256.h"
#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

namespace genesis::net {
namespace {
void bounded(std::size_t count) {
    if (count > 65533)
        throw std::invalid_argument("Transport payload exceeds wire length");
}
} // namespace
Crypto::~Crypto() {
    OPENSSL_cleanse(&state_, sizeof(state_));
}
Crypto::Crypto(Crypto&& other) noexcept {
    *this = std::move(other);
}
Crypto& Crypto::operator=(Crypto&& other) noexcept {
    if (this == &other)
        return *this;
    OPENSSL_cleanse(&state_, sizeof(state_));
    state_ = other.state_;
    inbound_ = other.inbound_;
    outbound_ = other.outbound_;
    alternate_ = other.alternate_;
    mode_ = other.mode_;
    xor_table_ = std::move(other.xor_table_);
    other.disable();
    other.mode_ = 0;
    OPENSSL_cleanse(&other.state_, sizeof(other.state_));
    return *this;
}
Bytes Crypto::challenge_reply(const Bytes& challenge, std::uint32_t prng_seed) {
    Reader r(challenge);
    auto algorithm = r.u8(), exponent_bytes = r.u8(), modulus_bytes = r.u8();
    auto exponent = r.fixed<64>();
    auto modulus = r.fixed<128>();
    if (r.remaining() || !exponent_bytes || exponent_bytes > 64 || !modulus_bytes ||
        modulus_bytes > 128)
        throw std::runtime_error("Invalid A101 public key lengths");
    using Big = std::unique_ptr<BIGNUM, decltype(&BN_clear_free)>;
    using Context = std::unique_ptr<BN_CTX, decltype(&BN_CTX_free)>;
    Big e(BN_lebin2bn(exponent.data(), exponent_bytes, nullptr), BN_clear_free);
    Big n(BN_lebin2bn(modulus.data(), modulus_bytes, nullptr), BN_clear_free);
    Big result(BN_new(), BN_clear_free);
    Context context(BN_CTX_new(), BN_CTX_free);
    if (!e || !n || !result || !context)
        throw std::runtime_error("RSA allocation failed");
    if (BN_is_zero(e.get()) || BN_is_zero(n.get()) || BN_is_one(n.get()) || !BN_is_odd(n.get()))
        throw std::runtime_error("Invalid A101 RSA public key");
    ClientCryptoState next = {};
    next.prng_seed = prng_seed;
    std::array<std::uint8_t, 128> random = {};
    client_crypto_initialize_a101(&next, algorithm, modulus.data(), modulus_bytes, random.data());
    Big base(BN_lebin2bn(random.data(), static_cast<int>(random.size()), nullptr), BN_clear_free);
    OPENSSL_cleanse(random.data(), random.size());
    if (!base || BN_mod_exp(result.get(), base.get(), e.get(), n.get(), context.get()) != 1)
        throw std::runtime_error("A101 RSA public operation failed");
    // The recovered exporter rounds to 32-bit limbs and always transmits a
    // 128-byte reply area. Define unused padding instead of leaking stack bytes.
    const int bytes = ((BN_num_bytes(result.get()) + 3) / 4) * 4;
    if (bytes > 128)
        throw std::runtime_error("A101 RSA reply exceeds 128 bytes");
    Bytes reply(129, 0);
    reply[0] = static_cast<std::uint8_t>(bytes);
    if (BN_bn2lebinpad(result.get(), reply.data() + 1, 128) != 128)
        throw std::runtime_error("Cannot encode A101 RSA result");
    state_ = next;
    OPENSSL_cleanse(&next, sizeof(next));
    return reply;
}
void Crypto::aes_material(const std::array<std::uint8_t, 32>& material) {
    state_ = {};
    aes_expand_key(&state_.inbound_aes.schedule, material.data(), 128);
    aes_expand_key(&state_.outbound_aes.schedule, material.data(), 128);
    std::memcpy(state_.inbound_aes.counter, material.data() + 16, 16);
    std::memcpy(state_.outbound_aes.counter, material.data() + 16, 16);
    std::copy_n(material.data() + 16, 16, state_.reconnect_nonce);
    enable();
}
void Crypto::outbound(void* data, std::size_t count) {
    bounded(count);
    if (!outbound_)
        return;
    if (state_.algorithm == 0)
        aes_transform_ctr(&state_.outbound_aes, data, data, static_cast<std::uint32_t>(count));
    else
        lcg_transform_stream(&state_.outbound_lcg, data, data, static_cast<std::uint32_t>(count));
}
void Crypto::inbound(void* data, std::size_t count) {
    bounded(count);
    if (alternate_) {
        if (mode_ != 2)
            return;
        // Only the known 4096-byte original table is defined. Larger mode-two
        // payloads depended on adjacent x86 storage and require separate recovery.
        if (count > xor_table_.size() / 2)
            throw std::runtime_error("Mode-two packet exceeds recovered XOR table");
        auto* bytes = static_cast<std::uint8_t*>(data);
        for (std::size_t i = 0; i < count; ++i)
            bytes[i] ^= xor_table_[count + i];
    } else if (inbound_) {
        if (state_.algorithm == 0)
            aes_transform_ctr(&state_.inbound_aes, data, data, static_cast<std::uint32_t>(count));
        else
            lcg_transform_stream(&state_.inbound_lcg, data, data,
                                 static_cast<std::uint32_t>(count));
    }
}
void Crypto::xor_seed(std::uint32_t mode, const std::array<std::uint8_t, 16>& seed) {
    mode_ = mode;
    xor_table_.resize(4096);
    sha256_digest(xor_table_.data(), seed.data(), 16);
    for (std::size_t offset = 32; offset < xor_table_.size(); offset += 32)
        sha256_digest(xor_table_.data() + offset, xor_table_.data() + offset - 16, 16);
    if (!mode)
        inbound_ = false;
}
void Crypto::reset_nonce() {
    std::array<std::uint8_t, 32> digest;
    sha256_digest(digest.data(), state_.reconnect_nonce, 16);
    if (state_.algorithm == 0) {
        std::memcpy(state_.outbound_aes.counter, digest.data(), 16);
        std::memcpy(state_.inbound_aes.counter, digest.data(), 16);
        state_.outbound_aes.position = state_.inbound_aes.position = 0;
    } else {
        std::array<std::uint8_t, 32> material;
        std::copy_n(state_.lcg_key, 16, material.begin());
        std::copy_n(digest.data(), 16, material.begin() + 16);
        std::memcpy(state_.outbound_lcg.seeds, material.data(), 32);
        std::memcpy(state_.inbound_lcg.seeds, material.data(), 32);
        lcg_generate_block(state_.outbound_lcg.seeds, state_.outbound_lcg.stream);
        lcg_generate_block(state_.inbound_lcg.seeds, state_.inbound_lcg.stream);
        state_.outbound_lcg.position = state_.inbound_lcg.position = 0;
        OPENSSL_cleanse(material.data(), material.size());
    }
    OPENSSL_cleanse(state_.reconnect_nonce, 16);
    OPENSSL_cleanse(state_.lcg_key, 16);
    OPENSSL_cleanse(digest.data(), digest.size());
}
void Crypto::enter_world_mode() {
    if (mode_ <= 1)
        inbound_ = false;
    else if (mode_ != 3)
        alternate_ = true;
}
void Crypto::leave_world_mode() {
    alternate_ = false;
    inbound_ = mode_ != 0;
}
Bytes Wire::encode(const Packet& packet) {
    if (packet.body.size() > 65531)
        throw std::invalid_argument("Packet exceeds 16-bit frame length");
    Writer w;
    w.u16(static_cast<std::uint16_t>(packet.body.size() + 4));
    w.u16(packet.opcode);
    w.append(packet.body.data(), packet.body.size());
    auto bytes = w.finish();
    crypto.outbound(bytes.data() + 2, bytes.size() - 2);
    return bytes;
}
Wire::Wire(Wire&& other) noexcept
    : crypto(std::move(other.crypto)), received_(std::move(other.received_)),
      offset_(std::exchange(other.offset_, 0)) {}
Wire& Wire::operator=(Wire&& other) noexcept {
    if (this == &other)
        return *this;
    crypto = std::move(other.crypto);
    received_ = std::move(other.received_);
    offset_ = std::exchange(other.offset_, 0);
    return *this;
}
void Wire::feed(const void* data, std::size_t count) {
    constexpr std::size_t limit = 1024 * 1024;
    if (count > limit || count > limit - (received_.size() - offset_))
        throw std::runtime_error("Receive queue limit exceeded");
    if (count && !data)
        throw std::invalid_argument("Null network input");
    if (offset_) {
        received_.erase(received_.begin(),
                        received_.begin() + static_cast<std::ptrdiff_t>(offset_));
        offset_ = 0;
    }
    if (count) {
        const auto* p = static_cast<const std::uint8_t*>(data);
        received_.insert(received_.end(), p, p + count);
    }
}
std::optional<Packet> Wire::pop() {
    const auto available = received_.size() - offset_;
    if (available < 2)
        return std::nullopt;
    const auto* p = received_.data() + offset_;
    auto length = std::uint16_t(p[0]) | std::uint16_t(p[1]) << 8;
    if (length < 4)
        throw std::runtime_error("Invalid network frame length");
    if (available < static_cast<std::size_t>(length))
        return std::nullopt;
    Bytes decoded(p + 2, p + length);
    crypto.inbound(decoded.data(), decoded.size());
    offset_ += length;
    Reader reader(decoded);
    Packet packet;
    packet.opcode = reader.u16();
    packet.body.assign(decoded.begin() + 2, decoded.end());
    return packet;
}
void Wire::reset() {
    received_.clear();
    offset_ = 0;
    crypto.disable();
}
} // namespace genesis::net
