/**
 * @file SpikeWaveformExtraction.cpp
 * @brief Implementation of multichannel spike waveform snippet extraction.
 */

#include "SpikeWaveformExtraction.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "core/ComputeContext.hpp"

#include <armadillo>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace Neuralyzer::Transforms::V2 {

namespace {

[[nodiscard]] std::string formatColumnName(int channel_id, float time_offset_ms) {
    std::ostringstream ss;
    ss << "ch" << std::setfill('0') << std::setw(2) << channel_id << "_";
    if (time_offset_ms >= 0.0f) {
        ss << "+";
    }
    ss << std::fixed << std::setprecision(3) << time_offset_ms << "ms";
    return ss.str();
}

}// namespace

namespace detail {

std::shared_ptr<TensorData> extractWaveformSnippetsImpl(
        TensorData const & voltage_data,
        std::span<SpikeEventDescriptor const> events,
        SpikeWaveformExtractionParams const & params,
        ComputeContext const & ctx) {

    size_t const total_samples = voltage_data.numRows();
    size_t const total_channels = voltage_data.numColumns();

    if (total_samples == 0 || total_channels == 0 || events.empty()) {
        ctx.reportProgress(100);
        return std::make_shared<TensorData>();
    }

    ctx.reportProgress(0);

    // Retrieve zero-copy Armadillo matrix wrapper
    auto const arma_data = voltage_data.toArmadillo();
    arma::fmat const & mat = arma_data.asArmadilloMatrix();

    // 1. Resolve selected channels of interest
    std::vector<int> selected_channels;
    if (params.channels_of_interest.empty()) {
        selected_channels.reserve(total_channels);
        for (size_t c = 0; c < total_channels; ++c) {
            selected_channels.push_back(static_cast<int>(c));
        }
    } else {
        std::set<int> unique_channels;
        for (int const ch: params.channels_of_interest) {
            if (ch >= 0 && static_cast<size_t>(ch) < total_channels) {
                unique_channels.insert(ch);
            }
        }
        selected_channels.assign(unique_channels.begin(), unique_channels.end());
        if (selected_channels.empty()) {
            for (size_t c = 0; c < total_channels; ++c) {
                selected_channels.push_back(static_cast<int>(c));
            }
        }
    }

    std::set<int> const selected_channel_set(selected_channels.begin(), selected_channels.end());

    // 2. Compute snippet window dimensions
    float const sr = params.sampling_rate_hz.value();
    int const w_pre = std::max(1, static_cast<int>(std::round((params.pre_window_ms.value() / 1000.0f) * sr)));
    int const w_post = std::max(1, static_cast<int>(std::round((params.post_window_ms.value() / 1000.0f) * sr)));
    size_t const snippet_len = static_cast<size_t>(w_pre + w_post + 1);
    size_t const num_feature_cols = selected_channels.size() * snippet_len;

    // 3. Generate column names
    std::vector<std::string> column_names;
    column_names.reserve(num_feature_cols);
    for (int const ch: selected_channels) {
        for (int dt = -w_pre; dt <= w_post; ++dt) {
            float const offset_ms = (static_cast<float>(dt) / sr) * 1000.0f;
            column_names.push_back(formatColumnName(ch, offset_ms));
        }
    }

    // 4. Resolve row time storage mapping
    auto const & row_desc = voltage_data.rows();
    bool const has_time_storage = (row_desc.type() == RowType::TimeFrameIndex && row_desc.timeStoragePtr() != nullptr);

    std::vector<TimeFrameIndex> accepted_timestamps;
    accepted_timestamps.reserve(events.size());

    std::vector<float> flat_features;
    flat_features.reserve(events.size() * num_feature_cols);

    auto const align_mode = params.alignment_mode;
    bool const filter_primary = params.filter_by_primary_channel;

    size_t const total_events = events.size();

    for (size_t i = 0; i < total_events; ++i) {
        if (i % 1000 == 0) {
            if (ctx.shouldCancel()) {
                return std::make_shared<TensorData>();
            }
            int const pct = static_cast<int>((static_cast<double>(i) / static_cast<double>(total_events)) * 90.0);
            ctx.reportProgress(pct);
        }

        auto const & ev = events[i];

        // Map TimeFrameIndex to discrete sample index
        int64_t sample_idx = -1;
        if (has_time_storage) {
            auto const opt_pos = row_desc.timeStorage().findArrayPositionForTimeIndex(ev.timestamp);
            if (opt_pos.has_value()) {
                sample_idx = static_cast<int64_t>(opt_pos.value());
            }
        } else {
            sample_idx = ev.timestamp.getValue();
        }

        if (sample_idx < w_pre || sample_idx + w_post >= static_cast<int64_t>(total_samples)) {
            continue;// Skip boundary events that cannot fit full temporal window
        }

        // Determine center channel if needed
        int center_ch = ev.center_channel.value_or(selected_channels[0]);
        if (!ev.center_channel.has_value() || filter_primary) {
            float max_energy = 0.0f;
            for (size_t c = 0; c < total_channels; ++c) {
                float const * col = mat.colptr(c);
                float energy = 0.0f;
                for (int k = -w_pre; k <= w_post; ++k) {
                    energy += std::abs(col[sample_idx + k]);
                }
                if (energy > max_energy) {
                    max_energy = energy;
                    center_ch = static_cast<int>(c);
                }
            }
        }

        // Apply primary channel filter if requested
        if (filter_primary && selected_channel_set.find(center_ch) == selected_channel_set.end()) {
            continue;// Primary channel is outside selected subset
        }

        // Compute aligned sample center
        int64_t aligned_sample = sample_idx;
        float const * center_col = mat.colptr(static_cast<size_t>(center_ch));

        if (align_mode == WaveformAlignmentMode::LocalNegativeTrough) {
            float min_v = std::numeric_limits<float>::infinity();
            int64_t min_t = sample_idx;
            for (int k = -w_pre; k <= w_post; ++k) {
                float const v = center_col[sample_idx + k];
                if (v < min_v) {
                    min_v = v;
                    min_t = sample_idx + k;
                }
            }
            aligned_sample = min_t;
        } else if (align_mode == WaveformAlignmentMode::LocalPositivePeak) {
            float max_v = -std::numeric_limits<float>::infinity();
            int64_t max_t = sample_idx;
            for (int k = -w_pre; k <= w_post; ++k) {
                float const v = center_col[sample_idx + k];
                if (v > max_v) {
                    max_v = v;
                    max_t = sample_idx + k;
                }
            }
            aligned_sample = max_t;
        } else if (align_mode == WaveformAlignmentMode::SincSubSampleCOG) {
            float sum_weighted_t = 0.0f;
            float sum_v2 = 0.0f;
            for (int k = -w_pre; k <= w_post; ++k) {
                float const v = center_col[sample_idx + k];
                float const v2 = v * v;
                sum_weighted_t += static_cast<float>(sample_idx + k) * v2;
                sum_v2 += v2;
            }
            if (sum_v2 > 1e-9f) {
                aligned_sample = static_cast<int64_t>(std::round(sum_weighted_t / sum_v2));
            }
        }

        // Re-check boundaries after alignment
        if (aligned_sample < w_pre || aligned_sample + w_post >= static_cast<int64_t>(total_samples)) {
            continue;
        }

        // Extract spatio-temporal snippet across all channels of interest
        for (int const ch: selected_channels) {
            float const * col = mat.colptr(static_cast<size_t>(ch));
            for (int k = -w_pre; k <= w_post; ++k) {
                flat_features.push_back(col[aligned_sample + k]);
            }
        }

        accepted_timestamps.push_back(ev.timestamp);
    }

    if (ctx.shouldCancel()) {
        return std::make_shared<TensorData>();
    }
    ctx.reportProgress(95);

    size_t const accepted_rows = accepted_timestamps.size();
    if (accepted_rows == 0) {
        ctx.reportProgress(100);
        return std::make_shared<TensorData>();
    }

    auto out_time_storage = std::make_shared<SparseTimeIndexStorage>(std::move(accepted_timestamps));
    auto out_tensor = std::make_shared<TensorData>(
            TensorData::createTimeSeries2D(
                    std::move(flat_features),
                    accepted_rows,
                    num_feature_cols,
                    out_time_storage,
                    voltage_data.getTimeFrame(),
                    std::move(column_names)));

    ctx.reportProgress(100);
    return out_tensor;
}

}// namespace detail

std::shared_ptr<TensorData> extractSpikeWaveforms(
        TensorData const & voltage_data,
        DigitalEventSeries const & event_series,
        SpikeWaveformExtractionParams const & params,
        ComputeContext const & ctx) {

    size_t const num_events = event_series.size();
    if (num_events == 0) {
        ctx.reportProgress(100);
        return std::make_shared<TensorData>();
    }

    std::vector<detail::SpikeEventDescriptor> event_descriptors;
    event_descriptors.reserve(num_events);

    for (size_t i = 0; i < num_events; ++i) {
        event_descriptors.push_back(detail::SpikeEventDescriptor{
                .timestamp = event_series.getStoredEvent(i),
                .center_channel = std::nullopt,
                .peak_amplitude = std::nullopt,
        });
    }

    return detail::extractWaveformSnippetsImpl(
            voltage_data,
            event_descriptors,
            params,
            ctx);
}

}// namespace Neuralyzer::Transforms::V2
