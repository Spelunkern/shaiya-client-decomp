#include "animation_pose.h"
#include "archive.h"
#include "fixtures/animation_pose_x86.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
using namespace genesis;
using Matrix = shaiya::graphics::native::Matrix;
namespace {
Matrix rest(float x = 0, float y = 0) {
    return {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {x, y, 0, 1}}};
}
void expect(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void near(float actual, float wanted, const char* message) {
    expect(std::abs(actual - wanted) < .0002f, message);
}
Animation skeleton() {
    Animation clip;
    clip.end = 10;
    clip.bones.resize(3);
    clip.bones[0].authored = rest();
    clip.bones[1].authored = rest(2, 3);
    clip.bones[1].parent = 0;
    clip.bones[2].authored = rest(2, 5);
    clip.bones[2].parent = 1;
    return clip;
}
} // namespace
int main(int argc, char** argv) {
    try {
        auto clip = skeleton();
        auto static_pose = AnimationPose(clip).sample(5);
        near(static_pose[2].m[3][0], 2, "Static global X reconstruction");
        near(static_pose[2].m[3][1], 5, "Static global Y reconstruction");
        clip.bones[0].rotations = {{0, {0, 0, 0, 1}}, {10, {0, 0, 1, 0}}};
        clip.bones[0].translations = {{0, {0, 0, 0}}, {10, {10, 0, 0}}};
        auto sampled = AnimationPose(clip).sample(5);
        near(sampled[0].m[0][1], 1, "Quaternion row-vector rotation");
        near(sampled[0].m[1][0], -1, "Quaternion handedness");
        near(sampled[1].m[3][0], 2, "Animated parent/local composition X");
        near(sampled[1].m[3][1], 2, "Animated parent/local composition Y");
        near(sampled[2].m[3][0], 0, "Multi-generation parent/local composition X");
        near(sampled[2].m[3][1], 2, "Multi-generation parent/local composition Y");
        for (std::size_t i = 0; i < animation_reference::rotation_frames.size(); ++i) {
            auto pose = AnimationPose(clip).sample(animation_reference::rotation_frames[i]);
            near(pose[0].m[0][0], animation_reference::rotation[i][0],
                 "Original x86 quaternion matrix XX");
            near(pose[0].m[0][1], animation_reference::rotation[i][1],
                 "Original x86 quaternion matrix XY");
            near(pose[0].m[1][0], animation_reference::rotation[i][2],
                 "Original x86 quaternion matrix YX");
        }
        // Opposite signs encode the same orientation; interpolation must take
        // the short arc and must not introduce a spinning intermediate pose.
        clip = skeleton();
        clip.bones[0].rotations = {{0, {0, 0, 0, 1}}, {10, {0, 0, 0, -1}}};
        sampled = AnimationPose(clip).sample(5);
        near(sampled[0].m[0][0], 1, "Antipodal quaternion shortest arc");
        // The last rotation interval uses clip end, while position uses its
        // next key's frame (00552a83 vs 00552b9e).
        clip = skeleton();
        clip.end = 20;
        clip.bones[0].rotations = {{0, {0, 0, 0, 1}}, {10, {0, 0, 1, 0}}};
        clip.bones[0].translations = {{0, {0, 0, 0}}, {10, {10, 0, 0}}};
        sampled = AnimationPose(clip).sample(5);
        near(sampled[0].m[0][0], std::sqrt(.5f), "Final rotation interval clip-end denominator");
        near(sampled[0].m[3][0], 5, "Translation key denominator");
        clip = skeleton();
        clip.end = 4;
        clip.bones[0].translations = {
            {0, {0, 0, 0}}, {1, {1, 0, 0}}, {2, {8, 0, 0}}, {3, {27, 0, 0}}, {4, {64, 0, 0}}};
        AnimationPose dense(clip);
        for (std::size_t i = 0; i < animation_reference::frames.size(); ++i)
            near(dense.sample(animation_reference::frames[i])[0].m[3][0],
                 animation_reference::translation[i], "Original x86 translation fixture");
        near(dense.sample(3.5f)[0].m[3][0], 36.5f, "Recovered dense-key interval selection");
        near(dense.sample(4)[0].m[3][0], 27, "Recovered last-frame key selection");
        clip = skeleton();
        clip.bones[0].translations = {{0, {0, 0, 0}}, {10, {10, 0, 0}}};
        near(AnimationPose(clip).sample(-1)[0].m[3][0], -1,
             "Recovered pre-first-key extrapolation");
        Animation source;
        source.end = 20;
        source.bones.resize(1);
        source.bones[0].authored = rest();
        source.bones[0].translations = {{0, {0, 0, 0}}, {20, {4, 0, 0}}};
        auto destination = source;
        destination.bones[0].authored = rest(10, 0);
        destination.bones[0].translations = {{0, {1000, 0, 0}}};
        AnimationPose moving(source), target(destination);
        for (unsigned i = 0; i < 3; ++i)
            for (unsigned j = 0; j < 5; ++j)
                near(moving
                         .blend_to_rest(float(i * 10), target,
                                        animation_reference::blend_phases[j])[0]
                         .m[3][0],
                     animation_reference::blend_translation[i][j],
                     "Original x86 blend toward destination rest, ignoring target key");
        source.bones[0].translations.clear();
        source.bones[0].rotations = {{0, {0, 0, 0, 1}}};
        destination.bones[0].authored = rest(10, 0);
        destination.bones[0].authored.m[0][0] = -1;
        destination.bones[0].authored.m[1][1] = -1;
        AnimationPose identity_source(source), turned_target(destination);
        for (unsigned j = 0; j < 5; ++j) {
            auto pose = identity_source.blend_to_rest(0, turned_target,
                                                      animation_reference::blend_phases[j]);
            near(pose[0].m[0][0], animation_reference::blend_rotation[j][0],
                 "Original x86 blend rotation XX");
            near(pose[0].m[0][1], animation_reference::blend_rotation[j][1],
                 "Original x86 blend rotation XY");
            near(pose[0].m[1][0], animation_reference::blend_rotation[j][2],
                 "Original x86 blend rotation YX");
        }
        auto hierarchy = skeleton(), other = hierarchy;
        other.bones[0].authored = rest(10, 0);
        other.bones[1].authored = rest(12, 3);
        other.bones[2].authored = rest(12, 5);
        auto blended = AnimationPose(hierarchy).blend_to_rest(0, AnimationPose(other), 2.5f);
        near(blended[2].m[3][0], 7, "Blend composed local hierarchy twice");
        near(blended[2].m[3][1], 5, "Blend changed child local offset");
        bool incompatible = false;
        try {
            (void)moving.blend_to_rest(0, AnimationPose(Animation{}), 1);
        } catch (const std::runtime_error&) {
            incompatible = true;
        }
        expect(incompatible, "Unmatched target bones were silently invented");
        bool failed = false;
        clip = skeleton();
        clip.bones[1].authored = {};
        try {
            AnimationPose invalid(clip);
        } catch (const std::runtime_error&) {
            failed = true;
        }
        expect(failed, "Singular rest matrix accepted");
        if (argc > 1) {
            const std::filesystem::path root(argv[1]);
            auto archive = shaiya::portable::Archive::open(root / "data.sah", root / "data.saf");
            AnimationPose idle(
                decode_animation(archive.read("data/Character/Human/ANI/humf_016_idle1.ANI")));
            for (int frame = 0; frame <= 92; ++frame)
                expect(idle.sample(float(frame)).size() == 36, "Original idle pose size");
            auto first = idle.sample(0), later = idle.sample(40);
            bool changed = false;
            for (std::size_t i = 0; i < first.size(); ++i)
                for (unsigned row = 0; row < 4; ++row)
                    for (unsigned column = 0; column < 4; ++column)
                        if (std::abs(first[i].m[row][column] - later[i].m[row][column]) > .001f)
                            changed = true;
            expect(changed, "Original idle channels did not move any bones");
        }
        std::puts("Native ANI pose hierarchy/interpolation and original idle sampling passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
