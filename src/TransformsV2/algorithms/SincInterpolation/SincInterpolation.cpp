/**
 * @file SincInterpolation.cpp
 * @brief Band-limited sinc interpolation implementation.
 */

#include "SincInterpolation.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "CoreMath/sinc_interpolation.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include <cmath>
#include <vector>

namespace Neuralyzer::Transforms::V2::Examples {

namespace {

[[nodiscard]] CoreMath::SincWindowType toCoreMathWindow(SincWindowType window) noexcept {
    switch (window) {
        case SincWindowType::Hann:
            return CoreMath::SincWindowType::Hann;
        case SincWindowType::Blackman:
            return CoreMath::SincWindowType::Blackman;
        case SincWindowType::Lanczos:
        default:
            return CoreMath::SincWindowType::Lanczos;
    }
}

[[nodiscard]] CoreMath::BoundaryMode toCoreMathBoundary(BoundaryMode boundary) noexcept {
    switch (boundary) {
        case BoundaryMode::ZeroPad:
            return CoreMath::BoundaryMode::ZeroPad;
        case BoundaryMode::SymmetricExtension:
        default:
            return CoreMath::BoundaryMode::SymmetricExtension;
    }
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
    auto const input_times = input.getTimeSeries();
    auto const n = static_cast<int64_t>(data.size());

    if (n == 0) {
        return nullptr;
    }

    // Factor == 1: identity (copy)
    if (factor == 1) {
        std::vector<float> output(data.begin(), data.end());
        std::vector<TimeFrameIndex> out_times(input_times.begin(), input_times.end());
        return std::make_shared<AnalogTimeSeries>(std::move(output), std::move(out_times));
    }

    // Single sample: no interpolation possible, just return a copy
    if (n == 1) {
        std::vector<float> output = {data[0]};
        std::vector<TimeFrameIndex> out_times = {
                TimeFrameIndex(input_times[0].getValue() * factor)};
        return std::make_shared<AnalogTimeSeries>(std::move(output), std::move(out_times));
    }

    int const K = params.kernel_half_width;
    auto const window = toCoreMathWindow(params.window_type);
    auto const boundary = toCoreMathBoundary(params.boundary_mode);

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

        output[static_cast<size_t>(m)] = CoreMath::SincInterpolator::interpolateAt(
                data, x, K, window, boundary);
    }

    // Report completion
    ctx.reportProgress(100);

    // Compute upsampled TimeFrameIndex values:
    // Original index t_i → t_i * factor; intermediate positions are linearly interpolated
    std::vector<TimeFrameIndex> out_times;
    out_times.reserve(static_cast<size_t>(m_total));
    for (int64_t m = 0; m < m_total; ++m) {
        auto const seg = m / factor; // segment index (which pair of original samples)
        auto const frac = m % factor;// fractional position within segment
        if (frac == 0) {
            // Exact original sample position
            out_times.emplace_back(input_times[static_cast<size_t>(seg)].getValue() * factor);
        } else {
            // Linearly interpolate TimeFrameIndex between seg and seg+1
            auto const t0 = input_times[static_cast<size_t>(seg)].getValue() * factor;
            auto const t1 = input_times[static_cast<size_t>(seg + 1)].getValue() * factor;
            auto const t = t0 + (t1 - t0) * frac / factor;
            out_times.emplace_back(t);
        }
    }

    return std::make_shared<AnalogTimeSeries>(std::move(output), std::move(out_times));
}

}// namespace Neuralyzer::Transforms::V2::Examples
