/**
 * @file SpikeWaveformExtraction.cpp
 * @brief Implementation of multichannel spike waveform snippet extraction.
 */

#include "SpikeWaveformExtraction.hpp"

#include "CoreMath/sinc_interpolation.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TransformsV2/utils/MultichannelTemplateRealigner.hpp"
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

    if (voltage_data.numRows() == 0 || events.empty()) {
        return std::make_shared<TensorData>();
    }

    auto const arma_data = voltage_data.toArmadillo();
    arma::fmat const & mat = arma_data.asArmadilloMatrix();

    size_t const total_samples = mat.n_rows;
    size_t const total_channels = mat.n_cols;

    if (total_samples == 0 || total_channels == 0) {
        return std::make_shared<TensorData>();
    }

    float const sr = params.sampling_rate_hz.value();
    int const w_pre = static_cast<int>(std::round((params.pre_window_ms.value() / 1000.0f) * sr));
    int const w_post = static_cast<int>(std::round((params.post_window_ms.value() / 1000.0f) * sr));
    int const num_points = w_pre + w_post + 1;
    int const search_samples = std::max(1, static_cast<int>(std::round((params.search_window_ms.value() / 1000.0f) * sr)));

    // Resolve channels of interest
    std::vector<int> selected_channels;
    if (params.channels_of_interest.empty()) {
        selected_channels.resize(total_channels);
        for (size_t c = 0; c < total_channels; ++c) {
            selected_channels[c] = static_cast<int>(c);
        }
    } else {
        for (int const ch: params.channels_of_interest) {
            if (ch >= 0 && static_cast<size_t>(ch) < total_channels) {
                selected_channels.push_back(ch);
            }
        }
    }

    if (selected_channels.empty()) {
        return std::make_shared<TensorData>();
    }

    std::set<int> const selected_channel_set(selected_channels.begin(), selected_channels.end());
    size_t const num_feature_cols = selected_channels.size() * static_cast<size_t>(num_points);

    // Map TimeFrameIndex to discrete sample index
    auto const & row_desc = voltage_data.rows();
    bool const has_time_storage = (row_desc.type() == RowType::TimeFrameIndex);

    std::vector<TimeFrameIndex> accepted_timestamps;
    accepted_timestamps.reserve(events.size());

    std::vector<float> flat_features;
    flat_features.reserve(events.size() * num_feature_cols);

    auto const align_mode = params.alignment_mode;
    bool const filter_primary = params.filter_by_primary_channel;
    size_t const total_events = events.size();

    // Pre-extract valid candidate events
    struct CandidateEvent {
        TimeFrameIndex timestamp;
        int64_t sample_idx{0};
        int center_channel{0};
    };

    std::vector<CandidateEvent> candidates;
    candidates.reserve(total_events);

    for (size_t i = 0; i < total_events; ++i) {
        auto const & ev = events[i];
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
            continue;
        }

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

        if (filter_primary && selected_channel_set.find(center_ch) == selected_channel_set.end()) {
            continue;
        }

        candidates.push_back(CandidateEvent{
                .timestamp = ev.timestamp,
                .sample_idx = sample_idx,
                .center_channel = center_ch,
        });
    }

    if (candidates.empty()) {
        ctx.reportProgress(100);
        return std::make_shared<TensorData>();
    }

    // Optional IterativeTemplateFit pre-pass
    std::vector<int64_t> aligned_sample_indices(candidates.size());
    for (size_t i = 0; i < candidates.size(); ++i) {
        aligned_sample_indices[i] = candidates[i].sample_idx;
    }

    if (align_mode == WaveformAlignmentMode::IterativeTemplateFit) {
        TemplateRealignerParams realign_params{
                .w_pre = w_pre,
                .w_post = w_post,
                .search_window = search_samples,
                .max_iterations = 3,
                .sinc_sub_divisions = 8,
                .sinc_kernel_half_width = 8,
        };
        aligned_sample_indices = MultichannelTemplateRealigner::realignEventsDiscrete(
                mat, aligned_sample_indices, selected_channels, realign_params);
    }

    for (size_t i = 0; i < candidates.size(); ++i) {
        if (i % 500 == 0) {
            if (ctx.shouldCancel()) {
                return std::make_shared<TensorData>();
            }
            int const pct = static_cast<int>((static_cast<double>(i) / static_cast<double>(candidates.size())) * 90.0);
            ctx.reportProgress(pct);
        }

        auto const & cand = candidates[i];
        int64_t const initial_sample = aligned_sample_indices[i];
        int64_t aligned_sample = initial_sample;
        float const * center_col = mat.colptr(static_cast<size_t>(cand.center_channel));

        if (align_mode == WaveformAlignmentMode::LocalNegativeTrough) {
            float min_v = std::numeric_limits<float>::infinity();
            int64_t min_t = initial_sample;
            int64_t const k_start = std::max(int64_t{0}, initial_sample - search_samples);
            int64_t const k_end = std::min(static_cast<int64_t>(total_samples) - 1, initial_sample + search_samples);
            for (int64_t t = k_start; t <= k_end; ++t) {
                float const v = center_col[t];
                if (v < min_v) {
                    min_v = v;
                    min_t = t;
                }
            }
            aligned_sample = min_t;
        } else if (align_mode == WaveformAlignmentMode::LocalPositivePeak) {
            float max_v = -std::numeric_limits<float>::infinity();
            int64_t max_t = initial_sample;
            int64_t const k_start = std::max(int64_t{0}, initial_sample - search_samples);
            int64_t const k_end = std::min(static_cast<int64_t>(total_samples) - 1, initial_sample + search_samples);
            for (int64_t t = k_start; t <= k_end; ++t) {
                float const v = center_col[t];
                if (v > max_v) {
                    max_v = v;
                    max_t = t;
                }
            }
            aligned_sample = max_t;
        } else if (align_mode == WaveformAlignmentMode::SincSubSampleCOG) {
            std::span<float const> const center_span(center_col, total_samples);
            double const continuous_cog = CoreMath::SincInterpolator::findSubSampleEnergyCentroid(
                    center_span, static_cast<int>(initial_sample), search_samples);
            aligned_sample = static_cast<int64_t>(std::round(continuous_cog));
        } else if (align_mode == WaveformAlignmentMode::SincSubSampleTrough) {
            std::span<float const> const center_span(center_col, total_samples);
            auto const [sub_t, _] = CoreMath::SincInterpolator::findSubSampleExtremum(
                    center_span, static_cast<int>(initial_sample), true, search_samples);
            aligned_sample = static_cast<int64_t>(std::round(sub_t));
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

        accepted_timestamps.push_back(cand.timestamp);
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

    // Generate column labels: "ch01_-0.500ms", ...
    std::vector<std::string> col_labels;
    col_labels.reserve(num_feature_cols);
    for (int const ch: selected_channels) {
        for (int k = -w_pre; k <= w_post; ++k) {
            float const offset_ms = (static_cast<float>(k) / sr) * 1000.0f;
            col_labels.push_back(formatColumnName(ch, offset_ms));
        }
    }

    auto out_time_storage = std::make_shared<SparseTimeIndexStorage>(std::move(accepted_timestamps));
    auto out_tensor = std::make_shared<TensorData>(
            TensorData::createTimeSeries2D(
                    std::move(flat_features),
                    accepted_rows,
                    num_feature_cols,
                    out_time_storage,
                    voltage_data.getTimeFrame(),
                    std::move(col_labels)));

    ctx.reportProgress(100);
    return out_tensor;
}

}// namespace detail

std::shared_ptr<TensorData> extractSpikeWaveforms(
        TensorData const & voltage_data,
        DigitalEventSeries const & event_series,
        SpikeWaveformExtractionParams const & params,
        ComputeContext const & ctx) {

    auto const n_events = event_series.size();
    if (n_events == 0) {
        return std::make_shared<TensorData>();
    }

    std::vector<detail::SpikeEventDescriptor> events;
    events.reserve(n_events);
    for (size_t i = 0; i < n_events; ++i) {
        events.push_back(detail::SpikeEventDescriptor{
                .timestamp = event_series.getStoredEvent(i),
                .center_channel = std::nullopt,
                .peak_amplitude = std::nullopt,
        });
    }

    return detail::extractWaveformSnippetsImpl(voltage_data, events, params, ctx);
}

}// namespace Neuralyzer::Transforms::V2
