/**
 * @file HeatmapDataPipeline.hpp
 * @brief Pure-data pipeline for building heatmap row data from DataManager
 *
 * Extracts the data-transformation logic from HeatmapOpenGLWidget::rebuildScene()
 * into a free function that can be tested without an OpenGL context.
 *
 * The pipeline performs: gather → estimate rates → scale → convert to HeatmapRowData.
 *
 * @see HeatmapOpenGLWidget::rebuildScene() — the original consumer
 * @see HeatmapMapper — downstream consumer of HeatmapRowData
 */

#ifndef HEATMAP_DATA_PIPELINE_HPP
#define HEATMAP_DATA_PIPELINE_HPP

#include "CorePlotting/Colormaps/Colormap.hpp"
#include "CorePlotting/Mappers/HeatmapMapper.hpp"
#include "Plots/Common/EventRateEstimation/EstimationParams.hpp"
#include "Plots/Common/EventRateEstimation/RateEstimate.hpp"
#include "Plots/Common/PlotAlignmentWidget/Core/PlotAlignmentData.hpp"

#include <memory>
#include <string>
#include <vector>

class DataManager;

namespace WhiskerToolbox::Plots {

/**
 * @brief Configuration for the heatmap data pipeline
 */
struct HeatmapPipelineConfig {
    double window_size = 1000.0;
    ScalingMode scaling = ScalingMode::FiringRateHz;
    EstimationParams estimation_params = BinningParams{};
    double time_units_per_second = 1000.0;
};

/**
 * @brief Result of the heatmap data pipeline
 *
 * Contains the row data suitable for HeatmapMapper and the intermediate
 * rate estimates (for debugging / inspection).
 */
struct HeatmapPipelineResult {
    std::vector<CorePlotting::HeatmapRowData> rows;
    std::vector<RateEstimate> rate_estimates;///< Before conversion to rows
    bool success = false;
};

/**
 * @brief Execute the heatmap data pipeline without an OpenGL context
 *
 * This function replicates the data flow from HeatmapOpenGLWidget::rebuildScene()
 * as a testable free function:
 *
 *  1. Gather trial-aligned DigitalEventSeries data for all unit keys
 *  2. Estimate firing rates for each unit
 *  3. Apply scaling (Hz, z-score, etc.)
 *  4. Convert RateEstimate to CorePlotting::HeatmapRowData
 *
 * @param data_manager   DataManager with loaded event and alignment data
 * @param unit_keys      DigitalEventSeries keys (one per row)
 * @param alignment_data Alignment configuration (event key, window size, etc.)
 * @param config         Pipeline configuration (scaling, estimation params, etc.)
 * @return Pipeline result with rows and intermediate data; success is false when
 *         the pipeline produces no output (e.g. empty gather, no alignment).
 */
[[nodiscard]] HeatmapPipelineResult runHeatmapPipeline(
        std::shared_ptr<DataManager> const & data_manager,
        std::vector<std::string> const & unit_keys,
        PlotAlignmentData const & alignment_data,
        HeatmapPipelineConfig const & config);

}// namespace WhiskerToolbox::Plots

#endif// HEATMAP_DATA_PIPELINE_HPP
