#ifndef PSTH_VIEW_STATE_ADAPTER_HPP
#define PSTH_VIEW_STATE_ADAPTER_HPP

/**
 * @file ViewStateAdapter.hpp
 * @brief Helper functions to convert PSTHViewState to CorePlotting::ViewState
 */

#include "CorePlotting/CoordinateTransform/ViewState.hpp"

struct PSTHViewState;

/**
 * @brief Convert PSTHViewState to CorePlotting::ViewState
 *
 * Handles zoom/pan conversion in the same way as the EventPlot adapter.
 *
 * @param view_state The PSTHViewState (data bounds + zoom/pan)
 * @param viewport_width  Width of the viewport in pixels
 * @param viewport_height Height of the viewport in pixels
 * @return CorePlotting::ViewState representation
 */
CorePlotting::ViewState toCoreViewState(
    PSTHViewState const & view_state,
    int viewport_width,
    int viewport_height);

#endif  // PSTH_VIEW_STATE_ADAPTER_HPP
