#ifndef NEURALYZER_V2_SWINDALE_EVENT_DETECTION_HPP
#define NEURALYZER_V2_SWINDALE_EVENT_DETECTION_HPP

/**
 * @file SwindaleEventDetection.hpp
 * @brief Multi-channel spike detection transform for polytrodes and high-density microelectrode arrays.
 *
 * Implements the two-stage event detection algorithm by Nicholas V. Swindale and Martin A. Spacek:
 *   1. Dynamic Multiphasic 2T proto-event extraction
 *   2. Spatio-temporal / GAC proto-event clustering and center-of-gravity alignment
 *
 * References:
 *   Swindale, N.V., Spacek, M.A., 2014. Spike sorting for polytrodes: a divide and conquer approach.
 *   Frontiers in Systems Neuroscience 8:6. https://doi.org/10.3389/fnsys.2014.00006
 *
 *   Swindale, N.V., Spacek, M.A., 2015. Spike detection methods for polytrodes and high density
 *   microelectrode arrays. J Comput Neurosci 38, 249–267. https://doi.org/10.1007/s10827-014-0539-z
 */

#include "CoreUtilities/ProbeGeometry/ChannelPosition.hpp"
#include "ParameterSchema/ParameterSchema.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <string>
#include <vector>

class TensorData;
class DigitalEventSeries;

namespace Neuralyzer::Transforms::V2 {
struct ComputeContext;

/**
 * @brief Alignment method for resolving cluster event timestamps.
 */
enum class AlignmentMethod {
    V2WeightedCOG, ///< V^2 weighted center of gravity (default; robust continuous energy centroid)
    NegativeTrough,///< Discrete local minimum / deepest negative trough on center channel
    PositivePeak,  ///< Discrete local maximum / positive peak on center channel
    DetectionTime  ///< Direct timestamp of the primary proto-event
};

/**
 * @brief Detection filter mode for Stage 1 proto-event extraction.
 */
enum class DetectionMethod {
    DynamicMultiphasic,///< Primary threshold crossing + 2T excursion within temporal window (recommended)
    UnipolarNegative,  ///< Detect negative troughs only (standard extracellular APs)
    UnipolarPositive,  ///< Detect positive peaks only
    BipolarAmplitude   ///< Detect extrema of either polarity
};

/**
 * @brief Clustering mode for Stage 2 proto-event grouping.
 */
enum class ClusteringMethod {
    GradientAscent,     ///< Swindale GAC (mean-shift) density clustering
    SpatioTemporalWindow///< Greedy space-time lockout window
};

/**
 * @brief 2D channel position for explicit probe geometry override.
 */
struct ChannelPosition2D {
    int channel_id{0};
    float x_um{0.0f};
    float y_um{0.0f};
};

/**
 * @brief Parameters for Swindale multi-channel spike event detection.
 */
struct SwindaleEventDetectionParams {
    /// Detection method for Stage 1 proto-events
    DetectionMethod method{DetectionMethod::DynamicMultiphasic};

    /// Primary threshold multiplier (threshold = threshold_multiplier * MAD / 0.6745)
    rfl::Validator<float, rfl::Minimum<1.0f>, rfl::Maximum<20.0f>> threshold_multiplier{5.0f};

    /// Multiphasic filter temporal window width in milliseconds (e.g. 0.20 ms)
    rfl::Validator<float, rfl::Minimum<0.05f>> multiphasic_window_ms{0.20f};

    /// Event alignment method on center channel
    AlignmentMethod alignment_method{AlignmentMethod::V2WeightedCOG};

    /// Clustering method for Stage 2 proto-event grouping
    ClusteringMethod clustering_method{ClusteringMethod::GradientAscent};

    /// Temporal clustering kernel sigma in milliseconds (default: 0.25 ms)
    rfl::Validator<float, rfl::Minimum<0.05f>> temporal_sigma_ms{0.25f};

    /// Spatial clustering kernel sigma in micrometers (default: 80.0 um)
    rfl::Validator<float, rfl::Minimum<0.0f>> spatial_sigma_um{80.0f};

    /// Acquisition sampling rate in Hz (used to convert ms parameters to discrete sample counts)
    rfl::Validator<float, rfl::Minimum<100.0f>> sampling_rate_hz{30000.0f};

