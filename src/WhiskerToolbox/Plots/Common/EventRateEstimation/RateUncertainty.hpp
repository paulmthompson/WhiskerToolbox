#ifndef RATE_UNCERTAINTY_HPP
#define RATE_UNCERTAINTY_HPP

/**
 * @file RateUncertainty.hpp
 * @brief Confidence band computation from per-trial rate estimates
 *
 * Provides functions to compute confidence bands from `RateEstimateWithTrials`.
 * These are stubs — the implementations will be filled in when the PSTH widget
 * gains variability display (PSTH roadmap Phase 3).
 *
 * @see RateEstimate.hpp for PerTrialData and RateEstimateWithTrials
 * @see RateNormalization.hpp for applying scaling before CI computation
 */

#include "RateEstimate.hpp"

#include <cstddef>
#include <vector>

namespace WhiskerToolbox::Plots {

/**
 * @brief Lower and upper bounds at each time point
 */
struct ConfidenceBand {
    std::vector<double> lower;    ///< Lower bound at each time point
    std::vector<double> upper;    ///< Upper bound at each time point
};

/**
 * @brief Compute SEM-based confidence band as mean ± k × SEM
 *
 * @param data  RateEstimateWithTrials (must have per-trial data)
 * @param k     Multiplier for SEM (1.0 for ±1 SEM, 1.96 for 95% CI)
 * @return ConfidenceBand with lower and upper bounds
 *
 * @note Stub — returns empty bands. Implementation deferred to PSTH Phase 3.
 */
[[nodiscard]] ConfidenceBand computeSEM(
        RateEstimateWithTrials const & data,
        double k = 1.0);

/**
 * @brief Compute percentile-based confidence band
 *
 * @param data       RateEstimateWithTrials (must have per-trial data)
 * @param lower_pct  Lower percentile (e.g. 2.5 for 95% CI)
 * @param upper_pct  Upper percentile (e.g. 97.5 for 95% CI)
 * @return ConfidenceBand with lower and upper bounds
 *
 * @note Stub — returns empty bands. Implementation deferred to PSTH Phase 3.
 */
[[nodiscard]] ConfidenceBand computePercentileCI(
        RateEstimateWithTrials const & data,
        double lower_pct = 2.5,
        double upper_pct = 97.5);

/**
 * @brief Bootstrap confidence band (resamples trials)
 *
 * @param data         RateEstimateWithTrials (must have per-trial data)
 * @param n_resamples  Number of bootstrap resamples
 * @param ci_level     Confidence level (e.g. 0.95 for 95% CI)
 * @return ConfidenceBand with lower and upper bounds
 *
 * @note Stub — returns empty bands. Implementation deferred to PSTH Phase 3.
 */
[[nodiscard]] ConfidenceBand bootstrapCI(
        RateEstimateWithTrials const & data,
        size_t n_resamples = 1000,
        double ci_level = 0.95);

} // namespace WhiskerToolbox::Plots

#endif // RATE_UNCERTAINTY_HPP
