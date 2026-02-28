#ifndef RATE_SCALING_HPP
#define RATE_SCALING_HPP

/**
 * @file RateScaling.hpp
 * @brief Backward-compatibility header — includes RateEstimate.hpp
 *
 * This header originally defined `HeatmapScaling`, `NormalizedRow`, and
 * helper functions. Those types have been superseded by `ScalingMode` and
 * the composable normalization API in `RateEstimate.hpp` / `RateNormalization.hpp`.
 *
 * This header now provides:
 * - `HeatmapScaling` as a type alias for `ScalingMode`
 * - `NormalizedRow` for any remaining consumers (deprecated)
 * - Re-exports of `scalingLabel`, `scalingAxisLabel`, `allScalingModes`
 *   that operate on `ScalingMode` (via `RateEstimate.hpp`)
 *
 * **New code should include `RateEstimate.hpp` directly.**
 *
 * @deprecated Use RateEstimate.hpp and RateNormalization.hpp instead.
 */

#include "RateEstimate.hpp"

#include <vector>

namespace WhiskerToolbox::Plots {

// =============================================================================
// Backward-compatibility alias
// =============================================================================

/// @deprecated Use `ScalingMode` instead
using HeatmapScaling = ScalingMode;

// =============================================================================
// Deprecated output struct (kept for transition)
// =============================================================================

/**
 * @brief One row of normalized values (deprecated — use RateEstimate + applyScaling instead)
 *
 * @deprecated This struct is retained only for backward compatibility during
 * migration. New code should use `RateEstimate` with in-place `applyScaling()`.
 */
struct [[deprecated("Use RateEstimate with applyScaling() instead")]] NormalizedRow {
    std::vector<double> values;  ///< Scaled per-bin values
    double bin_start = 0.0;      ///< Left edge of the first bin
    double bin_width = 1.0;      ///< Width of each bin (uniform)
};

} // namespace WhiskerToolbox::Plots

#endif // RATE_SCALING_HPP
