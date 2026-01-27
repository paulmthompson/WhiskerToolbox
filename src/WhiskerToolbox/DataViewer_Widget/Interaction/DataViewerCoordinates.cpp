#include "DataViewerCoordinates.hpp"

#include "CorePlotting/CoordinateTransform/TimeAxisCoordinates.hpp"

namespace DataViewer {

DataViewerCoordinates::DataViewerCoordinates(
        CorePlotting::TimeSeriesViewState const & view_state,
        int width, int height)
    : _time_params(view_state.time_start, view_state.time_end, width),
      _y_params(view_state.y_min, view_state.y_max, height, view_state.vertical_pan_offset) {
}

// ============================================================================
// Canvas to World/Time Conversions
// ============================================================================

float DataViewerCoordinates::canvasXToTime(float canvas_x) const {
    return CorePlotting::canvasXToTime(canvas_x, _time_params);
}

float DataViewerCoordinates::canvasXToWorldX(float canvas_x) const {
    // For DataViewer, world X is equivalent to time
    return canvasXToTime(canvas_x);
}

float DataViewerCoordinates::canvasYToWorldY(float canvas_y) const {
    return CorePlotting::canvasYToWorldY(canvas_y, _y_params);
}

std::pair<float, float> DataViewerCoordinates::canvasToWorld(float canvas_x, float canvas_y) const {
    return {canvasXToWorldX(canvas_x), canvasYToWorldY(canvas_y)};
}

// ============================================================================
// World/Time to Canvas Conversions
// ============================================================================

float DataViewerCoordinates::timeToCanvasX(float time) const {
    return CorePlotting::timeToCanvasX(time, _time_params);
}

float DataViewerCoordinates::worldYToCanvasY(float world_y) const {
    return CorePlotting::worldYToCanvasY(world_y, _y_params);
}

// ============================================================================
// Data Value Conversions
// ============================================================================

float DataViewerCoordinates::canvasYToAnalogValue(
        float canvas_y,
        CorePlotting::LayoutTransform const & y_transform) const {
    // Convert canvas Y to world Y
    float const world_y = canvasYToWorldY(canvas_y);

    // Use inverse transform to get data value
    return y_transform.inverse(world_y);
}

// ============================================================================
// Tolerance Conversions
// ============================================================================

float DataViewerCoordinates::pixelToleranceToWorldX(float pixels) const {
    return pixels * timeUnitsPerPixel();
}

float DataViewerCoordinates::pixelToleranceToWorldY(float pixels) const {
    return pixels * worldYUnitsPerPixel();
}

// ============================================================================
// Accessors
// ============================================================================

float DataViewerCoordinates::timeUnitsPerPixel() const {
    return CorePlotting::timeUnitsPerPixel(_time_params);
}

float DataViewerCoordinates::worldYUnitsPerPixel() const {
    if (_y_params.viewport_height_px <= 0) {
        return 0.0f;
    }

    float const y_span = _y_params.world_y_max - _y_params.world_y_min;
    return y_span / static_cast<float>(_y_params.viewport_height_px);
}

}// namespace DataViewer
