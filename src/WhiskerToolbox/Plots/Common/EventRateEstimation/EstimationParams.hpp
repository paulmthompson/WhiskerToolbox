#ifndef ESTIMATION_PARAMS_HPP
#define ESTIMATION_PARAMS_HPP

/**
 * @file EstimationParams.hpp
 * @brief Rate estimation method parameter structs and variant type
 *
 * This header is intentionally lightweight (no external dependencies except
 * standard library) so it can be included by state headers without pulling
 * in heavy transitive dependencies like DataManager or GatherResult.
 *
 * @see EventRateEstimation.hpp for the estimation functions that consume these types
 * @see RateEstimationControls for the UI widgets that edit these types
 */

#include <variant>

namespace WhiskerToolbox::Plots {

// =============================================================================
// Rate Estimation: parameter structs (one per method)
// =============================================================================

/**
 * @brief Parameters for simple histogram binning
 *
 * Divides the half-open window `[-window_size/2, +window_size/2)` into
 * equal-width bins of `bin_size` and counts events that fall into each bin
 * across all trials.
 *
 * The result stores raw event counts in `RateEstimate::values` and bin
 * centers in `RateEstimate::times`. Use `applyScaling()` to convert to
 * Hz, z-score, etc.
 */
struct BinningParams {
    double bin_size = 10.0; ///< Bin width in the same time units as `window_size`

    [[nodiscard]] bool operator==(BinningParams const &) const = default;
};

// Future methods — stubs for the variant, not yet implemented:

/// @brief Parameters for Gaussian kernel smoothing (stub — not yet implemented)
struct GaussianKernelParams {
    double sigma = 20.0;       ///< Kernel bandwidth (same units as window)
    double eval_step = 1.0;    ///< Spacing of evaluation points

    [[nodiscard]] bool operator==(GaussianKernelParams const &) const = default;
};

/// @brief Parameters for causal exponential filter (stub — not yet implemented)
struct CausalExponentialParams {
    double tau = 50.0;         ///< Decay time constant
    double eval_step = 1.0;    ///< Spacing of evaluation points

    [[nodiscard]] bool operator==(CausalExponentialParams const &) const = default;
};

// =============================================================================
// Rate Estimation: variation point
// =============================================================================

/**
 * @brief Tagged union of all supported rate estimation method parameter structs
 *
 * This is the single variation point for estimation method selection.
 * Selecting a method at runtime is zero-overhead compared to a virtual
 * base-class strategy — `std::visit` dispatches via a jump table.
 *
 * To add a new method:
 *  1. Define a new `*Params` struct above
 *  2. Append it to this alias
 *  3. Add the implementation in `EventRateEstimation.cpp`
 *
 * @note GaussianKernelParams and CausalExponentialParams are included in the
 * variant but their implementations return empty RateEstimate (stub).
 */
using EstimationParams = std::variant<BinningParams, GaussianKernelParams, CausalExponentialParams>;

} // namespace WhiskerToolbox::Plots

#endif // ESTIMATION_PARAMS_HPP
