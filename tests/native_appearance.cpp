#include "appearance.h"
#include "archive.h"
#include <cstdio>
#include <filesystem>
#include <stdexcept>
namespace {
using Bytes = std::vector<std::uint8_t>;
void u32(Bytes& b, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(std::uint8_t(n >> (i * 8)));
}
void name(Bytes& b, const char* value) {
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
        (void)genesis::decode_appearance(b);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    expect(failed, "Malformed appearance accepted");
}
} // namespace
int main(int argc, char** argv) {
    try {
        Bytes b = {'M', 'L', '2'};
        u32(b, 1);
        name(b, "body.3DC");
        u32(b, 2);
        name(b, "body_a.dds");
        name(b, "body_b.dds");
        u32(b, 2);
        for (unsigned i = 0; i < 2; ++i) {
            u32(b, 0);
            u32(b, i);
            u32(b, i ? 0x12345678 : 0);
        }
        auto table = genesis::decode_appearance(b);
        expect(table.models == std::vector<std::string>{"body.3DC"} && table.textures.size() == 2 &&
                   table.appearances[1].model == 0 && table.appearances[1].texture == 1 &&
                   table.appearances[1].flags == 0x12345678,
               "Appearance catalogs/aliases/flags");
        for (std::size_t i = 0; i < b.size(); ++i)
            rejects(Bytes(b.begin(), b.begin() + i));
        auto bad = b;
        bad.push_back(0);
        rejects(bad);
        bad = b;
        bad[2] = '1';
        rejects(bad);
        bad = b;
        bad[bad.size() - 12] = 1;
        rejects(bad);
        bad = b;
        bad[bad.size() - 8] = 2;
        rejects(bad);
        bad = b;
        bad[11] = 0;
        rejects(bad);
        if (argc > 1) {
            const std::filesystem::path root(argv[1]);
            auto archive = shaiya::portable::Archive::open(root / "data.sah", root / "data.saf");
            auto upper =
                genesis::decode_appearance(archive.read("data/Character/Human/humf_upper.MLT"));
            expect(upper.models.size() == 14 && upper.textures.size() == 28 &&
                       upper.appearances.size() == 28 && upper.models[0] == "humf_torso001.3DC" &&
                       upper.appearances[24].model == 10 && upper.appearances[24].texture == 24,
                   "Original ML2 appearance mapping");
        }
        std::puts("Native ML2 model/texture appearance catalogs passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
