/**
 * @file LineBatchBuilder.hpp
 * @brief Builder helpers to populate LineBatchData from different data sources
 *
 * Free functions that convert LineData (TemporalProjectionViewWidget) and
 * GatherResult<AnalogTimeSeries> (LinePlotWidget) into the flat segment
 * representation consumed by the batch rendering and intersection systems.
 *
 * Part of the CorePlotting layer — no OpenGL or Qt dependencies.
 */
#ifndef COREPLOTTING_LINEBATCH_LINEBATCHBUILDER_HPP
#define COREPLOTTING_LINEBATCH_LINEBATCHBUILDER_HPP

#include "LineBatchData.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/utils/GatherResult.hpp"

#include <cstdint>
#include <vector>

namespace CorePlotting {

/**
 * @brief Build a LineBatchData from LineData (TemporalProjectionViewWidget use case).
 *
 * Each line in the LineData becomes a logical line in the batch.
 * EntityIds are preserved in LineInfo::entity_id.
 * The line coordinates are stored as-is (world space).
 *
 * @param line_data     The source line data.
 * @param canvas_width  Width of the canvas (stored in output).
 * @param canvas_height Height of the canvas (stored in output).
 * @return Populated LineBatchData with all lines visible and none selected.
 */
[[nodiscard]] LineBatchData buildLineBatchFromLineData(
    LineData const & line_data,
    float canvas_width,
    float canvas_height);

/**
 * @brief Build a LineBatchData from GatherResult<AnalogTimeSeries> (LinePlotWidget use case).
 *
 * Each trial in the GatherResult becomes a logical line in the batch.
 * Trial indices are stored in LineInfo::trial_index.
 *
 * The x-coordinates are the TimeFrameIndex values (relative to the alignment
 * time for that trial), and y-coordinates are the analog sample values.
 *
 * @param gathered        The gathered analog time series (one per trial).
 * @param alignment_times Per-trial alignment times.  X = timeFrameIndex - alignment_times[trial].
 *                        Must have the same size as @p gathered.
 * @return Populated LineBatchData with all lines visible and none selected.
 */
[[nodiscard]] LineBatchData buildLineBatchFromGatherResult(
    GatherResult<AnalogTimeSeries> const & gathered,
    std::vector<std::int64_t> const & alignment_times);

} // namespace CorePlotting

#endif // COREPLOTTING_LINEBATCH_LINEBATCHBUILDER_HPP
