/**
 * @file SwindaleEventDetection.cpp
 * @brief Ultra-fast zero-copy implementation of the Swindale multi-channel spike detection transform.
 */

#include "SwindaleEventDetection.hpp"

#include "CoreUtilities/ProbeGeometry/SwindaleSpikeSorterLoader.hpp"
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
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace Neuralyzer::Transforms::V2 {

namespace {

struct ProtoEvent {
    int64_t sample_idx{0};
    int channel{0};
    float amplitude{0.0f};
    int polarity{-1};// -1 = negative trough, +1 = positive peak
};

[[nodiscard]] std::vector<ChannelPosition> resolveProbeGeometry(
        SwindaleEventDetectionParams const & params,
        size_t num_channels) {

    std::vector<ChannelPosition> positions;

    // 1. Try loading from .cfg file path if provided
    if (!params.probe_config_path.empty()) {
        std::ifstream file(params.probe_config_path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            positions = parseSwindaleSpikeSorterConfig(buffer.str());
        }
    }

    // 2. Try explicit parameter coordinates
    if (positions.empty() && !params.channel_positions.empty()) {
        positions.reserve(params.channel_positions.size());
        for (auto const & pos: params.channel_positions) {
            positions.push_back(ChannelPosition{
                    .channel_id = pos.channel_id,
                    .x = pos.x_um,
                    .y = pos.y_um,
            });
        }
    }

    // 3. Fallback to 1D linear channel spacing (25 um pitch)
    if (positions.empty()) {
        positions.reserve(num_channels);
        for (size_t c = 0; c < num_channels; ++c) {
            positions.push_back(ChannelPosition{
                    .channel_id = static_cast<int>(c),
                    .x = 0.0f,
                    .y = static_cast<float>(c) * 25.0f,
            });
        }
    }

    return positions;
}

[[nodiscard]] float computeChannelMAD(float const * col_data, size_t n) {
    if (col_data == nullptr || n == 0) {
        return 1.0f;
    }

    // Subsample up to 200,000 evenly spaced points for fast exact median estimation
    size_t const sample_count = std::min(n, size_t{200000});
    size_t const step = std::max(size_t{1}, n / sample_count);

    std::vector<float> sample_vals;
    sample_vals.reserve(sample_count);
    for (size_t i = 0; i < n; i += step) {
        sample_vals.push_back(col_data[i]);
    }

    if (sample_vals.empty()) {
        return 1.0f;
    }

    size_t const mid = sample_vals.size() / 2;
    std::nth_element(sample_vals.begin(), sample_vals.begin() + static_cast<std::ptrdiff_t>(mid), sample_vals.end());
    float const median_val = sample_vals[mid];

    std::vector<float> deviations;
    deviations.reserve(sample_vals.size());
    for (float const val: sample_vals) {
        deviations.push_back(std::abs(val - median_val));
    }

    std::nth_element(deviations.begin(), deviations.begin() + static_cast<std::ptrdiff_t>(mid), deviations.end());
    float const mad = deviations[mid];

    return mad > 1e-9f ? (mad / 0.6745f) : 1.0f;
}

}// namespace

std::shared_ptr<DigitalEventSeries> swindaleEventDetection(
        TensorData const & input,
        SwindaleEventDetectionParams const & params,
        ComputeContext const & ctx) {

    size_t const total_samples = input.numRows();
    size_t const num_channels = input.numColumns();

    if (total_samples == 0 || num_channels == 0) {
        ctx.reportProgress(100);
        return std::make_shared<DigitalEventSeries>();
    }

    ctx.reportProgress(0);

    // Retrieve zero-copy Armadillo matrix wrapper
    auto const arma_data = input.toArmadillo();
    arma::fmat const & mat = arma_data.asArmadilloMatrix();

    // 1. Resolve probe geometry and pairwise spatial distance matrix
    auto const probe_positions = resolveProbeGeometry(params, num_channels);
    std::vector<std::vector<float>> dist_matrix(num_channels, std::vector<float>(num_channels, 0.0f));

    std::unordered_map<int, ChannelPosition> pos_by_channel;
    for (auto const & pos: probe_positions) {
        pos_by_channel[pos.channel_id] = pos;
    }

    for (size_t i = 0; i < num_channels; ++i) {
        for (size_t j = 0; j < num_channels; ++j) {
            if (i == j) {
                continue;
            }
            float const xi = pos_by_channel.count(static_cast<int>(i)) > 0 ? pos_by_channel[static_cast<int>(i)].x : 0.0f;
            float const yi = pos_by_channel.count(static_cast<int>(i)) > 0 ? pos_by_channel[static_cast<int>(i)].y : static_cast<float>(i) * 25.0f;
            float const xj = pos_by_channel.count(static_cast<int>(j)) > 0 ? pos_by_channel[static_cast<int>(j)].x : 0.0f;
            float const yj = pos_by_channel.count(static_cast<int>(j)) > 0 ? pos_by_channel[static_cast<int>(j)].y : static_cast<float>(j) * 25.0f;
            dist_matrix[i][j] = std::sqrt((xi - xj) * (xi - xj) + (yi - yj) * (yi - yj));
        }
    }

    // 2. Compute noise scale (MAD) and threshold per channel using direct column pointers
    std::vector<float> noise_scales(num_channels, 1.0f);
    std::vector<float> thresholds(num_channels, 5.0f);
    float const threshold_mult = params.threshold_multiplier.value();

    for (size_t c = 0; c < num_channels; ++c) {
        float const * col_ptr = mat.colptr(c);
        noise_scales[c] = computeChannelMAD(col_ptr, total_samples);
        thresholds[c] = threshold_mult * noise_scales[c];
    }

    if (ctx.shouldCancel()) {
        return std::make_shared<DigitalEventSeries>();
    }
    ctx.reportProgress(5);

    // 3. Streaming Chunked Detection & Spatio-Temporal Clustering (10s chunks with 10ms overlap)
    float const sr = params.sampling_rate_hz.value();
    int const w_samples = std::max(1, static_cast<int>(std::round((params.multiphasic_window_ms.value() / 1000.0f) * sr)));
    float const sigma_t_samples = (params.temporal_sigma_ms.value() / 1000.0f) * sr;
    float const sigma_spatial_um = params.spatial_sigma_um.value();
    auto const detect_method = params.method;
    auto const align_method = params.alignment_method;

    int64_t const chunk_size = std::max(int64_t{30000}, static_cast<int64_t>(sr * 10.0f));// 10 seconds per chunk
    int64_t const overlap = std::max(int64_t{300}, static_cast<int64_t>(std::ceil(sigma_t_samples * 4.0f)));

    struct DetectedClusterEvent {
        int64_t sample_idx{0};
        int center_channel{0};
    };

    std::vector<DetectedClusterEvent> raw_cluster_events;
    raw_cluster_events.reserve(total_samples / 50);

    for (int64_t chunk_start = 0; chunk_start < static_cast<int64_t>(total_samples); chunk_start += chunk_size) {
        if (ctx.shouldCancel()) {
            return std::make_shared<DigitalEventSeries>();
        }

        int64_t const chunk_end = std::min(static_cast<int64_t>(total_samples), chunk_start + chunk_size + overlap);
        int64_t const valid_boundary = (chunk_end < static_cast<int64_t>(total_samples)) ? (chunk_start + chunk_size) : chunk_end;

        // Stage 1: Proto-Event Detection within chunk
        std::vector<ProtoEvent> chunk_proto_events;
        chunk_proto_events.reserve(4096);

        for (size_t c = 0; c < num_channels; ++c) {
            float const * col = mat.colptr(c);
            float const th = thresholds[c];

            int64_t const t_min = std::max(int64_t{w_samples}, chunk_start);
            int64_t const t_max = std::min(static_cast<int64_t>(total_samples) - w_samples - 1, chunk_end);

            for (int64_t t = t_min; t < t_max; ++t) {
                float const v_curr = col[t];
                float const v_prev = col[t - 1];
                float const v_next = col[t + 1];

                // Check negative trough
                if (detect_method == DetectionMethod::DynamicMultiphasic ||
                    detect_method == DetectionMethod::UnipolarNegative ||
                    detect_method == DetectionMethod::BipolarAmplitude) {

                    if (v_curr < v_prev && v_curr < v_next && v_curr <= -th) {
                        if (detect_method == DetectionMethod::DynamicMultiphasic) {
                            float max_val = v_curr;
                            for (int k = -w_samples; k <= w_samples; ++k) {
                                max_val = std::max(max_val, col[t + k]);
                            }
                            if (max_val - v_curr >= 2.0f * th) {
                                chunk_proto_events.push_back(ProtoEvent{
                                        .sample_idx = t,
                                        .channel = static_cast<int>(c),
                                        .amplitude = v_curr,
                                        .polarity = -1,
                                });
                            }
                        } else {
                            chunk_proto_events.push_back(ProtoEvent{
                                    .sample_idx = t,
                                    .channel = static_cast<int>(c),
                                    .amplitude = v_curr,
                                    .polarity = -1,
                            });
                        }
                    }
                }

                // Check positive peak
                if (detect_method == DetectionMethod::DynamicMultiphasic ||
                    detect_method == DetectionMethod::UnipolarPositive ||
                    detect_method == DetectionMethod::BipolarAmplitude) {

                    if (v_curr > v_prev && v_curr > v_next && v_curr >= th) {
                        if (detect_method == DetectionMethod::DynamicMultiphasic) {
                            float min_val = v_curr;
                            for (int k = -w_samples; k <= w_samples; ++k) {
                                min_val = std::min(min_val, col[t + k]);
                            }
                            if (v_curr - min_val >= 2.0f * th) {
                                chunk_proto_events.push_back(ProtoEvent{
                                        .sample_idx = t,
                                        .channel = static_cast<int>(c),
                                        .amplitude = v_curr,
                                        .polarity = 1,
                                });
                            }
                        } else {
                            chunk_proto_events.push_back(ProtoEvent{
                                    .sample_idx = t,
                                    .channel = static_cast<int>(c),
                                    .amplitude = v_curr,
                                    .polarity = 1,
                            });
                        }
                    }
                }
            }
        }

        // Stage 2: Spatio-Temporal Clustering within chunk
        std::sort(chunk_proto_events.begin(), chunk_proto_events.end(),
                  [](ProtoEvent const & a, ProtoEvent const & b) {
                      return std::abs(a.amplitude) > std::abs(b.amplitude);
                  });

        int64_t const bucket_size = 10;
        std::unordered_map<int64_t, std::vector<size_t>> time_buckets;
        time_buckets.reserve(chunk_proto_events.size() / 4);
        for (size_t i = 0; i < chunk_proto_events.size(); ++i) {
            int64_t const b = chunk_proto_events[i].sample_idx / bucket_size;
            time_buckets[b].push_back(i);
        }

        std::vector<bool> suppressed(chunk_proto_events.size(), false);

        for (size_t i = 0; i < chunk_proto_events.size(); ++i) {
            if (suppressed[i]) {
                continue;
            }

            auto const & primary = chunk_proto_events[i];
            suppressed[i] = true;

            std::vector<size_t> cluster_indices;
            cluster_indices.push_back(i);

            int64_t const b0 = static_cast<int64_t>(std::floor(static_cast<float>(primary.sample_idx) - sigma_t_samples - 1.0f)) / bucket_size;
            int64_t const b1 = static_cast<int64_t>(std::ceil(static_cast<float>(primary.sample_idx) + sigma_t_samples + 1.0f)) / bucket_size;

            for (int64_t b = b0; b <= b1; ++b) {
                auto const it = time_buckets.find(b);
                if (it == time_buckets.end()) {
                    continue;
                }
                for (size_t const j: it->second) {
                    if (suppressed[j]) {
                        continue;
                    }
                    auto const & cand = chunk_proto_events[j];
                    if (std::abs(static_cast<float>(cand.sample_idx - primary.sample_idx)) <= sigma_t_samples) {
                        if (dist_matrix[static_cast<size_t>(primary.channel)][static_cast<size_t>(cand.channel)] <= sigma_spatial_um) {
                            suppressed[j] = true;
                            cluster_indices.push_back(j);
                        }
                    }
                }
            }

            // Center Channel Selection: channel with largest sum of absolute voltages in window
            int center_channel = primary.channel;
            float max_abs_sum = 0.0f;

            for (size_t const idx: cluster_indices) {
                int const c = chunk_proto_events[idx].channel;
                float const * col = mat.colptr(static_cast<size_t>(c));
                float sum_abs = 0.0f;
                int64_t const t_start = std::max(int64_t{0}, primary.sample_idx - w_samples);
                int64_t const t_end = std::min(static_cast<int64_t>(total_samples) - 1, primary.sample_idx + w_samples);
                for (int64_t t = t_start; t <= t_end; ++t) {
                    sum_abs += std::abs(col[t]);
                }
                if (sum_abs > max_abs_sum) {
                    max_abs_sum = sum_abs;
                    center_channel = c;
                }
            }

            // Compute aligned timestamp on center channel
            float const * center_col = mat.colptr(static_cast<size_t>(center_channel));
            int64_t aligned_t = primary.sample_idx;

            if (align_method == AlignmentMethod::V2WeightedCOG) {
                float sum_weighted_t = 0.0f;
                float sum_v2 = 0.0f;
                int64_t const t_start = std::max(int64_t{0}, primary.sample_idx - w_samples);
                int64_t const t_end = std::min(static_cast<int64_t>(total_samples) - 1, primary.sample_idx + w_samples);
                for (int64_t t = t_start; t <= t_end; ++t) {
                    float const v = center_col[t];
                    float const v2 = v * v;
                    sum_weighted_t += static_cast<float>(t) * v2;
                    sum_v2 += v2;
                }
                if (sum_v2 > 1e-9f) {
                    aligned_t = static_cast<int64_t>(std::round(sum_weighted_t / sum_v2));
                }
            } else if (align_method == AlignmentMethod::NegativeTrough) {
                float min_v = std::numeric_limits<float>::infinity();
                int64_t min_t = primary.sample_idx;
                int64_t const t_start = std::max(int64_t{0}, primary.sample_idx - w_samples);
                int64_t const t_end = std::min(static_cast<int64_t>(total_samples) - 1, primary.sample_idx + w_samples);
                for (int64_t t = t_start; t <= t_end; ++t) {
                    float const v = center_col[t];
                    if (v < min_v) {
                        min_v = v;
                        min_t = t;
                    }
                }
                aligned_t = min_t;
            } else if (align_method == AlignmentMethod::PositivePeak) {
                float max_v = -std::numeric_limits<float>::infinity();
                int64_t max_t = primary.sample_idx;
                int64_t const t_start = std::max(int64_t{0}, primary.sample_idx - w_samples);
                int64_t const t_end = std::min(static_cast<int64_t>(total_samples) - 1, primary.sample_idx + w_samples);
                for (int64_t t = t_start; t <= t_end; ++t) {
                    float const v = center_col[t];
                    if (v > max_v) {
                        max_v = v;
                        max_t = t;
                    }
                }
                aligned_t = max_t;
            }

            // Only accept events whose aligned timestamp is within the non-overlapping chunk boundary
            if (aligned_t < valid_boundary) {
                raw_cluster_events.push_back(DetectedClusterEvent{
                        .sample_idx = aligned_t,
                        .center_channel = center_channel,
                });
            }
        }

        int const pct = static_cast<int>((static_cast<double>(chunk_end) / static_cast<double>(total_samples)) * 80.0);
        ctx.reportProgress(5 + pct);
    }

    // 3. Multichannel Template Realignment pass (matches SpikeSorter)
    std::vector<int64_t> final_event_samples;
    final_event_samples.reserve(raw_cluster_events.size());

    if (params.enable_template_realignment && !raw_cluster_events.empty()) {
        std::vector<std::vector<int64_t>> events_by_ch(num_channels);
        for (auto const & ev: raw_cluster_events) {
            if (ev.center_channel >= 0 && static_cast<size_t>(ev.center_channel) < num_channels) {
                events_by_ch[static_cast<size_t>(ev.center_channel)].push_back(ev.sample_idx);
            }
        }

        int const template_w_pre = std::max(5, static_cast<int>(std::round(0.33f / 1000.0f * sr)));
        int const template_w_post = std::max(10, static_cast<int>(std::round(0.70f / 1000.0f * sr)));
        int const search_samples = std::max(2, static_cast<int>(std::round((params.template_search_window_ms.value() / 1000.0f) * sr)));
        TemplateRealignerParams realign_params{
                .w_pre = template_w_pre,
                .w_post = template_w_post,
                .search_window = search_samples,
                .max_iterations = params.template_max_iterations.value(),
                .sinc_sub_divisions = 8,
                .sinc_kernel_half_width = 8,
        };

        for (size_t c = 0; c < num_channels; ++c) {
            if (events_by_ch[c].empty()) {
                continue;
            }
            std::vector<int> neighbor_channels;
            for (size_t k = 0; k < num_channels; ++k) {
                if (dist_matrix[c][k] <= sigma_spatial_um) {
                    neighbor_channels.push_back(static_cast<int>(k));
                }
            }
            if (neighbor_channels.empty()) {
                neighbor_channels.push_back(static_cast<int>(c));
            }

            auto realigned = MultichannelTemplateRealigner::realignEventsDiscrete(
                    mat, events_by_ch[c], neighbor_channels, realign_params);

            final_event_samples.insert(final_event_samples.end(), realigned.begin(), realigned.end());
        }
    } else {
        for (auto const & ev: raw_cluster_events) {
            final_event_samples.push_back(ev.sample_idx);
        }
    }

    // 4. Convert sample timestamps to TimeFrameIndex via RowDescriptor / TimeIndexStorage
    std::sort(final_event_samples.begin(), final_event_samples.end());
    final_event_samples.erase(std::unique(final_event_samples.begin(), final_event_samples.end()), final_event_samples.end());

    std::vector<TimeFrameIndex> event_indices;
    event_indices.reserve(final_event_samples.size());

    auto const & row_desc = input.rows();
    if (row_desc.type() == RowType::TimeFrameIndex) {
        auto const & time_store = row_desc.timeStorage();
        for (int64_t const s: final_event_samples) {
            if (s >= 0 && static_cast<size_t>(s) < total_samples) {
                event_indices.push_back(time_store.getTimeFrameIndexAt(static_cast<size_t>(s)));
            }
        }
    } else {
        for (int64_t const s: final_event_samples) {
            if (s >= 0) {
                event_indices.push_back(TimeFrameIndex{s});
            }
        }
    }

    auto out_series = std::make_shared<DigitalEventSeries>(std::move(event_indices));
    if (input.getTimeFrame()) {
        out_series->setTimeFrame(input.getTimeFrame());
    }

    ctx.reportProgress(100);
    return out_series;
}

}// namespace Neuralyzer::Transforms::V2
