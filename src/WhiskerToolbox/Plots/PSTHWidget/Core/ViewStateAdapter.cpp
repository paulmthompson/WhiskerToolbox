#include "ViewStateAdapter.hpp"

#include "PSTHState.hpp"

#include "CorePlotting/CoordinateTransform/ViewState.hpp"
#include "CoreGeometry/boundingbox.hpp"

CorePlotting::ViewState toCoreViewState(
    PSTHState const * psth_state,
    int viewport_width,
    int viewport_height)
{
    CorePlotting::ViewState core_state;

    if (!psth_state) {
        // Return default state if no state provided
        core_state.data_bounds = BoundingBox{-500.0f, -1.0f, 500.0f, 1.0f};
        core_state.data_bounds_valid = true;
        core_state.viewport_width = viewport_width;
        core_state.viewport_height = viewport_height;
        core_state.zoom_level_x = 1.0f;
        core_state.zoom_level_y = 1.0f;
        core_state.pan_offset_x = 0.0f;
        core_state.pan_offset_y = 0.0f;
        core_state.padding_factor = 1.0f;
        return core_state;
    }

    // Get window size from alignment state
    double window_size = psth_state->getWindowSize();
    double half_window = window_size / 2.0;

    // Set data bounds (X-axis time range, centered at 0)
    core_state.data_bounds = BoundingBox{
        static_cast<float>(-half_window),
        -1.0f,  // Y min (not used for axis)
        static_cast<float>(half_window),
        1.0f    // Y max (not used for axis)
    };
    core_state.data_bounds_valid = true;

    // Set viewport dimensions
    core_state.viewport_width = viewport_width;
    core_state.viewport_height = viewport_height;

    // For PSTH, we show the full window by default (no zoom/pan for now)
    core_state.zoom_level_x = 1.0f;
    core_state.zoom_level_y = 1.0f;
    core_state.pan_offset_x = 0.0f;
    core_state.pan_offset_y = 0.0f;
    core_state.padding_factor = 1.0f;

    return core_state;
}
