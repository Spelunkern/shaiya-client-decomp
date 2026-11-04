#include "animation_pose.h"
#include "math.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace genesis {
namespace {
using Matrix = shaiya::graphics::native::Matrix;
Matrix inverse(const Matrix& matrix) {
    double rows[4][8] = {};
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j) {
            rows[i][j] = matrix.m[i][j];
            rows[i][j + 4] = i == j ? 1 : 0;
        }
    for (unsigned column = 0; column < 4; ++column) {
        unsigned pivot = column;
        for (unsigned row = column + 1; row < 4; ++row)
            if (std::abs(rows[row][column]) > std::abs(rows[pivot][column]))
                pivot = row;
        if (std::abs(rows[pivot][column]) < 1e-15)
            throw std::runtime_error("Singular animation rest matrix");
        for (unsigned j = 0; j < 8; ++j)
            std::swap(rows[column][j], rows[pivot][j]);
        auto divisor = rows[column][column];
        for (auto& value : rows[column])
            value /= divisor;
        for (unsigned row = 0; row < 4; ++row)
            if (row != column) {
                auto factor = rows[row][column];
                for (unsigned j = 0; j < 8; ++j)
                    rows[row][j] -= factor * rows[column][j];
            }
    }
    Matrix result{};
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j)
            result.m[i][j] = float(rows[i][j + 4]);
    return result;
}
template <std::size_t N>
std::size_t key_index(const std::vector<AnimationKey<N>>& keys, float frame) {
    // 005529dd / 00552b05 subtract the actual 006bc9d4 constant (2.0),
    // not an epsilon. Retain its interval/extrapolation behavior for dense keys.
    std::size_t i = 0;
    while (i + 1 < keys.size() && float(keys[i].frame) <= frame - 2.f)
        ++i;
    if (float(keys[i].frame) > frame && i)
        --i;
    return i;
}
std::array<float, 4> slerp(const std::array<float, 4>& a, const std::array<float, 4>& b, float t) {
    // 005e6d56 shortest-arc selection, threshold 0x34000000, without a
    // normalization step. Preserve authored quaternion magnitudes.
    double dot =
        (double(a[0]) * b[0] + double(a[2]) * b[2]) + double(a[1]) * b[1] + double(a[3]) * b[3];
    double sign = dot < 0 ? -1 : 1;
    dot *= sign;
    double first = 1 - double(t), second = t;
    if (1 - dot > 0x1p-23) {
        double sine = std::sqrt(1 - dot * dot);
        float angle = float(std::atan2(sine, float(dot))), reciprocal = float(1 / sine);
        first = std::sin(double(angle) * (1 - double(t))) * reciprocal;
        second = std::sin(double(angle) * t) * reciprocal;
    }
    std::array<float, 4> result;
    for (unsigned i = 0; i < 4; ++i)
        result[i] = float(first * a[i] + second * sign * b[i]);
    return result;
}
Matrix rotation(const std::array<float, 4>& q) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    return {{{1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w), 0},
             {2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w), 0},
             {2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y), 0},
             {0, 0, 0, 1}}};
}
std::array<float, 4> quaternion(const Matrix& matrix) {
    // Scalar 005e6789 uses the row-vector off-diagonal differences and
    // selects the largest diagonal branch when the trace is non-positive.
    const auto& m = matrix.m;
    std::array<float, 4> q{};
    double trace = double(m[0][0]) + m[1][1] + m[2][2];
    if (trace > 0) {
        double scale = std::sqrt(trace + 1) * .5, factor = .25 / scale;
        q[3] = float(scale);
        q[0] = float((double(m[1][2]) - m[2][1]) * factor);
        q[1] = float((double(m[2][0]) - m[0][2]) * factor);
        q[2] = float((double(m[0][1]) - m[1][0]) * factor);
    } else {
        float diagonal[3] = {float(double(m[0][0]) - m[1][1] - m[2][2]),
                             float(double(m[1][1]) - m[0][0] - m[2][2]),
                             float(double(m[2][2]) - m[1][1] - m[0][0])};
        unsigned i = diagonal[0] < diagonal[1] ? 1 : 0;
        if (diagonal[i] < diagonal[2])
            i = 2;
        auto j = (i + 1) % 3, k = (j + 1) % 3;
        double scale = std::sqrt(double(diagonal[i]) + 1) * .5;
        if (!(scale > 0))
            throw std::runtime_error("Invalid animation rotation matrix");
        double factor = .25 / scale;
        q[i] = float(scale);
        q[j] = float((double(m[j][i]) + m[i][j]) * factor);
        q[k] = float((double(m[k][i]) + m[i][k]) * factor);
        q[3] = float((double(m[j][k]) - m[k][j]) * factor);
    }
    return q;
}
} // namespace
AnimationPose::AnimationPose(Animation animation) : animation_(std::move(animation)) {
    const auto count = animation_.bones.size();
    rest_.resize(count);
    std::vector<Matrix> inverses;
    inverses.reserve(count);
    for (const auto& bone : animation_.bones)
        inverses.push_back(inverse(bone.authored));
    for (std::size_t i = 0; i < count; ++i) {
        auto parent = animation_.bones[i].parent;
        if (parent >= 0 && std::size_t(parent) >= count)
            throw std::runtime_error("Missing animation parent");
        // 005531b0: raw matrix * inverse(raw parent) -> local rest at +0x80.
        rest_[i] = parent < 0 ? animation_.bones[i].authored
                              : multiply(animation_.bones[i].authored, inverses[parent]);
    }
    std::vector<unsigned char> state(count, 0);
    for (std::size_t i = 0; i < count; ++i) {
        std::vector<std::size_t> chain;
        auto at = static_cast<std::int32_t>(i);
        while (at >= 0 && !state[at]) {
            state[at] = 1;
            chain.push_back(std::size_t(at));
            at = animation_.bones[at].parent;
        }
        if (at >= 0 && state[at] == 1)
            throw std::runtime_error("Cyclic animation hierarchy");
        for (auto entry = chain.rbegin(); entry != chain.rend(); ++entry) {
            order_.push_back(*entry);
            state[*entry] = 2;
        }
    }
}
std::vector<Matrix> AnimationPose::locals(float frame) const {
    if (!std::isfinite(frame))
        throw std::invalid_argument("Non-finite animation sample frame");
    std::vector<Matrix> finals(rest_.size());
    for (auto i : order_) {
        const auto& bone = animation_.bones[i];
        Matrix local = rest_[i];
        if (!bone.rotations.empty()) {
            const auto& keys = bone.rotations;
            auto index = key_index(keys, frame);
            auto q = keys[index].value;
            if (float(keys.back().frame) > frame && index + 1 < keys.size()) {
                // 00552a83 substitutes clip end in the last rotation interval.
                auto end = index + 2 == keys.size() ? animation_.end : keys[index + 1].frame;
                auto span = double(end) - keys[index].frame;
                if (span == 0)
                    throw std::runtime_error("Zero-length animation rotation interval");
                q = slerp(q, keys[index + 1].value,
                          float((double(frame) - keys[index].frame) / span));
            }
            local = rotation(q);
        }
        for (unsigned axis = 0; axis < 3; ++axis)
            local.m[3][axis] = rest_[i].m[3][axis];
        if (!bone.translations.empty()) {
            const auto& keys = bone.translations;
            auto index = key_index(keys, frame);
            auto value = keys[index].value;
            if (float(keys.back().frame) > frame && index + 1 < keys.size()) {
                auto span = double(keys[index + 1].frame) - keys[index].frame;
                if (span == 0)
                    throw std::runtime_error("Zero-length animation translation interval");
                float t = float((double(frame) - keys[index].frame) / span);
                for (unsigned axis = 0; axis < 3; ++axis)
                    value[axis] += t * (keys[index + 1].value[axis] - value[axis]);
            }
            for (unsigned axis = 0; axis < 3; ++axis)
                local.m[3][axis] = value[axis];
        }
        finals[i] = local;
        for (const auto& row : finals[i].m)
            for (auto value : row)
                if (!std::isfinite(value))
                    throw std::runtime_error("Non-finite animation pose");
    }
    return finals;
}
std::vector<Matrix> AnimationPose::compose(const std::vector<Matrix>& locals) const {
    std::vector<Matrix> finals(locals.size());
    for (auto i : order_) {
        auto parent = animation_.bones[i].parent;
        finals[i] = parent < 0 ? locals[i] : multiply(locals[i], finals[parent]);
        for (const auto& row : finals[i].m)
            for (auto value : row)
                if (!std::isfinite(value))
                    throw std::runtime_error("Non-finite composed animation pose");
    }
    return finals;
}
std::vector<Matrix> AnimationPose::sample(float frame) const {
    return compose(locals(frame));
}
std::vector<Matrix> AnimationPose::blend_to_rest(float frame, const AnimationPose& target,
                                                 float phase) const {
    if (!std::isfinite(phase))
        throw std::invalid_argument("Non-finite animation blend phase");
    // The recovered owner retains previous finals on unmatched source bones.
    // A stateless blend cannot invent that history; its caller must supply a
    // compatible target until the stateful actor owner is migrated.
    if (target.rest_.size() < rest_.size())
        throw std::runtime_error("Animation blend target has fewer bones than source");
    auto source = locals(frame);
    float factor = phase * .2f;
    for (std::size_t i = 0; i < source.size(); ++i) {
        auto blended = rotation(slerp(quaternion(source[i]), quaternion(target.rest_[i]), factor));
        for (unsigned axis = 0; axis < 3; ++axis)
            blended.m[3][axis] =
                float(double(source[i].m[3][axis]) +
                      (double(target.rest_[i].m[3][axis]) - source[i].m[3][axis]) * factor);
        source[i] = blended;
    }
    return compose(source);
}
} // namespace genesis
