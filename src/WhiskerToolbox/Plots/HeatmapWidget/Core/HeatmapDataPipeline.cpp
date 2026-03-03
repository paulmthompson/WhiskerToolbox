/**
 * @file HeatmapDataPipeline.cpp
 * @brief Implementation of the heatmap data pipeline free function
 */

#include "HeatmapDataPipeline.hpp"

#include "Plots/Common/EventRateEstimation/EventRateEstimation.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <utility>

namespace WhiskerToolbox::Plots {

HeatmapPipelineResult runHeatmapPipeline(
        std::shared_ptr<DataManager> const & data_manager,
        std::vector<std::string> const & unit_keys,
        PlotAlignmentData const & alignment_data,
        HeatmapPipelineConfig const & config) {
    HeatmapPipelineResult result;

    if (!data_manager || unit_keys.empty()) {
        return result;
    }

    if (config.window_size <= 0.0) {
        return result;
    }

    // 1. Gather trial-aligned DigitalEventSeries data for all unit keys
    auto contexts = createUnitGatherContexts(
            data_manager, unit_keys, alignment_data);

    if (contexts.empty()) {
        return result;
    }

    // 2. Estimate firing rates for each unit
    auto rate_estimates = estimateRates(
            contexts, config.window_size, config.estimation_params);

    if (rate_estimates.empty()) {
        return result;
    }

    // 3. Apply scaling
    for (auto & est: rate_estimates) {
        applyScaling(est, config.scaling, config.time_units_per_second);
    }

    // 4. Convert RateEstimate to HeatmapRowData
    result.rows.reserve(rate_estimates.size());
    for (auto & est: rate_estimates) {
        double const spacing = est.metadata.sample_spacing;
        double const left_edge = est.times.empty()
                                         ? 0.0
                                         : est.times.front() - spacing / 2.0;
        result.rows.push_back(CorePlotting::HeatmapRowData{
                .values = est.values,// copy, don't move — keep estimates intact
                .bin_start = left_edge,
                .bin_width = spacing,
        });
    }

    result.rate_estimates = std::move(rate_estimates);
    result.success = true;
    return result;
}

// =============================================================================
// Sorting
// =============================================================================

namespace {

/**
 * @brief Compute the time of the peak value for a rate estimate
 *
 * Returns the time at which the maximum rate occurs. If the rate estimate
 * is empty, returns +infinity so empty rows sort to the end.
 */
double computeTimeToPeak(WhiskerToolbox::Plots::RateEstimate const & estimate) {
    if (estimate.values.empty() || estimate.times.empty()) {
        return std::numeric_limits<double>::infinity();
    }

    auto const max_it = std::max_element(estimate.values.begin(), estimate.values.end());
    auto const idx = static_cast<std::size_t>(
            std::distance(estimate.values.begin(), max_it));
    return (idx < estimate.times.size()) ? estimate.times[idx] : 0.0;
}

/**
 * @brief Compute the peak rate value for a rate estimate
 */
double computePeakRate(WhiskerToolbox::Plots::RateEstimate const & estimate) {
    if (estimate.values.empty()) {
        return -std::numeric_limits<double>::infinity();
    }
    return *std::max_element(estimate.values.begin(), estimate.values.end());
}

/**
 * @brief Compute the mean rate value for a rate estimate
 */
double computeMeanRate(WhiskerToolbox::Plots::RateEstimate const & estimate) {
    if (estimate.values.empty()) {
        return -std::numeric_limits<double>::infinity();
    }
    double const sum = std::accumulate(estimate.values.begin(), estimate.values.end(), 0.0);
    return sum / static_cast<double>(estimate.values.size());
}

}// anonymous namespace

std::vector<std::size_t> computeSortOrder(
        HeatmapPipelineResult const & result,
        std::vector<std::string> const & unit_keys,
        HeatmapSortMode sort_mode,
        bool ascending) {
    auto const n = result.rows.size();
    std::vector<std::size_t> indices(n);
    std::iota(indices.begin(), indices.end(), 0);

    if (sort_mode == HeatmapSortMode::Manual || n <= 1) {
        return indices;
    }

    switch (sort_mode) {
        case HeatmapSortMode::TimeToPeak: {
            // Pre-compute time-to-peak for each row
            std::vector<double> keys(n);
            for (std::size_t i = 0; i < n; ++i) {
                keys[i] = (i < result.rate_estimates.size())
                                  ? computeTimeToPeak(result.rate_estimates[i])
                                  : std::numeric_limits<double>::infinity();
            }
            std::stable_sort(indices.begin(), indices.end(),
                             [&](std::size_t a, std::size_t b) {
                                 return ascending ? keys[a] < keys[b] : keys[a] > keys[b];
                             });
            break;
        }
        case HeatmapSortMode::PeakRate: {
            std::vector<double> keys(n);
            for (std::size_t i = 0; i < n; ++i) {
                keys[i] = (i < result.rate_estimates.size())
                                  ? computePeakRate(result.rate_estimates[i])
                                  : -std::numeric_limits<double>::infinity();
            }
            // Default: descending (highest peak first)
            std::stable_sort(indices.begin(), indices.end(),
                             [&](std::size_t a, std::size_t b) {
                                 return ascending ? keys[a] < keys[b] : keys[a] > keys[b];
                             });
            break;
        }
        case HeatmapSortMode::MeanRate: {
            std::vector<double> keys(n);
            for (std::size_t i = 0; i < n; ++i) {
                keys[i] = (i < result.rate_estimates.size())
                                  ? computeMeanRate(result.rate_estimates[i])
                                  : -std::numeric_limits<double>::infinity();
            }
            // Default: descending (highest mean first)
            std::stable_sort(indices.begin(), indices.end(),
                             [&](std::size_t a, std::size_t b) {
                                 return ascending ? keys[a] < keys[b] : keys[a] > keys[b];
                             });
            break;
        }
        case HeatmapSortMode::Alphabetical: {
            std::stable_sort(indices.begin(), indices.end(),
                             [&](std::size_t a, std::size_t b) {
                                 auto const & ka = (a < unit_keys.size()) ? unit_keys[a] : "";
                                 auto const & kb = (b < unit_keys.size()) ? unit_keys[b] : "";
                                 return ascending ? ka < kb : ka > kb;
                             });
            break;
        }
        case HeatmapSortMode::Manual:
            // Already handled above
            break;
    }

    return indices;
}

void applySortOrder(
        HeatmapPipelineResult & result,
        std::vector<std::string> & unit_keys,
        std::vector<std::size_t> const & sort_indices) {
    if (sort_indices.empty()) {
        return;
    }

    auto const n = sort_indices.size();

    // Reorder rows
    if (!result.rows.empty()) {
        std::vector<CorePlotting::HeatmapRowData> sorted_rows;
        sorted_rows.reserve(n);
        for (auto const idx: sort_indices) {
            if (idx < result.rows.size()) {
                sorted_rows.push_back(std::move(result.rows[idx]));
            }
        }
        result.rows = std::move(sorted_rows);
    }

    // Reorder rate estimates
    if (!result.rate_estimates.empty()) {
        std::vector<WhiskerToolbox::Plots::RateEstimate> sorted_estimates;
        sorted_estimates.reserve(n);
        for (auto const idx: sort_indices) {
            if (idx < result.rate_estimates.size()) {
                sorted_estimates.push_back(std::move(result.rate_estimates[idx]));
            }
        }
        result.rate_estimates = std::move(sorted_estimates);
    }

    // Reorder unit keys
    if (!unit_keys.empty()) {
        std::vector<std::string> sorted_keys;
        sorted_keys.reserve(n);
        for (auto const idx: sort_indices) {
            if (idx < unit_keys.size()) {
                sorted_keys.push_back(std::move(unit_keys[idx]));
            }
        }
        unit_keys = std::move(sorted_keys);
    }
}

}// namespace WhiskerToolbox::Plots
