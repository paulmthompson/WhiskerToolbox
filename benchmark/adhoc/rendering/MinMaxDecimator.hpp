/**
 * @file MinMaxDecimator.hpp
 * @brief Proof-of-concept min/max decimation for analog time series rendering.
 *
 * Reduces N input samples to 2×num_bins output vertices (min and max per bin),
 * preserving the visual envelope of the signal. This is the standard approach
 * used by electrophysiology waveform viewers (e.g., SpikeGLX, Open Ephys).
 *
 * Two interfaces:
 *  1. Contiguous span path — fastest, for VectorAnalogDataStorage / TensorColumnAnalogStorage
 *  2. Generic AnalogTimeSeries path — works with any storage backend
 *
 * Output is a flat float array: [x_min0, y_min0, x_max0, y_max0, x_min1, y_min1, ...]
 * suitable for direct upload to a GL_LINE_STRIP or GL_LINES vertex buffer.
 */
#ifndef BENCHMARK_ADHOC_RENDERING_MINMAXDECIMATOR_HPP
#define BENCHMARK_ADHOC_RENDERING_MINMAXDECIMATOR_HPP

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace MinMaxDecimator {

/// Result of min/max decimation: flat vertex buffer + metadata
struct DecimationResult {
    std::vector<float> vertices;///< Flat [x0,y0, x1,y1, ...] — 2 vertices per bin
    std::size_t num_bins;       ///< Number of pixel bins
    std::size_t input_samples;  ///< Original sample count
};

// ============================================================================
// 1. Contiguous span path (fastest)
// ============================================================================

/**
 * @brief Decimate contiguous float data using min/max binning.
 *
 * For each bin, finds the min and max values. Outputs them in temporal order
 * (min first if min occurs before max in the bin, otherwise max first) to
 * minimize visual artifacts when rendered as GL_LINE_STRIP.
 *
 * @param data Contiguous span of float samples
 * @param num_bins Number of output bins (typically canvas_width)
 * @param x_start X coordinate of first sample
 * @param x_step X coordinate step per sample (1.0 for index-based)
 * @return DecimationResult with 2 * num_bins vertices (4 * num_bins floats)
 *
 * @pre data.size() > 0
 * @pre num_bins > 0
 */
inline DecimationResult decimateSpan(
        std::span<float const> data,
        std::size_t num_bins,
        float x_start = 0.0f,
        float x_step = 1.0f) {
    assert(!data.empty() && "decimateSpan: data must not be empty");
    assert(num_bins > 0 && "decimateSpan: num_bins must be > 0");

    // If fewer samples than bins, pass through (no decimation needed)
    if (data.size() <= num_bins * 2) {
        DecimationResult result;
        result.num_bins = data.size();
        result.input_samples = data.size();
        result.vertices.reserve(data.size() * 2);
        for (std::size_t i = 0; i < data.size(); ++i) {
            result.vertices.push_back(x_start + static_cast<float>(i) * x_step);
            result.vertices.push_back(data[i]);
        }
        return result;
    }

    DecimationResult result;
    result.num_bins = num_bins;
    result.input_samples = data.size();
    result.vertices.resize(num_bins * 4);// 2 vertices per bin, 2 floats per vertex

    double const samples_per_bin = static_cast<double>(data.size()) / static_cast<double>(num_bins);

    for (std::size_t bin = 0; bin < num_bins; ++bin) {
        auto const bin_start = static_cast<std::size_t>(static_cast<double>(bin) * samples_per_bin);
        auto const bin_end = std::min(
                static_cast<std::size_t>(static_cast<double>(bin + 1) * samples_per_bin),
                data.size());

        float min_val = std::numeric_limits<float>::max();
        float max_val = std::numeric_limits<float>::lowest();
        std::size_t min_idx = bin_start;
        std::size_t max_idx = bin_start;

        for (std::size_t i = bin_start; i < bin_end; ++i) {
            if (data[i] < min_val) {
                min_val = data[i];
                min_idx = i;
            }
            if (data[i] > max_val) {
                max_val = data[i];
                max_idx = i;
            }
        }

        // Output in temporal order to preserve waveform shape
        std::size_t const out_offset = bin * 4;
        if (min_idx <= max_idx) {
            result.vertices[out_offset + 0] = x_start + static_cast<float>(min_idx) * x_step;
            result.vertices[out_offset + 1] = min_val;
            result.vertices[out_offset + 2] = x_start + static_cast<float>(max_idx) * x_step;
            result.vertices[out_offset + 3] = max_val;
        } else {
            result.vertices[out_offset + 0] = x_start + static_cast<float>(max_idx) * x_step;
            result.vertices[out_offset + 1] = max_val;
            result.vertices[out_offset + 2] = x_start + static_cast<float>(min_idx) * x_step;
            result.vertices[out_offset + 3] = min_val;
        }
    }

    return result;
}

// ============================================================================
// 2. In-place variant (writes into pre-allocated buffer)
// ============================================================================

/**
 * @brief Decimate into a pre-allocated output buffer (zero allocation after warmup).
 *
 * @param data Input samples
 * @param num_bins Number of output bins
 * @param out_vertices Pre-allocated buffer, must have capacity >= num_bins * 4
 * @param x_start X coordinate of first sample
 * @param x_step X step per sample
 * @return Number of floats written (always num_bins * 4, or data.size()*2 if passthrough)
 *
 * @pre out_vertices.size() >= num_bins * 4
 */
inline std::size_t decimateSpanInPlace(
        std::span<float const> data,
        std::size_t num_bins,
        std::span<float> out_vertices,
        float x_start = 0.0f,
        float x_step = 1.0f) {
    assert(!data.empty());
    assert(num_bins > 0);

    // Passthrough: fewer samples than 2x bins
    if (data.size() <= num_bins * 2) {
        assert(out_vertices.size() >= data.size() * 2);
        for (std::size_t i = 0; i < data.size(); ++i) {
            out_vertices[i * 2 + 0] = x_start + static_cast<float>(i) * x_step;
            out_vertices[i * 2 + 1] = data[i];
        }
        return data.size() * 2;
    }

    assert(out_vertices.size() >= num_bins * 4);

    double const samples_per_bin = static_cast<double>(data.size()) / static_cast<double>(num_bins);

    for (std::size_t bin = 0; bin < num_bins; ++bin) {
        auto const bin_start = static_cast<std::size_t>(static_cast<double>(bin) * samples_per_bin);
        auto const bin_end = std::min(
                static_cast<std::size_t>(static_cast<double>(bin + 1) * samples_per_bin),
                data.size());

        float min_val = std::numeric_limits<float>::max();
        float max_val = std::numeric_limits<float>::lowest();
        std::size_t min_idx = bin_start;
        std::size_t max_idx = bin_start;

        for (std::size_t i = bin_start; i < bin_end; ++i) {
            if (data[i] < min_val) {
                min_val = data[i];
                min_idx = i;
            }
            if (data[i] > max_val) {
                max_val = data[i];
                max_idx = i;
            }
        }

        std::size_t const out_offset = bin * 4;
        if (min_idx <= max_idx) {
            out_vertices[out_offset + 0] = x_start + static_cast<float>(min_idx) * x_step;
            out_vertices[out_offset + 1] = min_val;
            out_vertices[out_offset + 2] = x_start + static_cast<float>(max_idx) * x_step;
            out_vertices[out_offset + 3] = max_val;
        } else {
            out_vertices[out_offset + 0] = x_start + static_cast<float>(max_idx) * x_step;
            out_vertices[out_offset + 1] = max_val;
            out_vertices[out_offset + 2] = x_start + static_cast<float>(min_idx) * x_step;
            out_vertices[out_offset + 3] = min_val;
        }
    }

    return num_bins * 4;
}

// ============================================================================
// 3. Generic per-element path (any storage backend)
// ============================================================================

/**
 * @brief Decimate using per-element accessor (works with any storage, including mmap).
 *
 * @tparam GetValueFn Callable taking size_t index, returning float
 * @param get_value Accessor function
 * @param num_samples Total number of samples
 * @param num_bins Number of output bins
 * @param x_start X coordinate of first sample
 * @param x_step X step per sample
 * @return DecimationResult
 */
template<typename GetValueFn>
inline DecimationResult decimateGeneric(
        GetValueFn && get_value,
        std::size_t num_samples,
        std::size_t num_bins,
        float x_start = 0.0f,
        float x_step = 1.0f) {
    assert(num_samples > 0);
    assert(num_bins > 0);

    if (num_samples <= num_bins * 2) {
        DecimationResult result;
        result.num_bins = num_samples;
        result.input_samples = num_samples;
        result.vertices.reserve(num_samples * 2);
        for (std::size_t i = 0; i < num_samples; ++i) {
            result.vertices.push_back(x_start + static_cast<float>(i) * x_step);
            result.vertices.push_back(get_value(i));
        }
        return result;
    }

    DecimationResult result;
    result.num_bins = num_bins;
    result.input_samples = num_samples;
    result.vertices.resize(num_bins * 4);

    double const samples_per_bin = static_cast<double>(num_samples) / static_cast<double>(num_bins);

    for (std::size_t bin = 0; bin < num_bins; ++bin) {
        auto const bin_start = static_cast<std::size_t>(static_cast<double>(bin) * samples_per_bin);
        auto const bin_end = std::min(
                static_cast<std::size_t>(static_cast<double>(bin + 1) * samples_per_bin),
                num_samples);

        float min_val = std::numeric_limits<float>::max();
        float max_val = std::numeric_limits<float>::lowest();
        std::size_t min_idx = bin_start;
        std::size_t max_idx = bin_start;

        for (std::size_t i = bin_start; i < bin_end; ++i) {
            float const v = get_value(i);
            if (v < min_val) {
                min_val = v;
                min_idx = i;
            }
            if (v > max_val) {
                max_val = v;
                max_idx = i;
            }
        }

        std::size_t const out_offset = bin * 4;
        if (min_idx <= max_idx) {
            result.vertices[out_offset + 0] = x_start + static_cast<float>(min_idx) * x_step;
            result.vertices[out_offset + 1] = min_val;
            result.vertices[out_offset + 2] = x_start + static_cast<float>(max_idx) * x_step;
            result.vertices[out_offset + 3] = max_val;
        } else {
            result.vertices[out_offset + 0] = x_start + static_cast<float>(max_idx) * x_step;
            result.vertices[out_offset + 1] = max_val;
            result.vertices[out_offset + 2] = x_start + static_cast<float>(min_idx) * x_step;
            result.vertices[out_offset + 3] = min_val;
        }
    }

    return result;
}

/**
 * @brief Generic in-place variant for pre-allocated buffers.
 */
template<typename GetValueFn>
inline std::size_t decimateGenericInPlace(
        GetValueFn && get_value,
        std::size_t num_samples,
        std::size_t num_bins,
        std::span<float> out_vertices,
        float x_start = 0.0f,
        float x_step = 1.0f) {
    assert(num_samples > 0);
    assert(num_bins > 0);

    if (num_samples <= num_bins * 2) {
        assert(out_vertices.size() >= num_samples * 2);
        for (std::size_t i = 0; i < num_samples; ++i) {
            out_vertices[i * 2 + 0] = x_start + static_cast<float>(i) * x_step;
            out_vertices[i * 2 + 1] = get_value(i);
        }
        return num_samples * 2;
    }

    assert(out_vertices.size() >= num_bins * 4);

    double const samples_per_bin = static_cast<double>(num_samples) / static_cast<double>(num_bins);

    for (std::size_t bin = 0; bin < num_bins; ++bin) {
        auto const bin_start = static_cast<std::size_t>(static_cast<double>(bin) * samples_per_bin);
        auto const bin_end = std::min(
                static_cast<std::size_t>(static_cast<double>(bin + 1) * samples_per_bin),
                num_samples);

        float min_val = std::numeric_limits<float>::max();
        float max_val = std::numeric_limits<float>::lowest();
        std::size_t min_idx = bin_start;
        std::size_t max_idx = bin_start;

        for (std::size_t i = bin_start; i < bin_end; ++i) {
            float const v = get_value(i);
            if (v < min_val) {
                min_val = v;
                min_idx = i;
            }
            if (v > max_val) {
                max_val = v;
                max_idx = i;
            }
        }

        std::size_t const out_offset = bin * 4;
        if (min_idx <= max_idx) {
            out_vertices[out_offset + 0] = x_start + static_cast<float>(min_idx) * x_step;
            out_vertices[out_offset + 1] = min_val;
            out_vertices[out_offset + 2] = x_start + static_cast<float>(max_idx) * x_step;
            out_vertices[out_offset + 3] = max_val;
        } else {
            out_vertices[out_offset + 0] = x_start + static_cast<float>(max_idx) * x_step;
            out_vertices[out_offset + 1] = max_val;
            out_vertices[out_offset + 2] = x_start + static_cast<float>(min_idx) * x_step;
            out_vertices[out_offset + 3] = min_val;
        }
    }

    return num_bins * 4;
}

}// namespace MinMaxDecimator

#endif// BENCHMARK_ADHOC_RENDERING_MINMAXDECIMATOR_HPP