    /// Path to a SpikeSorter electrode configuration file (.cfg) specifying probe layout
    std::string probe_config_path{};

    /// Explicit probe channel coordinates in micrometers (used when probe_config_path is empty)
    std::vector<ChannelPosition2D> channel_positions{};

    /// Enable iterative multichannel template realignment on detected spike clusters (matches SpikeSorter)
    bool enable_template_realignment{true};

    /// Maximum search window width in milliseconds for template alignment (default: 0.50 ms = 15 samples @ 30 kHz)
    rfl::Validator<float, rfl::Minimum<0.05f>, rfl::Maximum<2.0f>> template_search_window_ms{0.50f};

    /// Maximum Expectation-Maximization iterations for template realignment (default: 3)
    rfl::Validator<int, rfl::Minimum<1>, rfl::Maximum<10>> template_max_iterations{3};
};

/**
 * @brief Detect multi-channel spike events across all channels of a TensorData container.
 *
 * @param input Multi-channel tensor data (T time samples x C channels)
 * @param params Algorithm parameters (threshold, window width, alignment, probe geometry)
 * @param ctx Compute context for progress reporting and cancellation checks
 * @return Shared pointer to DigitalEventSeries containing detected spike timestamps
 */
std::shared_ptr<DigitalEventSeries> swindaleEventDetection(
        TensorData const & input,
        SwindaleEventDetectionParams const & params,
        ComputeContext const & ctx);

}// namespace Neuralyzer::Transforms::V2

/**
 * @brief ParameterUIHints specialization for SwindaleEventDetectionParams
 */
template<>
struct ParameterUIHints<Neuralyzer::Transforms::V2::SwindaleEventDetectionParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("probe_config_path")) {
            f->path_field_kind = PathFieldKind::FilePath;
            f->file_dialog_id = "swindale_probe_cfg_open";
            f->tooltip = "Path to SpikeSorter electrode configuration file (.cfg) specifying probe geometry (e.g. electrode.cfg)";
        }
        if (auto * f = schema.field("enable_template_realignment")) {
            f->tooltip = "Enable iterative multichannel template realignment to refine spike times against cluster mean waveforms (matches SpikeSorter)";
        }
        if (auto * f = schema.field("template_search_window_ms")) {
            f->tooltip = "Maximum temporal search shift in milliseconds for template realignment (default: 0.50 ms)";
        }
        if (auto * f = schema.field("template_max_iterations")) {
            f->tooltip = "Maximum Expectation-Maximization iterations for template realignment (default: 3)";
        }
        if (auto * f = schema.field("method")) {
            f->tooltip = "Stage 1 candidate proto-event detection method (DynamicMultiphasic recommended for extracellular APs)";
        }
        if (auto * f = schema.field("threshold_multiplier")) {
            f->tooltip = "Primary detection threshold multiplier in units of noise (threshold = multiplier * MAD / 0.6745; default: 5.0)";
        }
        if (auto * f = schema.field("multiphasic_window_ms")) {
            f->tooltip = "Temporal window width in milliseconds for 2T excursion test (default: 0.20 ms)";
        }
        if (auto * f = schema.field("alignment_method")) {
            f->tooltip = "Spike event time alignment method on center channel (V2WeightedCOG or NegativeTrough)";
        }
        if (auto * f = schema.field("clustering_method")) {
            f->tooltip = "Stage 2 spatio-temporal proto-event clustering method (GradientAscent or SpatioTemporalWindow)";
        }
        if (auto * f = schema.field("temporal_sigma_ms")) {
            f->tooltip = "Temporal clustering lockout / GAC kernel sigma in milliseconds (default: 0.25 ms)";
        }
        if (auto * f = schema.field("spatial_sigma_um")) {
            f->tooltip = "Spatial clustering lockout / GAC kernel sigma in micrometers (default: 80.0 um)";
        }
        if (auto * f = schema.field("sampling_rate_hz")) {
            f->tooltip = "Acquisition sampling rate in Hz (default: 30000 Hz)";
        }
    }
};

#endif// NEURALYZER_V2_SWINDALE_EVENT_DETECTION_HPP
