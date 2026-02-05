#ifndef LINE_PLOT_VIEW_STATE_ADAPTER_HPP
#define LINE_PLOT_VIEW_STATE_ADAPTER_HPP

/**
 * @file ViewStateAdapter.hpp
 * @brief Helper functions to convert LinePlot alignment data to CorePlotting::ViewState
 * 
 * Provides adapters to convert LinePlotState alignment settings to CorePlotting::ViewState
 * for use with RelativeTimeAxisWidget and other CorePlotting components.
 */

#include "CorePlotting/CoordinateTransform/ViewState.hpp"

#include <cstdint>

// Forward declaration
class LinePlotState;

/**
 * @brief Convert LinePlot alignment data to CorePlotting::ViewState
 * 
 * Creates a CorePlotting::ViewState from LinePlotState alignment settings for use with
 * RelativeTimeAxisWidget and other CorePlotting components.
 * 
 * The view bounds are centered at 0 (alignment point) and extend ±window_size/2.
 * 
 * @param line_plot_state The LinePlotState to get alignment data from
 * @param viewport_width Width of the viewport in pixels
 * @param viewport_height Height of the viewport in pixels
 * @return CorePlotting::ViewState representation
 */
CorePlotting::ViewState toCoreViewState(
    LinePlotState const * line_plot_state,
    int viewport_width,
    int viewport_height);

#endif  // LINE_PLOT_VIEW_STATE_ADAPTER_HPP
