#include "RateNormalization.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace WhiskerToolbox::Plots {

// =============================================================================
// Normalization primitives
// =============================================================================

void toFiringRateHz(std::span<double> values, size_t num_trials,
                    double sample_spacing, double time_units_per_second)
{
    if (num_trials == 0 || sample_spacing <= 0.0 || time_units_per_second <= 0.0) {
        return;
    }
    double const spacing_s = sample_spacing / time_units_per_second;
    double const divisor = static_cast<double>(num_trials) * spacing_s;
    for (auto & v : values) {
        v /= divisor;
    }
}

void toCountPerTrial(std::span<double> values, size_t num_trials)
{
    if (num_trials == 0) {
        return;
    }
    double const divisor = static_cast<double>(num_trials);
    for (auto & v : values) {
        v /= divisor;
    }
}

void zScoreNormalize(std::span<double> values)
{
    if (values.empty()) {
        return;
    }
    auto const n = static_cast<double>(values.size());
    double const sum = std::accumulate(values.begin(), values.end(), 0.0);
    double const mean = sum / n;

    double sq_sum = 0.0;
    for (double v : values) {
        double const diff = v - mean;
        sq_sum += diff * diff;
    }
    double const std_dev = std::sqrt(sq_sum / n);

    if (std_dev > 0.0) {
        for (auto & v : values) {
            v = (v - mean) / std_dev;
        }
    } else {
        std::fill(values.begin(), values.end(), 0.0);
    }
}

void minMaxNormalize(std::span<double> values)
{
    if (values.empty()) {
        return;
    }
    auto const [min_it, max_it] = std::minmax_element(values.begin(), values.end());
    double const vmin = *min_it;
    double const vmax = *max_it;
    double const range = vmax - vmin;

    if (range > 0.0) {
        for (auto & v : values) {
            v = (v - vmin) / range;
        }
    } else {
        std::fill(values.begin(), values.end(), 0.0);
    }
}

// =============================================================================
// Convenience: ScalingMode dispatch
// =============================================================================

void applyScaling(RateEstimate & estimate,
                  ScalingMode mode,
                  double time_units_per_second)
{
    switch (mode) {
        case ScalingMode::RawCount:
            // No transform
            break;

        case ScalingMode::CountPerTrial:
            toCountPerTrial(estimate.values, estimate.num_trials);
            break;

        case ScalingMode::FiringRateHz:
            toFiringRateHz(estimate.values, estimate.num_trials,
                           estimate.metadata.sample_spacing, time_units_per_second);
            break;

        case ScalingMode::ZScore:
            // First normalize to count-per-trial, then z-score
            toCountPerTrial(estimate.values, estimate.num_trials);
            zScoreNormalize(estimate.values);
            break;

        case ScalingMode::Normalized01:
            // First normalize to count-per-trial, then min-max
            toCountPerTrial(estimate.values, estimate.num_trials);
            minMaxNormalize(estimate.values);
            break;
    }
}

namespace {

/**
 * @brief Compute mean and std of a span of doubles
 */
struct MeanStd {
    double mean = 0.0;
    double std_dev = 0.0;
};

[[nodiscard]] MeanStd computeMeanStd(std::span<double const> values)
{
    if (values.empty()) {
        return {};
    }
    auto const n = static_cast<double>(values.size());
    double const sum = std::accumulate(values.begin(), values.end(), 0.0);
    double const mean = sum / n;

    double sq_sum = 0.0;
    for (double v : values) {
        double const diff = v - mean;
        sq_sum += diff * diff;
    }
    return {mean, std::sqrt(sq_sum / n)};
}

/**
 * @brief Z-score a span using externally provided mean and std
 */
void zScoreWithStats(std::span<double> values, double mean, double std_dev)
{
    if (std_dev > 0.0) {
        for (auto & v : values) {
            v = (v - mean) / std_dev;
        }
    } else {
        std::fill(values.begin(), values.end(), 0.0);
    }
}

/**
 * @brief Min-max normalize a span using externally provided min and max
 */
void minMaxWithStats(std::span<double> values, double vmin, double vmax)
{
    double const range = vmax - vmin;
    if (range > 0.0) {
        for (auto & v : values) {
            v = (v - vmin) / range;
        }
    } else {
        std::fill(values.begin(), values.end(), 0.0);
    }
}

} // anonymous namespace

void applyScaling(RateEstimateWithTrials & data,
                  ScalingMode mode,
                  double time_units_per_second)
{
    auto & est = data.estimate;
    auto & trials = data.trials.per_trial_values;

    switch (mode) {
        case ScalingMode::RawCount:
            // No transform
            break;

        case ScalingMode::CountPerTrial:
            toCountPerTrial(est.values, est.num_trials);
            // Per-trial curves are already single-trial counts — no division needed
            break;

        case ScalingMode::FiringRateHz:
            toFiringRateHz(est.values, est.num_trials,
                           est.metadata.sample_spacing, time_units_per_second);
            for (auto & trial : trials) {
                // Each trial's values are raw counts from a single trial
                toFiringRateHz(trial, 1, est.metadata.sample_spacing,
                               time_units_per_second);
            }
            break;

        case ScalingMode::ZScore: {
            // First normalize aggregate to count-per-trial
            toCountPerTrial(est.values, est.num_trials);
            // Compute stats from the aggregate BEFORE z-scoring
            auto const stats = computeMeanStd(
                    std::span<double const>{est.values.data(), est.values.size()});
            // Z-score the aggregate
            zScoreWithStats(est.values, stats.mean, stats.std_dev);
            // Z-score each trial using the AGGREGATE stats
            for (auto & trial : trials) {
                // Per-trial values are already single-trial counts; no need
                // to divide by trial count, but normalize to same units
                // (per-trial values are already "count per trial" for 1 trial)
                zScoreWithStats(trial, stats.mean, stats.std_dev);
            }
            break;
        }

        case ScalingMode::Normalized01: {
            // First normalize aggregate to count-per-trial
            toCountPerTrial(est.values, est.num_trials);
            // Get min/max from aggregate BEFORE normalizing
            if (!est.values.empty()) {
                auto const [min_it, max_it] = std::minmax_element(
                        est.values.begin(), est.values.end());
                double const vmin = *min_it;
                double const vmax = *max_it;
                // Normalize aggregate
                minMaxWithStats(est.values, vmin, vmax);
                // Normalize each trial using AGGREGATE min/max
                for (auto & trial : trials) {
                    minMaxWithStats(trial, vmin, vmax);
                }
            }
            break;
        }
    }
}

} // namespace WhiskerToolbox::Plots
