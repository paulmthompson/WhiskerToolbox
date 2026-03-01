#ifndef RATE_ESTIMATE_HPP
#define RATE_ESTIMATE_HPP

/**
 * @file RateEstimate.hpp
 * @brief Core output types and scaling modes for rate estimation
 *
 * This header defines:
 * - `RateEstimate`: the universal output of any rate estimation method
 * - `PerTrialData` / `RateEstimateWithTrials`: opt-in per-trial breakdown
 * - `ScalingMode`: shared scaling enum (replaces the old `HeatmapScaling`)
 * - Helper functions for UI population (`scalingLabel`, `scalingAxisLabel`, `allScalingModes`)
 *
 * This header is lightweight (no transitive dependencies on GatherResult,
 * DataManager, or Qt) and is safe to include from state headers.
 *
 * @see RateEstimation.hpp for the estimation functions
 * @see RateNormalization.hpp for scaling/normalization transforms
 * @see RateUncertainty.hpp for confidence band computation
 */

#include <cstddef>
#include <string>
#include <vector>

namespace WhiskerToolbox::Plots {

// =============================================================================
// Core Output
// =============================================================================

/**
 * @brief The universal output of any rate estimation method
 *
 * Always a paired (times, values) suitable for direct plotting.
 * For binning, `times` are bin centers. For kernel methods, `times` would be
 * evaluation grid points. Consumers that need rectangle geometry (heatmap)
 * can reconstruct left edges from `metadata.sample_spacing`.
 */
struct RateEstimate {
    std::vector<double> times;      ///< Evaluation time points (relative to alignment)
    std::vector<double> values;     ///< Estimated values at each time point
    size_t num_trials = 0;          ///< Trials that contributed to the estimate

    /// Metadata about how this estimate was produced
    struct Metadata {
        double sample_spacing = 1.0;  ///< Time between evaluation points
                                       ///  (bin_size for binning, eval_step for kernels)
    };
    Metadata metadata;
};

// =============================================================================
// Per-Trial Data (opt-in)
// =============================================================================

/**
 * @brief Optional per-trial breakdown, for uncertainty and variability analysis
 *
 * Each row is one trial's rate curve, evaluated at the same `times` as the
 * aggregate RateEstimate.
 */
struct PerTrialData {
    /// trials × time_points matrix (row-major: per_trial_values[trial][time_idx])
    std::vector<std::vector<double>> per_trial_values;
};

/**
 * @brief Extended output when per-trial data is requested
 *
 * Returned by `estimateRateWithTrials()`. The aggregate `estimate` is the
 * trial-averaged result; `trials` provides the per-trial breakdown.
 */
struct RateEstimateWithTrials {
    RateEstimate estimate;        ///< Mean/aggregate estimate
    PerTrialData trials;          ///< Per-trial breakdown
};

// =============================================================================
// Scaling Mode (replaces HeatmapScaling)
// =============================================================================

/**
 * @brief Scaling mode for rate estimate values
 *
 * Controls how raw event counts are transformed. Shared between
 * HeatmapWidget, PSTHWidget, and any future rate-based plot widgets.
 * Applied per-unit (per-row for heatmaps) after rate estimation.
 */
enum class ScalingMode {
    FiringRateHz,   ///< count / (sample_spacing_s × num_trials) → spikes/s
    ZScore,         ///< Per-unit z-score: (rate - mean) / std across all time points
    Normalized01,   ///< Per-unit min-max normalization to [0, 1]
    RawCount,       ///< Raw event count (no normalization)
    CountPerTrial,  ///< count / num_trials
};

/**
 * @brief Return a human-readable label for a ScalingMode value
 */
[[nodiscard]] constexpr char const * scalingLabel(ScalingMode mode) noexcept
{
    switch (mode) {
        case ScalingMode::FiringRateHz:  return "Firing Rate (Hz)";
        case ScalingMode::ZScore:        return "Z-Score";
        case ScalingMode::Normalized01:  return "Normalized [0, 1]";
        case ScalingMode::RawCount:      return "Raw Count";
        case ScalingMode::CountPerTrial: return "Count / Trial";
    }
    return "Unknown";
}

/**
 * @brief Return the axis label appropriate for a scaling mode
 */
[[nodiscard]] constexpr char const * scalingAxisLabel(ScalingMode mode) noexcept
{
    switch (mode) {
        case ScalingMode::FiringRateHz:  return "Firing Rate (Hz)";
        case ScalingMode::ZScore:        return "Z-Score (σ)";
        case ScalingMode::Normalized01:  return "Normalized";
        case ScalingMode::RawCount:      return "Count";
        case ScalingMode::CountPerTrial: return "Count / Trial";
    }
    return "";
}

/**
 * @brief All available scaling modes (for populating combo boxes)
 */
[[nodiscard]] inline std::vector<ScalingMode> allScalingModes()
{
    return {
        ScalingMode::FiringRateHz,
        ScalingMode::ZScore,
        ScalingMode::Normalized01,
        ScalingMode::RawCount,
        ScalingMode::CountPerTrial,
    };
}

} // namespace WhiskerToolbox::Plots

#endif // RATE_ESTIMATE_HPP
