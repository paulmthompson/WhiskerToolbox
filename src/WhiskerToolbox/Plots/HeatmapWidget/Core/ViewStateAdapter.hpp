#ifndef HEATMAP_VIEW_STATE_ADAPTER_HPP
#define HEATMAP_VIEW_STATE_ADAPTER_HPP

#include "CorePlotting/CoordinateTransform/ViewState.hpp"

#include <cstdint>

struct HeatmapViewState;

CorePlotting::ViewState toCoreViewState(
    HeatmapViewState const & heatmap_view_state,
    int viewport_width,
    int viewport_height);

#endif  // HEATMAP_VIEW_STATE_ADAPTER_HPP
