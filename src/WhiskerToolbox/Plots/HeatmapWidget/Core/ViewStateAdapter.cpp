#include "ViewStateAdapter.hpp"

#include "HeatmapState.hpp"

#include "CorePlotting/CoordinateTransform/ViewState.hpp"
#include "CoreGeometry/boundingbox.hpp"

CorePlotting::ViewState toCoreViewState(
    HeatmapViewState const & heatmap_view_state,
    int viewport_width,
    int viewport_height)
{
    CorePlotting::ViewState core_state;

    core_state.data_bounds = BoundingBox{
        static_cast<float>(heatmap_view_state.x_min),
        -1.0f,
        static_cast<float>(heatmap_view_state.x_max),
        1.0f
    };
    core_state.data_bounds_valid = true;

    core_state.viewport_width = viewport_width;
    core_state.viewport_height = viewport_height;

    double x_range = heatmap_view_state.x_max - heatmap_view_state.x_min;
    double zoomed_range = x_range / heatmap_view_state.x_zoom;

    core_state.zoom_level_x = static_cast<float>(x_range / zoomed_range);
    core_state.zoom_level_y = 1.0f;

    double pan_offset_world = heatmap_view_state.x_pan;

    if (x_range > 0) {
        core_state.pan_offset_x = static_cast<float>(pan_offset_world / (x_range / core_state.zoom_level_x));
    } else {
        core_state.pan_offset_x = 0.0f;
    }
    core_state.pan_offset_y = 0.0f;

    core_state.padding_factor = 1.0f;

    return core_state;
}
