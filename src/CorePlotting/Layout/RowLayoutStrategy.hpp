#ifndef COREPLOTTING_LAYOUT_ROWLAYOUTSTRATEGY_HPP
#define COREPLOTTING_LAYOUT_ROWLAYOUTSTRATEGY_HPP

#include "LayoutEngine.hpp"

namespace CorePlotting {

/**
 * @brief Horizontal row layout strategy (Raster plot style)
 * 
 * Arranges series in horizontal rows, typically for raster plots where each
 * row represents a trial or condition. All rows have equal height and are
 * stacked top-to-bottom.
 * 
 * LAYOUT RULES:
 * 1. Each series gets one row
 * 2. All rows have equal height (viewport divided equally)
 * 3. Rows are ordered top-to-bottom by series index
 * 4. No concept of "stackable" vs "full-canvas" — all series are rows
 * 
 * COORDINATE SYSTEM:
 * - Y coordinates are in viewport space (typically -1 to +1 NDC)
 * - Row heights are in viewport units
 * - Row centers are positioned to create even vertical spacing
 * 
 * USE CASES:
 * - Raster plots (events across trials)
 * - Multi-trial time series
 * - Condition-based grouping
 */
class RowLayoutStrategy : public ILayoutStrategy {
public:
    /**
     * @brief Compute row-based layout
     * 
     * @param request Layout parameters and series list
     * @return Layout positions for each series (one row per series)
     */
    LayoutResponse compute(LayoutRequest const & request) const override;

private:
    /**
     * @brief Calculate layout for a single row
     * 
     * @param series_info Series metadata
     * @param row_index Index of this row (0-based from top)
     * @param total_rows Total number of rows
     * @param request Layout parameters
     * @return Computed layout for this row
     */
    [[nodiscard]] SeriesLayout computeRowLayout(
            SeriesInfo const & series_info,
            int row_index,
            int total_rows,
            LayoutRequest const & request) const;
};

}// namespace CorePlotting

#endif// COREPLOTTING_LAYOUT_ROWLAYOUTSTRATEGY_HPP
