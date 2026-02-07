#ifndef LINE_PLOT_VIEW_STATE_ADAPTER_HPP
#define LINE_PLOT_VIEW_STATE_ADAPTER_HPP

/**
 * @file ViewStateAdapter.hpp
 * @brief Helper functions to convert LinePlotViewState to CorePlotting::ViewState
 *
 * Handles zoom/pan conversion in the same way as the PSTH adapter.
 */

#include "CorePlotting/CoordinateTransform/ViewState.hpp"

struct LinePlotViewState;

/**
 * @brief Convert LinePlotViewState to CorePlotting::ViewState
 *
 * @param view_state The LinePlotViewState (data bounds + zoom/pan)
 * @param viewport_width  Width of the viewport in pixels
 * @param viewport_height Height of the viewport in pixels
 * @return CorePlotting::ViewState representation
 */
CorePlotting::ViewState toCoreViewState(
    LinePlotViewState const & view_state,
    int viewport_width,
    int viewport_height);

#endif  // LINE_PLOT_VIEW_STATE_ADAPTER_HPP
