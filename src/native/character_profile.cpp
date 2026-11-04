#include "character_profile.h"
#include "asset_path.h"
#include <stdexcept>
namespace genesis {
namespace {
bool alternate_role(std::uint8_t race, std::uint8_t job) {
    if (race > 3)
        throw std::invalid_argument("Unknown character race");
    return race == 1   ? (job != 2 && job != 3)
           : race == 2 ? (job == 4 || job == 5)
                       : (job != 0 && job != 1);
}
} // namespace
std::uint8_t character_kind(std::uint8_t race, std::uint8_t job, std::uint8_t gender) {
    auto alternate = alternate_role(race, job);
    static constexpr std::uint8_t base[] = {0, 4, 12, 8};
    return std::uint8_t(base[race] + (gender != 0 ? 2 : 0) + alternate);
}
std::string character_profile(std::uint8_t race, std::uint8_t job, std::uint8_t gender) {
    auto alternate = alternate_role(race, job);
    static const char* directories[] = {"Human", "Elf", "Vile", "DeathEater"};
    static const char* prefixes[4][2][2] = {{{"humf", "humm"}, {"huwf", "huwm"}},
                                            {{"elmr", "elmm"}, {"elwr", "elwm"}},
                                            {{"vimr", "vimm"}, {"viwr", "viwm"}},
                                            {{"demf", "demr"}, {"dewf", "dewr"}}};
    return std::string("data/Character/") + directories[race] + "/" +
           prefixes[race][gender != 0][alternate];
}
std::string character_cloth_catalog(std::uint8_t race, std::uint8_t job, std::uint8_t gender,
                                    std::uint8_t variant) {
    auto profile = character_profile(race, job, gender);
    auto directory = asset_path::parent(profile);
    directory.insert(5, "cloak/");
    return asset_path::join(directory, std::to_string(unsigned(variant) + 1) + "_" +
                                           asset_path::stem(profile) + ".pc");
}
} // namespace genesis
