/**
 * @file AnalogDifference.cpp
 * @brief Implementation of analog time series differencing container transform
 */

#include "AnalogDifference.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "ParameterSchema/ParameterSchema.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace Neuralyzer::Transforms::V2::Examples {

namespace {

/// @brief Whether the differencing window is fully available at index i
[[nodiscard]] bool hasCompleteWindow(
        DifferenceMethod method,
        int window,
        std::size_t index,
        std::size_t sample_count) {
    assert(window >= 1);
    auto const w = static_cast<std::size_t>(window);
    auto const i = index;
    auto const n = sample_count;

    switch (method) {
        case DifferenceMethod::Backward:
            return i >= w;
        case DifferenceMethod::Forward:
            return i + w < n;
        case DifferenceMethod::Central:
            return i >= w && i + w < n;
    }
    return false;
}

/// @brief Fetch a sample, clamping to boundary when index is out of range
[[nodiscard]] float fetchClamped(std::span<float const> data, std::ptrdiff_t index) {
    if (index <= 0) {
        return data.front();
    }
    auto const n = static_cast<std::ptrdiff_t>(data.size());
    if (index >= n) {
        return data.back();
    }
    return data[static_cast<std::size_t>(index)];
}

/// @brief Compute the difference value at index i
[[nodiscard]] float computeDifference(
        DifferenceMethod method,
        int window,
        std::span<float const> data,
        std::size_t index,
        DifferenceEdgePolicy edge_policy) {
    assert(window >= 1);
    auto const w = static_cast<std::ptrdiff_t>(window);
    auto const i = static_cast<std::ptrdiff_t>(index);

    auto fetch = [&](std::ptrdiff_t idx) -> float {
        if (edge_policy == DifferenceEdgePolicy::Clamp) {
            return fetchClamped(data, idx);
        }
        return data[static_cast<std::size_t>(idx)];
    };

    switch (method) {
        case DifferenceMethod::Backward: {
            float const current = data[static_cast<std::size_t>(i)];
            float const past = fetch(i - w);
            return current - past;
        }
        case DifferenceMethod::Forward: {
            float const current = data[static_cast<std::size_t>(i)];
            float const future = fetch(i + w);
            return future - current;
        }
        case DifferenceMethod::Central: {
            float const past = fetch(i - w);
            float const future = fetch(i + w);
            return (future - past) / (2.0f * static_cast<float>(window));
        }
    }

    return std::numeric_limits<float>::quiet_NaN();
}

/// @brief Fill value for incomplete windows under NaN or Zero policies
[[nodiscard]] float incompleteFillValue(DifferenceEdgePolicy edge_policy) {
    if (edge_policy == DifferenceEdgePolicy::NaN) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    return 0.0f;
}

/// @brief Compute output value at index i respecting edge policy
[[nodiscard]] float valueAtIndex(
        DifferenceMethod method,
        int window,
        std::span<float const> data,
        std::size_t index,
        DifferenceEdgePolicy edge_policy) {
    if (hasCompleteWindow(method, window, index, data.size())) {
        return computeDifference(method, window, data, index, edge_policy);
    }

    if (edge_policy == DifferenceEdgePolicy::Clamp) {
        return computeDifference(method, window, data, index, DifferenceEdgePolicy::Clamp);
    }

    return incompleteFillValue(edge_policy);
}

}// namespace

auto analogDifference(
        AnalogTimeSeries const & input,
        AnalogDifferenceParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<AnalogTimeSeries> {

    int const window = params.window.value();
    if (window < 1) {
        ctx.logMessage("AnalogDifference: window must be >= 1");
        return nullptr;
    }

    auto const data = input.getAnalogTimeSeries();
    auto const times = input.getTimeSeries();
    auto const n = data.size();

    if (n == 0) {
        ctx.logMessage("AnalogDifference: input is empty");
        return nullptr;
    }

    if (times.size() != n) {
        ctx.logMessage("AnalogDifference: data and time vector size mismatch");
        return nullptr;
    }

    ctx.reportProgress(0);

    DifferenceMethod const method = params.method;
    DifferenceEdgePolicy const edge_policy = params.edge_policy;

    std::vector<float> output;
    std::vector<TimeFrameIndex> out_times;

    if (edge_policy == DifferenceEdgePolicy::Skip) {
        output.reserve(n);
        out_times.reserve(n);

        int64_t const progress_interval = std::max(static_cast<int64_t>(n) / 20, int64_t{1});
        for (std::size_t i = 0; i < n; ++i) {
            if (static_cast<int64_t>(i) % progress_interval == 0) {
                if (ctx.shouldCancel()) {
                    return nullptr;
                }
                ctx.reportProgress(static_cast<int>((i * 100) / n));
            }

            if (!hasCompleteWindow(method, window, i, n)) {
                continue;
            }

            output.push_back(computeDifference(method, window, data, i, edge_policy));
            out_times.push_back(times[i]);
        }

        if (output.empty()) {
            ctx.logMessage("AnalogDifference: Skip policy eliminated all samples");
            return nullptr;
        }
    } else {
        output.resize(n);
        out_times.reserve(n);

        int64_t const progress_interval = std::max(static_cast<int64_t>(n) / 20, int64_t{1});
        for (std::size_t i = 0; i < n; ++i) {
            if (static_cast<int64_t>(i) % progress_interval == 0) {
                if (ctx.shouldCancel()) {
                    return nullptr;
                }
                ctx.reportProgress(static_cast<int>((i * 100) / n));
            }

            output[i] = valueAtIndex(method, window, data, i, edge_policy);
            out_times.push_back(times[i]);
        }
    }

    ctx.reportProgress(100);
    return std::make_shared<AnalogTimeSeries>(std::move(output), std::move(out_times));
}

}// namespace Neuralyzer::Transforms::V2::Examples

/**
 * @brief ParameterUIHints specialization for AnalogDifferenceParams
 */
template<>
struct ParameterUIHints<Neuralyzer::Transforms::V2::Examples::AnalogDifferenceParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("method")) {
            f->tooltip = "Backward: x[i] - x[i-window]. "
                         "Central: (x[i+window] - x[i-window]) / (2*window). "
                         "Forward: x[i+window] - x[i].";
        }
        if (auto * f = schema.field("window")) {
            f->tooltip = "Lag in index-adjacent samples for the difference window";
        }
        if (auto * f = schema.field("edge_policy")) {
            f->tooltip = "How to handle samples where the window extends beyond boundaries";
        }
    }
};
