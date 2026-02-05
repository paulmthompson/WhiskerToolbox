#ifndef VIEW_STATE_ADAPTER_HPP
#define VIEW_STATE_ADAPTER_HPP

/**
 * @file ViewStateAdapter.hpp
 * @brief Helper functions to convert EventPlotViewState to CorePlotting::ViewState
 * 
 * Provides adapters to convert EventPlotViewState to CorePlotting::ViewState
 * for use with RelativeTimeAxisWidget and other CorePlotting components.
 */

#include "CorePlotting/CoordinateTransform/ViewState.hpp"

#include <cstdint>

// Forward declaration
struct EventPlotViewState;

/**
 * @brief Convert EventPlotViewState to CorePlotting::ViewState
 * 
 * Creates a CorePlotting::ViewState from EventPlotViewState for use with
 * RelativeTimeAxisWidget and other CorePlotting components.
 * 
 * @param event_view_state The EventPlotViewState to convert
 * @param viewport_width Width of the viewport in pixels
 * @param viewport_height Height of the viewport in pixels
 * @return CorePlotting::ViewState representation
 */
CorePlotting::ViewState toCoreViewState(
    EventPlotViewState const & event_view_state,
    int viewport_width,
    int viewport_height);

#endif  // VIEW_STATE_ADAPTER_HPP
