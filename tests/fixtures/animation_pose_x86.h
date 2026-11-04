#pragma once
#include <array>
namespace animation_reference {
inline constexpr std::array<float, 5> blend_phases = {0, 1, 2.5f, 4, 5};
inline constexpr std::array<std::array<float, 5>, 3> blend_translation = {
    {{0, 2, 5, 8, 10}, {2, 3.5999999f, 6, 8.39999962f, 10}, {4, 5.19999981f, 7, 8.80000019f, 10}}};
inline constexpr std::array<std::array<float, 3>, 5> blend_rotation = {
    {{1, 0, 0},
     {.809017003f, .587785304f, -.587785304f},
     {3.42285418e-8f, .99999994f, -.99999994f},
     {-.809017062f, .587785244f, -.587785244f},
     {-1, 0, 0}}};
// Captured from the original 00552910/00552cb0 x86 instruction streams, with
// their 005e5adf/005e6789 matrix conversions and 005e6d56 SLERP. Regenerate
// using tools/generate-animation-reference.py and tests/reference harness.
inline constexpr std::array<float, 7> frames = {-1, 0, 1.5f, 2.5f, 3.5f, 4, 5};
inline constexpr std::array<float, 7> translation = {-1, 0, 1.5f, 11.5f, 36.5f, 27, 64};
inline constexpr std::array<float, 4> rotation_frames = {0, 2.5f, 5, 10};
inline constexpr std::array<std::array<float, 3>, 4> rotation = {
    {{1, 0, 0},
     {.707106769f, .707106829f, -.707106829f},
     {3.42285418e-8f, .99999994f, -.99999994f},
     {-1, 0, 0}}};
} // namespace animation_reference
