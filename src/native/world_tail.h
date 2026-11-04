#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace genesis {
// The part of a `.wld` descriptor shared by field and dungeon worlds, read by
// `004518a0` after the terrain/dungeon section: eight object placement lists,
// effect placements, object/animated lists and the environment sections. The
// section boundaries and record sizes follow the recovered readers; record
// payloads are retained as raw little-endian bytes because only the sizes are
// established by the loader's stream reads (their later use is by game code
// that is not native yet).
struct WorldSection {
    // Names are the 0x100-byte NUL-terminated strings preceding the records.
    std::vector<std::string> names;
    std::uint32_t record_size = 0;
    std::vector<std::vector<std::uint8_t>> records;
};
struct WorldTail {
    // 004462d0 x4, 004466c0 x2, 00446ab0 (record 40 bytes), 00420b30 (44 bytes).
    std::vector<WorldSection> placements;
    std::string effect_name;     // 0x100 bytes before the effect placements
    WorldSection effects;        // 40-byte records
    // 00536790: names, 76-byte records, then 152-byte records (pairs of 76).
    WorldSection objects, object_pairs;
    // 00537200: names and 40-byte records.
    WorldSection animated;
    // 0053ce70: two name lists with 36-byte records and a variable list, plus
    // 20-byte records.
    WorldSection shape_names, shape_records, route_names;
    std::vector<std::vector<std::uint8_t>> routes; // 0x18 bytes + counted 4-byte entries
    WorldSection markers;
    // 005388f0 sections.
    WorldSection lights, volumes;
    std::vector<std::vector<std::uint8_t>> zones;   // counted 36-byte entries per zone
    WorldSection triggers;
    std::vector<std::vector<std::uint8_t>> paths;   // 28-byte header + counted 12-byte points
    // Field worlds only: three 0x100-byte names before the final block.
    std::array<std::string, 3> field_names;
    // Final fixed block: three vec3 (36 bytes) and two words (8 bytes).
    std::array<std::uint8_t, 44> environment{};
    std::size_t bytes_consumed = 0;
};
// `offset` is where the shared stream starts: 0x104 for DUN descriptors, and
// after the 0x100-byte water name for FLD descriptors. Throws
// std::invalid_argument on truncation or implausible counts. The stream must
// end exactly at the end of the file, as every local descriptor does.
WorldTail decode_world_tail(const std::vector<std::uint8_t>& bytes, std::size_t offset,
                            bool field);
} // namespace genesis
