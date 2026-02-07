#include "ViewStateAdapter.hpp"

#include "PSTHState.hpp"

#include "CorePlotting/CoordinateTransform/ViewState.hpp"
#include "CoreGeometry/boundingbox.hpp"

CorePlotting::ViewState toCoreViewState(
    PSTHViewState const & vs,
    int viewport_width,
    int viewport_height)
{
    CorePlotting::ViewState core_state;

    core_state.data_bounds = BoundingBox{
        static_cast<float>(vs.x_min),
        -1.0f,
        static_cast<float>(vs.x_max),
        1.0f};
    core_state.data_bounds_valid = true;

    core_state.viewport_width = viewport_width;
    core_state.viewport_height = viewport_height;

    // Zoom: same conversion as EventPlot adapter
    double const x_range = vs.x_max - vs.x_min;
    double const zoomed_range = x_range / vs.x_zoom;
    core_state.zoom_level_x = static_cast<float>(x_range / zoomed_range);
    core_state.zoom_level_y = 1.0f;

    // Pan: normalize to visible range
    if (x_range > 0) {
        core_state.pan_offset_x = static_cast<float>(
            vs.x_pan / (x_range / core_state.zoom_level_x));
    } else {
        core_state.pan_offset_x = 0.0f;
    }
    core_state.pan_offset_y = 0.0f;

    core_state.padding_factor = 1.0f;

    return core_state;
}
