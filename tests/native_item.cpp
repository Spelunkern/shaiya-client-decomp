#include "item.h"
#include "character_equipment.h"
#include "sdata.h"
#include "archive.h"
#include <cstdio>
#include <filesystem>
#include <stdexcept>
using Bytes = std::vector<std::uint8_t>;
void expect(bool ok) {
    if (!ok)
        throw std::runtime_error("Item contract failed");
}
void u32(Bytes& b, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(std::uint8_t(n >> (8 * i)));
}
template <class F> void rejects(F fn) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    expect(rejected);
}
int main(int argc, char** argv) {
    try {
        Bytes b;
        u32(b, 2);
        u32(b, 1);
        u32(b, 3);
        b.insert(b.end(), {'A', 0xe9, 0});
        u32(b, 0);
        for (unsigned i = 0; i < 81; ++i)
            b.push_back(std::uint8_t(i));
        u32(b, 0);
        auto table = genesis::decode_items(b);
        auto item = table.find(1, 1);
        expect(item && item->name == std::string("A\xe9", 2) && item->description.empty());
        expect(item->model() == 2 && item->icon() == 3 && item->minimum_level() == 0x504);
        expect(item->payload[80] == 80 && !table.find(0, 1) && !table.find(1, 0) &&
               !table.find(2, 1) && !table.find(3, 1) && !table.find(1, 2));
        for (std::size_t i = 0; i < b.size(); ++i)
            rejects([&] { genesis::decode_items(Bytes(b.begin(), b.begin() + i)); });
        genesis::CharacterEquipment equipment;
        expect(item->cloth_variant() == 28);
        expect(!genesis::character_cloak(table, equipment));
        equipment.types[7] = 1;
        equipment.ids[7] = 1;
        auto cape = genesis::character_cloak(table, equipment);
        expect(cape && cape->model == 2 && cape->variant == 0);
        expect(!genesis::character_cloak(table, equipment, false));
        equipment.types[1] = 1;
        equipment.ids[1] = 1;
        for (unsigned value = 0; value < 256; ++value) {
            table.groups[0][0].payload[28] = std::uint8_t(value);
            auto selected = genesis::character_cloak(table, equipment);
            expect(selected && selected->variant == (value >= 1 && value <= 8 ? value - 1 : 0));
        }
        equipment.ids[7] = 255;
        cape = genesis::character_cloak(table, equipment);
        expect(cape && cape->model == 0 && cape->variant == 0);
        equipment = {};
        auto base = genesis::character_appearance(table, 7, 9, equipment);
        expect(!base.helmet && base.indices == std::array<std::uint32_t, 6>{7, 9, 0, 0, 0, 0});
        for (unsigned slot = 0; slot < 5; ++slot) {
            equipment.types[slot] = 1;
            equipment.ids[slot] = 1;
        }
        auto clothed = genesis::character_appearance(table, 7, 9, equipment);
        expect(clothed.helmet && clothed.indices == std::array<std::uint32_t, 6>{7, 2, 2, 2, 2, 2});
        auto hidden = genesis::character_appearance(table, 7, 9, equipment, false);
        expect(!hidden.helmet && hidden.indices == std::array<std::uint32_t, 6>{7, 9, 2, 2, 2, 2});
        equipment.ids.fill(255);
        auto missing = genesis::character_appearance(table, 7, 9, equipment);
        expect(missing.helmet && missing.indices == base.indices);
        equipment.types.fill(0);
        auto type_zero = genesis::character_appearance(table, 7, 9, equipment);
        expect(!type_zero.helmet && type_zero.indices == base.indices);
        b.push_back(0);
        rejects([&] { genesis::decode_items(b); });
        rejects([] { genesis::decode_sdata({}); });
        if (argc > 1) {
            std::filesystem::path root(argv[1]);
            auto archive = shaiya::portable::Archive::open(root / "data.sah", root / "data.saf");
            auto encrypted = archive.read("data/Item/Item.SData");
            auto plain = genesis::decode_sdata(encrypted);
            auto all = genesis::decode_items(plain);
            expect(all.groups.size() == 100);
            std::size_t rows = 0;
            for (std::size_t g = 0; g < all.groups.size(); ++g) {
                for (std::size_t i = 0; i < all.groups[g].size(); ++i) {
                    auto p = all.find(std::uint8_t(g + 1), std::uint8_t(i + 1));
                    expect(p && p->payload[0] == g + 1 && p->payload[1] == i + 1);
                    ++rows;
                }
            }
            expect(rows == 22073);
            for (auto at : {std::size_t(0), std::size_t(40), std::size_t(44), std::size_t(64),
                            encrypted.size() - 16}) {
                auto bad = encrypted;
                bad[at] ^= 0x80;
                rejects([&] { genesis::decode_sdata(bad); });
            }
            encrypted.pop_back();
            rejects([&] { genesis::decode_sdata(encrypted); });
            std::printf("Client Item.SData: %zu groups, %zu items, CRC verified\n",
                        all.groups.size(), rows);
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
