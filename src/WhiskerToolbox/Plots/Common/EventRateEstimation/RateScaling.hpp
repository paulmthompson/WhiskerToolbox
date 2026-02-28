#ifndef RATE_SCALING_HPP
#define RATE_SCALING_HPP

/**
 * @file RateScaling.hpp
 * @brief Lightweight scaling/normalization types for rate estimation
 *
 * This header contains only the enum, helper functions, and output struct
 * used for configuring and applying rate scaling. It has no transitive
 * dependencies on GatherResult, DataManager, or any Qt classes, making it
 * safe to include from state headers (e.g., HeatmapState.hpp) without
 * pulling in heavy compilation dependencies.
 *
 * The actual normalization implementation (`normalizeRateProfiles`) lives in
 * EventRateEstimation.hpp/cpp, which includes this header.
 *
 * @see EventRateEstimation.hpp for the full rate estimation API
 * @see HeatmapState.hpp — includes this header for enum/config types
 */

#include <cstddef>
#include <string>
#include <vector>

namespace WhiskerToolbox::Plots {

// =============================================================================
// Scaling Mode
// =============================================================================

/**
 * @brief Scaling mode for heatmap cell values
 *
 * Controls how raw event counts are transformed before colormap mapping.
 * Applied per-unit (per-row) after rate estimation.
 */
enum class HeatmapScaling {
    FiringRate,     ///< count / (bin_size_s × num_trials) → spikes/s (default)
    ZScore,         ///< Per-unit z-score: (rate - mean) / std across all bins
    Normalized01,   ///< Per-unit min-max normalization to [0, 1]
    RawCount,       ///< Raw event count (no normalization)
    CountPerTrial   ///< count / num_trials
};

/**
 * @brief Return a human-readable label for a HeatmapScaling value
 */
[[nodiscard]] constexpr char const * scalingLabel(HeatmapScaling scaling) noexcept
{
    switch (scaling) {
        case HeatmapScaling::FiringRate:    return "Firing Rate (Hz)";
        case HeatmapScaling::ZScore:        return "Z-Score";
        case HeatmapScaling::Normalized01:  return "Normalized [0, 1]";
        case HeatmapScaling::RawCount:      return "Raw Count";
        case HeatmapScaling::CountPerTrial: return "Count / Trial";
    }
    return "Unknown";
}

/**
 * @brief Return the colormap axis label appropriate for a scaling mode
 */
[[nodiscard]] constexpr char const * scalingAxisLabel(HeatmapScaling scaling) noexcept
{
    switch (scaling) {
        case HeatmapScaling::FiringRate:    return "Firing Rate (Hz)";
        case HeatmapScaling::ZScore:        return "Z-Score (σ)";
        case HeatmapScaling::Normalized01:  return "Normalized";
        case HeatmapScaling::RawCount:      return "Count";
        case HeatmapScaling::CountPerTrial: return "Count / Trial";
    }
    return "";
}

/**
 * @brief All available scaling modes (for populating combo boxes)
 */
[[nodiscard]] inline std::vector<HeatmapScaling> allScalingModes()
{
    return {
        HeatmapScaling::FiringRate,
        HeatmapScaling::ZScore,
        HeatmapScaling::Normalized01,
        HeatmapScaling::RawCount,
        HeatmapScaling::CountPerTrial,
    };
}

// =============================================================================
// Normalized output
// =============================================================================

/**
 * @brief One row of normalized values ready for heatmap rendering
 *
 * Produced by `normalizeRateProfiles()`. Contains the scaled per-bin values
 * plus X-axis metadata. This struct intentionally mirrors
 * `CorePlotting::HeatmapRowData` so the widget can move-construct rows
 * without depending on CorePlotting in this library.
 */
struct NormalizedRow {
    std::vector<double> values;  ///< Scaled per-bin values
    double bin_start = 0.0;      ///< Left edge of the first bin
    double bin_width = 1.0;      ///< Width of each bin (uniform)
};

} // namespace WhiskerToolbox::Plots

#endif // RATE_SCALING_HPP
