#ifndef VIEW_STATE_ADAPTER_HPP
#define VIEW_STATE_ADAPTER_HPP

/**
 * @file ViewStateAdapter.hpp
 * @brief Helper functions to convert HeatmapState to CorePlotting::ViewState
 * 
 * Provides adapters to convert HeatmapState window size to CorePlotting::ViewState
 * for use with RelativeTimeAxisWidget and other CorePlotting components.
 */

#include "CorePlotting/CoordinateTransform/ViewState.hpp"

#include <cstdint>

// Forward declaration
class HeatmapState;

/**
 * @brief Convert HeatmapState to CorePlotting::ViewState
 * 
 * Creates a CorePlotting::ViewState from HeatmapState for use with
 * RelativeTimeAxisWidget and other CorePlotting components.
 * 
 * The view state is based on the window size from the alignment state,
 * centered around zero (the alignment point).
 * 
 * @param state The HeatmapState to convert
 * @param viewport_width Width of the viewport in pixels
 * @param viewport_height Height of the viewport in pixels
 * @return CorePlotting::ViewState representation
 */
CorePlotting::ViewState toCoreViewState(
    HeatmapState const * state,
    int viewport_width,
    int viewport_height);

#endif  // VIEW_STATE_ADAPTER_HPP
