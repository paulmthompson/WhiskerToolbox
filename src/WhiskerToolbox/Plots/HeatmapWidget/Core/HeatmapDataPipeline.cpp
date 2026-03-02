/**
 * @file HeatmapDataPipeline.cpp
 * @brief Implementation of the heatmap data pipeline free function
 */

#include "HeatmapDataPipeline.hpp"

#include "Plots/Common/EventRateEstimation/EventRateEstimation.hpp"

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

}// namespace WhiskerToolbox::Plots
