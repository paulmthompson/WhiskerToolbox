/**
 * @file sinc_interpolation.cpp
 * @brief Implementation of SincInterpolator numerical utilities.
 */

#include "sinc_interpolation.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <numbers>

namespace CoreMath {

double SincInterpolator::sinc(double t) noexcept {
    if (std::abs(t) < 1e-12) {
        return 1.0;
    }
    double const pi_t = std::numbers::pi * t;
    return std::sin(pi_t) / pi_t;
}

double SincInterpolator::window(double t, int a, SincWindowType window_type) noexcept {
    double const abs_t = std::abs(t);
    double const da = static_cast<double>(a);
    if (abs_t >= da) {
        return 0.0;
    }
    switch (window_type) {
        case SincWindowType::Hann:
            return 0.5 * (1.0 + std::cos(std::numbers::pi * t / da));
        case SincWindowType::Blackman: {
            double const x = std::numbers::pi * t / da;
            return 0.42 + 0.5 * std::cos(x) + 0.08 * std::cos(2.0 * x);
        }
        case SincWindowType::Hamming:
            return 0.54 + 0.46 * std::cos(std::numbers::pi * t / da);
        case SincWindowType::Lanczos:
        default:
            return sinc(t / da);
    }
}

float SincInterpolator::fetchSample(
        std::span<float const> data,
        int64_t index,
        BoundaryMode mode) noexcept {

    int64_t const n = static_cast<int64_t>(data.size());
    if (index >= 0 && index < n) {
        return data[static_cast<size_t>(index)];
    }
    if (mode == BoundaryMode::ZeroPad) {
        return 0.0f;
    }
    if (mode == BoundaryMode::Replication) {
        if (index < 0) {
            return data.front();
        }
        return data.back();
    }
    // SymmetricExtension
    if (index < 0) {
        int64_t const reflected = -index;
        if (reflected < n) {
            return data[static_cast<size_t>(reflected)];
        }
        return data.front();
    }
    int64_t const reflected = 2 * (n - 1) - index;
    if (reflected >= 0 && reflected < n) {
        return data[static_cast<size_t>(reflected)];
    }
    return data.back();
}

float SincInterpolator::interpolateAt(
        std::span<float const> data,
        double fractional_index,
        int kernel_half_width,
        SincWindowType window_type,
        BoundaryMode mode) {

    assert(!data.empty() && "interpolateAt: data must not be empty");
    assert(kernel_half_width >= 1 && "interpolateAt: kernel_half_width must be >= 1");

    auto const center_n = static_cast<int64_t>(std::floor(fractional_index));
    int64_t const k_start = center_n - kernel_half_width + 1;
    int64_t const k_end = center_n + kernel_half_width;

    double sum = 0.0;
    double weight_sum = 0.0;

    for (int64_t n = k_start; n <= k_end; ++n) {
        double const t = fractional_index - static_cast<double>(n);
        double const w = sinc(t) * window(t, kernel_half_width, window_type);
        float const sample = fetchSample(data, n, mode);
        sum += w * static_cast<double>(sample);
        weight_sum += w;
    }

    if (std::abs(weight_sum) > 1e-12) {
        return static_cast<float>(sum / weight_sum);
    }
    return static_cast<float>(sum);
}

std::vector<float> SincInterpolator::upsample(
        std::span<float const> data,
        int upsampling_factor,
        int kernel_half_width,
        SincWindowType window_type,
        BoundaryMode mode) {

    assert(!data.empty() && "upsample: data must not be empty");
    assert(upsampling_factor >= 1 && "upsample: upsampling_factor must be >= 1");
    assert(kernel_half_width >= 1 && "upsample: kernel_half_width must be >= 1");

    if (upsampling_factor == 1) {
        return {data.begin(), data.end()};
    }

    size_t const in_len = data.size();
    size_t const out_len = (in_len - 1) * static_cast<size_t>(upsampling_factor) + 1;

    std::vector<float> result(out_len);
    double const step = 1.0 / static_cast<double>(upsampling_factor);

    for (size_t m = 0; m < out_len; ++m) {
        double const frac_pos = static_cast<double>(m) * step;
        result[m] = interpolateAt(data, frac_pos, kernel_half_width, window_type, mode);
    }

    return result;
}

std::vector<float> SincInterpolator::shift(
        std::span<float const> data,
        double fractional_shift,
        int kernel_half_width,
        SincWindowType window_type,
        BoundaryMode mode) {

    assert(!data.empty() && "shift: data must not be empty");
    assert(kernel_half_width >= 1 && "shift: kernel_half_width must be >= 1");

    std::vector<float> result(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        double const pos = static_cast<double>(i) - fractional_shift;
        result[i] = interpolateAt(data, pos, kernel_half_width, window_type, mode);
    }

    return result;
}

std::pair<double, float> SincInterpolator::findSubSampleExtremum(
        std::span<float const> data,
        int center_index,
        bool find_minimum,
        int search_radius,
        int sub_divisions,
        int kernel_half_width,
        SincWindowType window_type,
        BoundaryMode mode) {

    assert(!data.empty() && "findSubSampleExtremum: data must not be empty");
    assert(center_index >= 0 && center_index < static_cast<int>(data.size()) && "findSubSampleExtremum: center_index out of bounds");
    assert(sub_divisions >= 1 && "findSubSampleExtremum: sub_divisions must be >= 1");

    double const start_pos = static_cast<double>(center_index - search_radius);
    double const end_pos = static_cast<double>(center_index + search_radius);
    double const step = 1.0 / static_cast<double>(sub_divisions);

    double best_pos = static_cast<double>(center_index);
    float best_val = interpolateAt(data, best_pos, kernel_half_width, window_type, mode);

    for (double pos = start_pos; pos <= end_pos; pos += step) {
        float const val = interpolateAt(data, pos, kernel_half_width, window_type, mode);
        if (find_minimum) {
            if (val < best_val) {
                best_val = val;
                best_pos = pos;
            }
        } else {
            if (val > best_val) {
                best_val = val;
                best_pos = pos;
            }
        }
    }

    return {best_pos, best_val};
}

double SincInterpolator::findSubSampleEnergyCentroid(
        std::span<float const> data,
        int center_index,
        int window_half_width,
        int sub_divisions,
        int kernel_half_width,
        SincWindowType window_type,
        BoundaryMode mode) {

    assert(!data.empty() && "findSubSampleEnergyCentroid: data must not be empty");
    assert(center_index >= 0 && center_index < static_cast<int>(data.size()) && "findSubSampleEnergyCentroid: center_index out of bounds");
    assert(sub_divisions >= 1 && "findSubSampleEnergyCentroid: sub_divisions must be >= 1");

    double const start_pos = static_cast<double>(center_index - window_half_width);
    double const end_pos = static_cast<double>(center_index + window_half_width);
    double const step = 1.0 / static_cast<double>(sub_divisions);

    double sum_weighted_pos = 0.0;
    double sum_v2 = 0.0;

    for (double pos = start_pos; pos <= end_pos; pos += step) {
        float const val = interpolateAt(data, pos, kernel_half_width, window_type, mode);
        double const v2 = static_cast<double>(val) * static_cast<double>(val);
        sum_weighted_pos += pos * v2;
        sum_v2 += v2;
    }

    if (sum_v2 > 1e-12) {
        return sum_weighted_pos / sum_v2;
    }
    return static_cast<double>(center_index);
}

}// namespace CoreMath
