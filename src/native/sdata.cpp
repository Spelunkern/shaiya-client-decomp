#include "sdata.h"
#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/seed.h>
#include <cstring>
#include <stdexcept>
namespace genesis {
namespace {
std::uint32_t u32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 |
           std::uint32_t(p[3]) << 24;
}
// Recovered expanded schedule at 0070cf38, used by 005c135f / 005c6220.
constexpr std::uint32_t key[] = {
    0x79f5dbde, 0x345ac74a, 0x0f482438, 0x0131f493, 0x81a8500c, 0x0659bdcf, 0x26ff71c1, 0x86e9a5cb,
    0xca6fb745, 0x50e2c1ae, 0x381ddae1, 0xc3402821, 0x3feccb4a, 0x3e0be066, 0x372582ff, 0x826317e3,
    0xa47b5369, 0xc9093c0e, 0xe16c9cb1, 0x2e27228e, 0x84e2d1cd, 0xc840f818, 0x44aea6f8, 0xd298548d,
    0x2040ca27, 0x4b4e2b78, 0x64f0a045, 0xc171a8da, 0x384855ed, 0xc033578b, 0x2a8703c7, 0xf15da3a7};
} // namespace
std::vector<std::uint8_t> decode_sdata(const std::vector<std::uint8_t>& bytes) {
    constexpr char signature[] = "0001CBCEBC5B2784D3FC9A2A9DB84D1C3FEB6E99";
    if (bytes.size() < 80 || std::memcmp(bytes.data(), signature, 40) || (bytes.size() - 64) % 16)
        throw std::runtime_error("Invalid SData envelope");
    auto size = u32(bytes.data() + 44);
    if (!size || size > bytes.size() - 64 || bytes.size() - 64 - size >= 16)
        throw std::runtime_error("Invalid SData plaintext size");
    SEED_KEY_SCHEDULE schedule{};
    for (unsigned i = 0; i < 32; ++i)
        schedule.data[i] = key[i];
    std::vector<std::uint8_t> result(bytes.size() - 64);
    for (std::size_t i = 0; i < result.size(); i += 16)
        SEED_decrypt(bytes.data() + 64 + i, result.data() + i, &schedule);
    result.resize(size);
    std::uint32_t crc = 0xffffffff;
    for (auto byte : result) {
        crc ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    if ((crc ^ 0xffffffff) != u32(bytes.data() + 40))
        throw std::runtime_error("SData checksum mismatch");
    return result;
}
} // namespace genesis
