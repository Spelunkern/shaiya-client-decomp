#include "character_profile.h"
#include "action.h"
#include "archive.h"
#include "cloth.h"
#include "asset_path.h"
#include <cstdio>
#include <filesystem>
#include <stdexcept>
int main(int argc, char** argv) {
    try {
        const char* expected[4][2][2] = {
            {{"Human/humf", "Human/humm"}, {"Human/huwf", "Human/huwm"}},
            {{"Elf/elmr", "Elf/elmm"}, {"Elf/elwr", "Elf/elwm"}},
            {{"Vile/vimr", "Vile/vimm"}, {"Vile/viwr", "Vile/viwm"}},
            {{"DeathEater/demf", "DeathEater/demr"}, {"DeathEater/dewf", "DeathEater/dewr"}}};
        // These branch groups are the actual 0041760c jump table order.
        for (unsigned race = 0; race < 4; ++race)
            for (unsigned job = 0; job < 256; ++job)
                for (unsigned gender = 0; gender < 3; ++gender) {
                    bool alternate;
                    switch (race) {
                    case 0:
                    case 3:
                        alternate = job != 0 && job != 1;
                        break;
                    case 1:
                        alternate = job != 2 && job != 3;
                        break;
                    default:
                        alternate = job == 4 || job == 5;
                    }
                    static constexpr unsigned kind_base[] = {0, 4, 12, 8};
                    if (genesis::character_kind(std::uint8_t(race), std::uint8_t(job),
                                                std::uint8_t(gender)) !=
                        kind_base[race] + 2 * (gender != 0) + unsigned(alternate))
                        throw std::runtime_error("Recovered numeric actor kind mismatch");
                    auto actual = genesis::character_profile(std::uint8_t(race), std::uint8_t(job),
                                                             std::uint8_t(gender));
                    if (actual !=
                        std::string("data/Character/") + expected[race][gender != 0][alternate])
                        throw std::runtime_error("Recovered profile/archetype selector mismatch");
                    for (std::uint8_t variant = 0; variant < 8; ++variant) {
                        auto prefix = std::string(expected[race][gender != 0][alternate]);
                        auto slash = prefix.find('/');
                        auto expected_pc = "data/cloak/Character/" + prefix.substr(0, slash + 1) +
                                           std::to_string(unsigned(variant) + 1) + "_" +
                                           prefix.substr(slash + 1) + ".pc";
                        if (genesis::character_cloth_catalog(race, job, gender, variant) !=
                            expected_pc)
                            throw std::runtime_error("Recovered cloth catalog selector mismatch");
                    }
                }
        bool rejected = false;
        try {
            (void)genesis::character_profile(4, 0, 0);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        if (!rejected)
            throw std::runtime_error("Invalid profile race accepted");
        if (argc > 1) {
            const std::filesystem::path root(argv[1]);
            auto archive = shaiya::portable::Archive::open(root / "data.sah", root / "data.saf");
            for (const auto& race : expected)
                for (const auto& gender : race)
                    for (auto prefix : gender) {
                        auto path = std::string("data/Character/") + prefix;
                        auto directory = genesis::asset_path::parent(path);
                        directory.insert(5, "cloak/");
                        for (unsigned variant = 1; variant <= 8; ++variant) {
                            auto pc = genesis::asset_path::join(
                                directory, std::to_string(variant) + "_" +
                                               genesis::asset_path::stem(path) + ".pc");
                            auto catalog = genesis::decode_cloth_catalog(archive.read(pc));
                            if (catalog.simulation.empty())
                                throw std::runtime_error("Missing cloth variant setups");
                        }
                        auto table = genesis::decode_actions(archive.read(path + "_action.alt"));
                        if (table.slots.empty() || table.slots[0].mode() != 1)
                            throw std::runtime_error("Missing original normal-action profile");
                        for (const auto* part : {"face", "hair", "upper", "lower", "hand", "foot"})
                            if (!archive.find(path + "_" + part + ".MLT"))
                                throw std::runtime_error("Missing original profile part table");
                    }
        }
        std::puts("Native character profile/archetype branches and 16 resource sets passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
