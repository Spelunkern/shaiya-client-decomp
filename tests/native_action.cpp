#include "action.h"
#include "archive.h"
#include "asset_path.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
namespace {
using Bytes = std::vector<std::uint8_t>;
void u32(Bytes& b, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(std::uint8_t(n >> (8 * i)));
}
void text(Bytes& b, const char* value) {
    std::string s(value);
    u32(b, std::uint32_t(s.size() + 1));
    b.insert(b.end(), s.begin(), s.end());
    b.push_back(0);
}
void expect(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
void rejects(const Bytes& b) {
    bool failed = false;
    try {
        (void)genesis::decode_actions(b);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    expect(failed, "Malformed AL3 accepted");
}
Bytes fixture() {
    Bytes b = {'A', 'L', '3'};
    u32(b, 3);
    text(b, "idle.ANI");
    u32(b, 0x7301);
    u32(b, 0);
    u32(b, 0);
    u32(b, 0);
    u32(b, 0);
    u32(b, 0);
    u32(b, 0); // An empty slot has no metadata following its zero length.
    text(b, "attack.ANI");
    u32(b, 0xabcdef02);
    u32(b, 0);
    for (auto value : {0x3f800000u, 0x40000000u, 0x40400000u, 0x7fc00042u})
        u32(b, value);
    return b;
}
} // namespace
int main(int argc, char** argv) {
    try {
        const std::string opaque = "data/caf\xe9/obj\xe1.ALT";
        expect(genesis::asset_path::extension(opaque) == ".alt" &&
                   genesis::asset_path::parent(opaque) == "data/caf\xe9" &&
                   genesis::asset_path::stem(opaque) == "obj\xe1",
               "Archive paths underwent locale conversion");
        expect(genesis::asset_path::replace_extension(opaque, ".ANI") ==
                       "data/caf\xe9/obj\xe1.ANI" &&
                   genesis::asset_path::join("data/caf\xe9", "obj\xe1.ALT") == opaque,
               "Archive byte names were not preserved");
        expect(genesis::asset_path::extension("data/.hidden").empty() &&
                   genesis::asset_path::stem("data\\mesh.ANI") == "mesh",
               "Archive separator/dotfile handling");
        auto b = fixture();
        auto table = genesis::decode_actions(b);
        expect(table.slots.size() == 3 && table.slots[0].animation == "idle.ANI" &&
                   table.slots[1].animation.empty() && table.slots[2].animation == "attack.ANI",
               "Sparse action slot identity");
        expect(table.slots[0].control == 0x7301 && table.slots[0].mode() == 1 &&
                   table.slots[2].mode() == 2 && table.slots[2].next == 0,
               "Action mode masks padding instead of interpreting it");
        std::uint32_t bits;
        std::memcpy(&bits, &table.slots[2].parameters[3], 4);
        expect(table.slots[2].parameters[0] == 1 && table.slots[2].parameters[2] == 3 &&
                   bits == 0x7fc00042u,
               "Authored action parameter bits");
        for (std::size_t size = 0; size < b.size(); ++size)
            rejects(Bytes(b.begin(), b.begin() + size));
        auto bad = b;
        bad.push_back(0);
        rejects(bad);
        bad = b;
        bad[2] = '2';
        rejects(bad);
        bad = b;
        bad[7] = 1;
        bad[8] = 1;
        rejects(bad);
        bad = b;
        bad[11] = 0;
        rejects(bad);
        if (argc > 1) {
            const std::filesystem::path root(argv[1]);
            auto archive = shaiya::portable::Archive::open(root / "data.sah", root / "data.saf");
            auto human =
                genesis::decode_actions(archive.read("data/Character/Human/humf_action.alt"));
            expect(human.slots.size() == 126 && human.slots[0].animation == "humf_000_normal.ANI" &&
                       human.slots[0].control == 0x7301 &&
                       human.slots[1].animation == "humf_001_walk.ANI" &&
                       human.slots[16].animation == "humf_016_idle1.ANI",
                   "Original AL3 action selection");
            unsigned tables = 0, actions = 0;
            for (const auto& path : archive.paths()) {
                auto extension = genesis::asset_path::extension(path);
                for (auto& c : extension)
                    if (c >= 'A' && c <= 'Z')
                        c += 32;
                if (extension != ".alt")
                    continue;
                auto table = genesis::decode_actions(archive.read(path));
                ++tables;
                for (const auto& action : table.slots)
                    if (!action.animation.empty()) {
                        auto animation = genesis::asset_path::join(
                            genesis::asset_path::join(genesis::asset_path::parent(path), "ANI"),
                            action.animation);
                        expect(archive.find(animation), "AL3 points to missing ANI resource");
                        ++actions;
                    }
            }
            std::printf("Checked %u original AL3 tables and %u referenced animations\n", tables,
                        actions);
        }
        std::puts("Native AL3 sparse slots/control/parameter contracts passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
