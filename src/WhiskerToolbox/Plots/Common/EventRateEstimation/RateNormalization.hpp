#ifndef RATE_NORMALIZATION_HPP
#define RATE_NORMALIZATION_HPP

/**
 * @file RateNormalization.hpp
 * @brief Composable normalization primitives and convenience scaling dispatch
 *
 * Provides:
 * - Normalization primitives that operate on `std::span<double>` (e.g.,
 *   `toFiringRateHz`, `toCountPerTrial`, `zScoreNormalize`, `minMaxNormalize`)
 * - Convenience `applyScaling()` functions that dispatch on `ScalingMode`
 *
 * These functions modify values in-place. They can be composed by chaining
 * calls on the same value vector.
 *
 * @see RateEstimate.hpp for ScalingMode and RateEstimate definitions
 */

#include "RateEstimate.hpp"

#include <span>

namespace WhiskerToolbox::Plots {

// =============================================================================
// Normalization primitives (operate on value vectors in-place)
// =============================================================================

/**
 * @brief Convert raw counts to firing rate (Hz)
 *
 * Formula: values[i] /= (num_trials × sample_spacing_s)
 * where sample_spacing_s = sample_spacing / time_units_per_second
 *
 * @param values   Value vector to modify in-place
 * @param num_trials Number of trials
 * @param sample_spacing Spacing between evaluation points (in data time units)
 * @param time_units_per_second Conversion factor (e.g. 1000.0 for milliseconds)
 */
void toFiringRateHz(std::span<double> values, size_t num_trials,
                    double sample_spacing, double time_units_per_second);

/**
 * @brief Convert raw counts to count-per-trial
 *
 * Formula: values[i] /= num_trials
 *
 * @param values   Value vector to modify in-place
 * @param num_trials Number of trials (no-op if 0)
 */
void toCountPerTrial(std::span<double> values, size_t num_trials);

/**
 * @brief Apply per-unit z-score normalization
 *
 * Formula: values[i] = (values[i] - mean) / std
 * If std is zero (constant signal), all values become 0.
 *
 * @param values Value vector to modify in-place
 */
void zScoreNormalize(std::span<double> values);

/**
 * @brief Apply per-unit min-max normalization to [0, 1]
 *
 * If all values are equal, all become 0.
 *
 * @param values Value vector to modify in-place
 */
void minMaxNormalize(std::span<double> values);

// =============================================================================
// Convenience: ScalingMode dispatch
// =============================================================================

/**
 * @brief Apply a named scaling to a RateEstimate in-place
 *
 * Dispatches to the appropriate normalization primitive based on `mode`.
 * Reads `estimate.metadata.sample_spacing` and `estimate.num_trials`
 * automatically.
 *
 * | ScalingMode       | Transform                                        |
 * |-------------------|--------------------------------------------------|
 * | FiringRateHz      | count / (sample_spacing_s × num_trials)          |
 * | CountPerTrial     | count / num_trials                               |
 * | RawCount          | no transform                                     |
 * | Normalized01      | per-unit (v - min) / (max - min)                 |
 * | ZScore            | per-unit (v - mean) / std (after count/trial)    |
 *
 * @param estimate             RateEstimate to modify in-place
 * @param mode                 Scaling mode to apply
 * @param time_units_per_second Conversion factor (e.g. 1000.0 for ms)
 */
void applyScaling(RateEstimate & estimate,
                  ScalingMode mode,
                  double time_units_per_second);

/**
 * @brief Apply scaling consistently to aggregate + all per-trial curves
 *
 * For z-score: uses the *aggregate* mean/std to normalize all trials,
 * ensuring CI bounds computed afterward are in the same z-score space
 * as the mean.
 *
 * @param data                  RateEstimateWithTrials to modify in-place
 * @param mode                  Scaling mode to apply
 * @param time_units_per_second Conversion factor (e.g. 1000.0 for ms)
 */
void applyScaling(RateEstimateWithTrials & data,
                  ScalingMode mode,
                  double time_units_per_second);

} // namespace WhiskerToolbox::Plots

#endif // RATE_NORMALIZATION_HPP
