#include "animation.h"
#include "archive.h"
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
namespace {
using Bytes = std::vector<std::uint8_t>;
void u32(Bytes& b, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(std::uint8_t(n >> (8 * i)));
}
void f32(Bytes& b, float f) {
    std::uint32_t bits;
    std::memcpy(&bits, &f, 4);
    u32(b, bits);
}
void expect(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void rejects(const Bytes& b) {
    bool failed = false;
    try {
        (void)genesis::decode_animation(b);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    expect(failed, "Malformed animation accepted");
}
Bytes fixture() {
    Bytes b;
    u32(b, 0xffffffffu);
    u32(b, 10);
    b.insert(b.end(), {2, 0});
    for (unsigned bone = 0; bone < 2; ++bone) {
        u32(b, bone ? 0 : 0xffffffffu);
        for (unsigned row = 0; row < 4; ++row)
            for (unsigned column = 0; column < 4; ++column)
                f32(b, row == 3 && column == 1 ? float(bone + 2) : (row == column ? 1.f : 0.f));
        u32(b, bone ? 0 : 2);
        if (!bone)
            for (auto frame : {0xffffffffu, 10u}) {
                u32(b, frame);
                for (float value : {0.f, 0.f, 0.f, 1.f})
                    f32(b, value);
            }
        u32(b, bone ? 0 : 1);
        if (!bone) {
            u32(b, 5);
            for (float value : {1.f, 2.f, 3.f})
                f32(b, value);
        }
    }
    return b;
}
} // namespace
int main(int argc, char** argv) {
    try {
        auto b = fixture();
        auto clip = genesis::decode_animation(b);
        expect(clip.start == -1 && clip.end == 10 && clip.bones.size() == 2,
               "Signed animation range");
        expect(clip.bones[0].parent == -1 && clip.bones[1].parent == 0, "Animation hierarchy");
        expect(clip.bones[1].authored.m[3][1] == 3 && clip.bones[1].authored.m[1][3] == 0,
               "Authored row-vector matrix");
        expect(clip.bones[0].rotations.size() == 2 && clip.bones[0].rotations[0].frame == -1 &&
                   clip.bones[0].rotations[1].value[3] == 1,
               "Quaternion channel");
        expect(clip.bones[0].translations[0].frame == 5 &&
                   clip.bones[0].translations[0].value[2] == 3 && clip.bones[1].rotations.empty() &&
                   clip.bones[1].translations.empty(),
               "Independent/static animation channels");
        for (std::size_t size = 0; size < b.size(); ++size)
            rejects(Bytes(b.begin(), b.begin() + size));
        auto bad = b;
        bad.push_back(0);
        rejects(bad);
        bad = b;
        for (unsigned i = 0; i < 4; ++i)
            bad[10 + i] = 0;
        rejects(bad); // Self-parent cycle
        bad = b;
        bad[10] = 2;
        bad[11] = bad[12] = bad[13] = 0;
        rejects(bad); // Missing parent
        bad = b;
        bad[10] = 1;
        bad[11] = bad[12] = bad[13] = 0;
        rejects(bad); // Two-node cycle
        bad = b;
        bad[102] = 0xfe;
        bad[103] = bad[104] = bad[105] = 0xff;
        rejects(bad); // Decreasing signed frames
        bad = b;
        bad[16] = 0x80;
        bad[17] = 0x7f;
        rejects(bad); // Non-finite matrix
        bad = b;
        bad[78] = bad[79] = bad[80] = bad[81] = 0xff;
        rejects(bad); // Oversized key payload
        // Forward references are retained if the hierarchy is acyclic.
        bad = b;
        bad[10] = 1;
        bad[11] = bad[12] = bad[13] = 0;
        for (unsigned i = 0; i < 4; ++i)
            bad[142 + i] = 0xff;
        expect(genesis::decode_animation(bad).bones[0].parent == 1,
               "Acyclic forward parent rejected");
        bad = b;
        bad[10] = 0xfe;
        expect(genesis::decode_animation(bad).bones[0].parent == -2,
               "Client's negative root parent contract");
        if (argc > 1) {
            const std::filesystem::path root(argv[1]);
            auto archive = shaiya::portable::Archive::open(root / "data.sah", root / "data.saf");
            auto idle = genesis::decode_animation(
                archive.read("data/Character/Human/ANI/humf_016_idle1.ANI"));
            expect(idle.bones.size() == 36 && idle.start == 0 && idle.end == 92,
                   "Original human idle ANI");
            auto demonic =
                genesis::decode_animation(archive.read("data/Monster/ANI/Mob_Demonic_idle.ANI"));
            expect(demonic.start == -1 && demonic.end == 86 && demonic.bones.size() == 59,
                   "Original signed sentinel ANI");
        }
        std::puts("Native ANI hierarchy, authored matrices and signed key channels passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
