#ifndef COREPLOTTING_LAYOUT_STACKEDLAYOUTSTRATEGY_HPP
#define COREPLOTTING_LAYOUT_STACKEDLAYOUTSTRATEGY_HPP

#include "LayoutEngine.hpp"

namespace CorePlotting {

/**
 * @brief Vertical stacking layout strategy (DataViewer style)
 * 
 * Stacks series vertically with equal height allocation. This is the default
 * layout for DataViewer where analog time series and digital event series
 * share the canvas in a stacked arrangement.
 * 
 * LAYOUT RULES:
 * 1. Stackable series (analog + stacked events) divide viewport equally
 * 2. Non-stackable series (full-canvas digital intervals) span entire viewport
 * 3. Series are ordered top-to-bottom by their index in the request
 * 4. Global zoom/scale/pan are applied uniformly to all series
 * 
 * COORDINATE SYSTEM:
 * - Y coordinates are in viewport space (typically -1 to +1 NDC)
 * - Heights are in viewport units
 * - Series centers are positioned to create even vertical spacing
 */
class StackedLayoutStrategy : public ILayoutStrategy {
public:
    /**
     * @brief Compute stacked vertical layout
     * 
     * @param request Layout parameters and series list
     * @return Layout positions for each series
     */
    LayoutResponse compute(LayoutRequest const & request) const override;

private:
    /**
     * @brief Calculate layout for stackable series
     * 
     * Stackable series include:
     * - All analog time series
     * - Digital event series marked as stackable
     * 
     * These series divide the viewport height equally among themselves.
     * 
     * @param series_info Series metadata
     * @param series_index Global index in series list
     * @param stackable_index Index among stackable series only
     * @param total_stackable Total number of stackable series
     * @param request Layout parameters
     * @return Computed layout for this series
     */
    [[nodiscard]] SeriesLayout computeStackableLayout(
            SeriesInfo const & series_info,
            int series_index,
            int stackable_index,
            int total_stackable,
            LayoutRequest const & request) const;

    /**
     * @brief Calculate layout for full-canvas series
     * 
     * Full-canvas series include:
     * - Digital interval series
     * - Digital event series marked as non-stackable
     * 
     * These series span the entire viewport height for maximum visibility.
     * 
     * @param series_info Series metadata
     * @param series_index Global index in series list
     * @param request Layout parameters
     * @return Computed layout for this series
     */
    [[nodiscard]] SeriesLayout computeFullCanvasLayout(
            SeriesInfo const & series_info,
            int series_index,
            LayoutRequest const & request) const;
};

}// namespace CorePlotting

#endif// COREPLOTTING_LAYOUT_STACKEDLAYOUTSTRATEGY_HPP
