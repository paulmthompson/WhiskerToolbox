#ifndef NEURALYZER_V2_SPIKE_WAVEFORM_EXTRACTION_HPP
#define NEURALYZER_V2_SPIKE_WAVEFORM_EXTRACTION_HPP

/**
 * @file SpikeWaveformExtraction.hpp
 * @brief Multichannel spike waveform snippet extraction and alignment transform.
 *
 * Extracts spatio-temporal voltage snippets for detected spike event timestamps,
 * applies optional sub-sample re-alignment, and formats the snippets into a 2D TensorData
 * waveform feature matrix suitable for dimensionality reduction, clustering, and curation.
 */

#include "ParameterSchema/ParameterSchema.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

class TensorData;
class DigitalEventSeries;

namespace Neuralyzer::Transforms::V2 {
struct ComputeContext;

/**
 * @brief Alignment mode for extracted spike waveform snippets.
 */
enum class WaveformAlignmentMode {
    OriginalEventTime,  ///< Extract directly around the input discrete event timestamp
    LocalNegativeTrough,///< Re-align to discrete local minimum on center channel
    LocalPositivePeak,  ///< Re-align to discrete local maximum on center channel
    SincSubSampleCOG,   ///< Sub-sample continuous V^2 energy centroid alignment
    SincSubSampleTrough ///< Sub-sample continuous trough alignment
};

/**
 * @brief Parameters for spike waveform extraction.
 */
struct SpikeWaveformExtractionParams {
    /// Pre-spike extraction window in milliseconds (e.g. 0.50 ms = 15 samples @ 30 kHz)
    rfl::Validator<float, rfl::Minimum<0.05f>, rfl::Maximum<10.0f>> pre_window_ms{0.50f};

    /// Post-spike extraction window in milliseconds (e.g. 1.00 ms = 30 samples @ 30 kHz)
    rfl::Validator<float, rfl::Minimum<0.05f>, rfl::Maximum<10.0f>> post_window_ms{1.00f};

    /// Selected 0-based channel indices of interest (empty = extract all channels)
    std::vector<int> channels_of_interest{};

    /// Filter events: only extract waveforms for events whose primary center channel is in channels_of_interest
    bool filter_by_primary_channel{false};

    /// Waveform snippet alignment method
    WaveformAlignmentMode alignment_mode{WaveformAlignmentMode::OriginalEventTime};

    /// Sinc interpolation upsampling factor (used when sub-sample alignment is enabled)
    rfl::Validator<int, rfl::Minimum<1>, rfl::Maximum<32>> sinc_upsample_factor{4};

    /// Output full upsampled points (true) or resample back to original grid (false)
    bool output_upsampled_points{false};

    /// Acquisition sampling rate in Hz (used to convert ms windows to discrete sample counts)
    rfl::Validator<float, rfl::Minimum<100.0f>> sampling_rate_hz{30000.0f};
};

/**
 * @brief Extract spatio-temporal multichannel waveform snippets for detected spike events.
 *
 * Registered as BinaryContainerTransform: (TensorData, DigitalEventSeries) -> TensorData
 *
 * @param voltage_data Multichannel continuous voltage tensor (T samples x C channels)
 * @param event_series Detected spike event timestamps (DigitalEventSeries)
 * @param params Extraction parameters (window widths, channel selection, alignment)
 * @param ctx Compute context for progress reporting and cancellation checks
 * @return Shared pointer to 2D TensorData (N_events rows x [K_channels * L_points] columns)
 */
[[nodiscard]] std::shared_ptr<TensorData> extractSpikeWaveforms(
        TensorData const & voltage_data,
        DigitalEventSeries const & event_series,
        SpikeWaveformExtractionParams const & params,
        ComputeContext const & ctx);

namespace detail {

/**
 * @brief Intermediate descriptor for a spike event to be extracted.
 */
struct SpikeEventDescriptor {
    TimeFrameIndex timestamp;
    std::optional<int> center_channel;
    std::optional<float> peak_amplitude;
};

/**
 * @brief Core extraction implementation shared across DigitalEventSeries and TensorData event inputs.
 */
[[nodiscard]] std::shared_ptr<TensorData> extractWaveformSnippetsImpl(
        TensorData const & voltage_data,
        std::span<SpikeEventDescriptor const> events,
        SpikeWaveformExtractionParams const & params,
        ComputeContext const & ctx);

}// namespace detail

}// namespace Neuralyzer::Transforms::V2

/**
 * @brief ParameterUIHints specialization for SpikeWaveformExtractionParams
 */
template<>
struct ParameterUIHints<Neuralyzer::Transforms::V2::SpikeWaveformExtractionParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("pre_window_ms")) {
            f->tooltip = "Pre-spike temporal extraction window in milliseconds (default: 0.50 ms)";
        }
        if (auto * f = schema.field("post_window_ms")) {
            f->tooltip = "Post-spike temporal extraction window in milliseconds (default: 1.00 ms)";
        }
        if (auto * f = schema.field("channels_of_interest")) {
            f->tooltip = "0-based channel indices of interest (e.g. tetrode or shank subset). Leave empty to extract all channels.";
        }
        if (auto * f = schema.field("filter_by_primary_channel")) {
            f->tooltip = "If true, only events whose center channel falls within channels_of_interest are extracted.";
        }
        if (auto * f = schema.field("alignment_mode")) {
            f->tooltip = "Waveform re-alignment method (OriginalEventTime, LocalNegativeTrough, SincSubSampleCOG).";
        }
        if (auto * f = schema.field("sinc_upsample_factor")) {
            f->tooltip = "Upsampling factor for sub-sample sinc interpolation (default: 4x).";
        }
        if (auto * f = schema.field("output_upsampled_points")) {
            f->tooltip = "If true, output columns contain the full upsampled waveform points instead of grid-resampled points.";
        }
        if (auto * f = schema.field("sampling_rate_hz")) {
            f->tooltip = "Acquisition sampling rate in Hz (default: 30000 Hz).";
        }
    }
};

#endif// NEURALYZER_V2_SPIKE_WAVEFORM_EXTRACTION_HPP
