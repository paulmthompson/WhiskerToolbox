#include "RateUncertainty.hpp"

namespace WhiskerToolbox::Plots {

ConfidenceBand computeSEM(
        RateEstimateWithTrials const & /*data*/,
        double /*k*/)
{
    // Stub — will be implemented in PSTH Phase 3
    return {};
}

ConfidenceBand computePercentileCI(
        RateEstimateWithTrials const & /*data*/,
        double /*lower_pct*/,
        double /*upper_pct*/)
{
    // Stub — will be implemented in PSTH Phase 3
    return {};
}

ConfidenceBand bootstrapCI(
        RateEstimateWithTrials const & /*data*/,
        size_t /*n_resamples*/,
        double /*ci_level*/)
{
    // Stub — will be implemented in PSTH Phase 3
    return {};
}

} // namespace WhiskerToolbox::Plots
