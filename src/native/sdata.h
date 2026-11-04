#pragma once
#include <cstdint>
#include <vector>
namespace genesis {
// Original client SData envelope: 64-byte header, SEED ECB, CRC32 of plaintext.
std::vector<std::uint8_t> decode_sdata(const std::vector<std::uint8_t>& bytes);
} // namespace genesis
