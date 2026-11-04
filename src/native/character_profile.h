#pragma once
#include <cstdint>
#include <string>
namespace genesis {
// 00417220 computes the archetype bit; 00552400's actual jump table maps
// races to Human, Elf, Vile, DeathEater (not source-code block order).
// 004103a0: numeric actor kind uses Human/Elf/DeathEater/Vile block order.
std::uint8_t character_kind(std::uint8_t race, std::uint8_t job, std::uint8_t gender);
std::string character_profile(std::uint8_t race, std::uint8_t job, std::uint8_t gender);
// 00585b40: same gender/archetype branches, one-based PC filename prefix.
std::string character_cloth_catalog(std::uint8_t race, std::uint8_t job, std::uint8_t gender,
                                    std::uint8_t variant);
} // namespace genesis
