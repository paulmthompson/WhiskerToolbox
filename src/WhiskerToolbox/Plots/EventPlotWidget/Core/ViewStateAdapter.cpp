#include "ViewStateAdapter.hpp"

#include "EventPlotState.hpp"

#include "CorePlotting/CoordinateTransform/ViewState.hpp"
#include "CoreGeometry/boundingbox.hpp"

CorePlotting::ViewState toCoreViewState(
    EventPlotViewState const & event_view_state,
    int viewport_width,
    int viewport_height)
{
    CorePlotting::ViewState core_state;

    // Set data bounds (X-axis time range, Y-axis is arbitrary for axis widget)
    core_state.data_bounds = BoundingBox{
        static_cast<float>(event_view_state.x_min),
        -1.0f,  // Y min (not used for axis)
        static_cast<float>(event_view_state.x_max),
        1.0f    // Y max (not used for axis)
    };
    core_state.data_bounds_valid = true;

    // Set viewport dimensions
    core_state.viewport_width = viewport_width;
    core_state.viewport_height = viewport_height;

    // Convert zoom: EventPlotViewState uses direct zoom factors,
    // CorePlotting uses zoom levels where 1.0 = fit to bounds
    // For relative time plots, we want to show the full range by default,
    // so we need to calculate the zoom level that achieves the current zoomed range
    double x_range = event_view_state.x_max - event_view_state.x_min;
    double zoomed_range = x_range / event_view_state.x_zoom;
    
    // Zoom level is the ratio of data bounds to visible range
    // Higher zoom level = more zoomed in
    core_state.zoom_level_x = static_cast<float>(x_range / zoomed_range);
    core_state.zoom_level_y = 1.0f;  // Not used for axis widget

    // Convert pan: EventPlotViewState uses world coordinates,
    // CorePlotting uses normalized offsets relative to data bounds center
    double x_center = (event_view_state.x_min + event_view_state.x_max) / 2.0;
    double pan_offset_world = event_view_state.x_pan;
    
    // Normalize pan offset to data bounds width
    // pan_offset_x in CorePlotting is normalized: 0 = centered, positive = right
    if (x_range > 0) {
        core_state.pan_offset_x = static_cast<float>(pan_offset_world / (x_range / core_state.zoom_level_x));
    } else {
        core_state.pan_offset_x = 0.0f;
    }
    core_state.pan_offset_y = 0.0f;  // Not used for axis widget

    core_state.padding_factor = 1.0f;  // No padding for axis widget

    return core_state;
}
