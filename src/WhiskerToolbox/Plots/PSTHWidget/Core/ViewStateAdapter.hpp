#ifndef PSTH_VIEW_STATE_ADAPTER_HPP
#define PSTH_VIEW_STATE_ADAPTER_HPP

/**
 * @file ViewStateAdapter.hpp
 * @brief Helper functions to convert PSTH alignment data to CorePlotting::ViewState
 * 
 * Provides adapters to convert PSTH alignment settings to CorePlotting::ViewState
 * for use with RelativeTimeAxisWidget and other CorePlotting components.
 */

#include "CorePlotting/CoordinateTransform/ViewState.hpp"

#include <cstdint>

// Forward declaration
class PSTHState;

/**
 * @brief Convert PSTH alignment data to CorePlotting::ViewState
 * 
 * Creates a CorePlotting::ViewState from PSTHState alignment settings for use with
 * RelativeTimeAxisWidget and other CorePlotting components.
 * 
 * The view bounds are centered at 0 (alignment point) and extend ±window_size/2.
 * 
 * @param psth_state The PSTHState to get alignment data from
 * @param viewport_width Width of the viewport in pixels
 * @param viewport_height Height of the viewport in pixels
 * @return CorePlotting::ViewState representation
 */
CorePlotting::ViewState toCoreViewState(
    PSTHState const * psth_state,
    int viewport_width,
    int viewport_height);

#endif  // PSTH_VIEW_STATE_ADAPTER_HPP
