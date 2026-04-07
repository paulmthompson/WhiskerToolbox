/**
 * @file SincInterpolation.cpp
 * @brief Band-limited sinc interpolation implementation.
 */

#include "SincInterpolation.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include <cmath>
#include <numbers>
#include <vector>

namespace WhiskerToolbox::Transforms::V2::Examples {

namespace {

/// @brief Normalized sinc function: sinc(t) = sin(pi*t) / (pi*t), sinc(0) = 1.
double sinc(double t) {
    if (std::abs(t) < 1e-12) {
        return 1.0;
    }
    double const pi_t = std::numbers::pi * t;
    return std::sin(pi_t) / pi_t;
}

/// @brief Lanczos window: sinc(t / a) for |t| <= a, 0 otherwise.
double lanczosWindow(double t, int a) {
    if (std::abs(t) >= static_cast<double>(a)) {
        return 0.0;
    }
    return sinc(t / static_cast<double>(a));
}

/// @brief Hann window: 0.5 * (1 + cos(pi*t/a)) for |t| <= a, 0 otherwise.
double hannWindow(double t, int a) {
    if (std::abs(t) >= static_cast<double>(a)) {
        return 0.0;
    }
    return 0.5 * (1.0 + std::cos(std::numbers::pi * t / static_cast<double>(a)));
}

/// @brief Blackman window for |t| <= a, 0 otherwise.
double blackmanWindow(double t, int a) {
    if (std::abs(t) >= static_cast<double>(a)) {
        return 0.0;
    }
    double const x = std::numbers::pi * t / static_cast<double>(a);
    return 0.42 + 0.5 * std::cos(x) + 0.08 * std::cos(2.0 * x);
}

/// @brief Apply the selected window function.
double applyWindow(double t, int a, SincWindowType window) {
    switch (window) {
        case SincWindowType::Hann:
            return hannWindow(t, a);
        case SincWindowType::Blackman:
            return blackmanWindow(t, a);
        case SincWindowType::Lanczos:
        default:
            return lanczosWindow(t, a);
    }
}

/// @brief Fetch an input sample with boundary handling.
/// @param data Input sample data
/// @param index Integer sample index (may be out of bounds)
/// @param n Total number of input samples
/// @param mode Boundary handling mode
float fetchSample(std::span<float const> data, int64_t index, int64_t n, BoundaryMode mode) {
    if (index >= 0 && index < n) {
        return data[static_cast<size_t>(index)];
    }
    if (mode == BoundaryMode::ZeroPad) {
        return 0.0f;
    }
    // SymmetricExtension: reflect at boundaries
    if (index < 0) {
        // Reflect: -1 → 1, -2 → 2, etc.
        int64_t const reflected = -index;
        if (reflected < n) {
            return data[static_cast<size_t>(reflected)];
        }
        return data[static_cast<size_t>(n - 1)];// clamp for very large overshoot
    }
    // index >= n: reflect from end
    int64_t const reflected = 2 * (n - 1) - index;
    if (reflected >= 0) {
        return data[static_cast<size_t>(reflected)];
    }
    return data[0];// clamp for very large overshoot
}

}// anonymous namespace

std::shared_ptr<AnalogTimeSeries> sincInterpolation(
        AnalogTimeSeries const & input,
        SincInterpolationParams const & params,
        ComputeContext const & ctx) {

    int const factor = params.upsampling_factor;
    if (factor < 1) {
        return nullptr;
    }

    auto const data = input.getAnalogTimeSeries();
    auto const n = static_cast<int64_t>(data.size());

    if (n == 0) {
        return nullptr;
    }

    // Factor == 1: identity (copy)
    if (factor == 1) {
        std::vector<float> output(data.begin(), data.end());
        return std::make_shared<AnalogTimeSeries>(std::move(output), output.size());
    }

    // Single sample: no interpolation possible, just return a copy
    if (n == 1) {
        std::vector<float> output = {data[0]};
        return std::make_shared<AnalogTimeSeries>(std::move(output), output.size());
    }

    int const K = params.getKernelHalfWidth();
    SincWindowType const window = params.getWindowType();
    BoundaryMode const boundary = params.getBoundaryMode();

    // Output size: (N-1) * factor + 1
    auto const m_total = static_cast<int64_t>((n - 1) * factor + 1);
    std::vector<float> output(static_cast<size_t>(m_total));

    // Progress reporting interval (report ~20 times)
    int64_t const progress_interval = std::max(m_total / 20, int64_t{1});

    for (int64_t m = 0; m < m_total; ++m) {
        // Check for cancellation periodically
        if ((m % progress_interval) == 0) {
            if (ctx.shouldCancel()) {
                return nullptr;
            }
            ctx.reportProgress(static_cast<int>((m * 100) / m_total));
        }

        // Fractional input position
        double const x = static_cast<double>(m) / static_cast<double>(factor);

        // Check if this is an exact input sample position
        if ((m % factor) == 0) {
            // Exact input sample — use it directly (avoids rounding artifacts)
            auto const idx = m / factor;
            output[static_cast<size_t>(m)] = data[static_cast<size_t>(idx)];
            continue;
        }

        // Windowed-sinc interpolation with kernel normalization
        auto const center = static_cast<int64_t>(std::floor(x));
        int64_t const lo = center - K + 1;
        int64_t const hi = center + K;

        double accumulator = 0.0;
        double weight_sum = 0.0;
        for (int64_t idx = lo; idx <= hi; ++idx) {
            double const t = x - static_cast<double>(idx);
            double const w = sinc(t) * applyWindow(t, K, window);
            auto const sample = static_cast<double>(fetchSample(data, idx, n, boundary));
            accumulator += w * sample;
            weight_sum += w;
        }

        // Normalize to ensure DC preservation
        if (std::abs(weight_sum) > 1e-15) {
            accumulator /= weight_sum;
        }

        output[static_cast<size_t>(m)] = static_cast<float>(accumulator);
    }

    // Report completion
    ctx.reportProgress(100);

    auto const output_size = output.size();
    return std::make_shared<AnalogTimeSeries>(std::move(output), output_size);
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
