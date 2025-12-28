#ifndef DATAVIEWER_COORDINATES_HPP
#define DATAVIEWER_COORDINATES_HPP

/**
 * @file DataViewerCoordinates.hpp
 * @brief Coordinate transformation utilities for the DataViewer widget
 * 
 * This class consolidates coordinate transformation logic that was previously
 * duplicated across OpenGLWidget, DataViewerInputHandler, and DataViewerInteractionManager.
 * 
 * It provides a unified interface for converting between:
 * - Canvas coordinates (pixels, origin at top-left)
 * - World coordinates (normalized space used by scene/layout)
 * - Time coordinates (data time values)
 * - Data values (analog series values via inverse transform)
 * 
 * The class is lightweight and designed to be constructed per-frame or per-event
 * with the current view state and dimensions.
 * 
 * @section Usage
 * 
 * @code
 * // Create from current view state
 * DataViewerCoordinates coords(_view_state, width(), height());
 * 
 * // Convert mouse position to time
 * float time = coords.canvasXToTime(mouse_x);
 * 
 * // Convert to world coordinates for hit testing
 * float world_x = coords.canvasXToWorldX(mouse_x);
 * float world_y = coords.canvasYToWorldY(mouse_y);
 * 
 * // Get pixel tolerance in world units
 * float tolerance = coords.pixelToleranceToWorldX(10.0f);
 * @endcode
 */

#include "CorePlotting/CoordinateTransform/TimeAxisCoordinates.hpp"
#include "CorePlotting/CoordinateTransform/TimeRange.hpp"
#include "CorePlotting/Layout/LayoutTransform.hpp"

namespace DataViewer {

/**
 * @brief Coordinate transformation utilities for the DataViewer widget
 * 
 * Consolidates all coordinate conversion logic into a single class,
 * reducing code duplication and ensuring consistent behavior across
 * input handling, hit testing, and interaction management.
 */
class DataViewerCoordinates {
public:
    /**
     * @brief Construct from view state and dimensions
     * 
     * @param view_state Current TimeSeriesViewState with time bounds and Y-axis state
     * @param width Canvas width in pixels
     * @param height Canvas height in pixels
     */
    DataViewerCoordinates(CorePlotting::TimeSeriesViewState const & view_state,
                          int width, int height);

    /**
     * @brief Default constructor (creates identity-like transforms)
     */
    DataViewerCoordinates() = default;

    // ========================================================================
    // Canvas to World/Time Conversions
    // ========================================================================

    /**
     * @brief Convert canvas X coordinate to time coordinate
     * 
     * Maps a pixel position on the canvas to the corresponding time value
     * based on the current visible time range.
     * 
     * @param canvas_x X position in pixels (0 = left edge of canvas)
     * @return Time coordinate (floating point for sub-frame precision)
     */
    [[nodiscard]] float canvasXToTime(float canvas_x) const;

    /**
     * @brief Convert canvas X coordinate to world X coordinate
     * 
     * For the DataViewer, world X is equivalent to time coordinate.
     * This method is an alias for canvasXToTime for semantic clarity.
     * 
     * @param canvas_x X position in pixels
     * @return World X coordinate (same as time)
     */
    [[nodiscard]] float canvasXToWorldX(float canvas_x) const;

    /**
     * @brief Convert canvas Y coordinate to world Y coordinate
     * 
     * Maps a pixel position to world Y coordinate, accounting for:
     * - Canvas origin at top-left with Y increasing downward
     * - World Y increasing upward
     * - Vertical pan offset
     * 
     * @param canvas_y Y position in pixels (0 = top of canvas)
     * @return World Y coordinate
     */
    [[nodiscard]] float canvasYToWorldY(float canvas_y) const;

    /**
     * @brief Convert canvas position to world coordinates
     * 
     * Convenience method that converts both X and Y in a single call.
     * 
     * @param canvas_x X position in pixels
     * @param canvas_y Y position in pixels
     * @return Pair of (world_x, world_y)
     */
    [[nodiscard]] std::pair<float, float> canvasToWorld(float canvas_x, float canvas_y) const;

    // ========================================================================
    // World/Time to Canvas Conversions
    // ========================================================================

    /**
     * @brief Convert time coordinate to canvas X pixel coordinate
     * 
     * @param time Time coordinate
     * @return Canvas X position in pixels (0 = left edge)
     * 
     * @note Times outside the visible range will return negative values or
     *       values greater than canvas width.
     */
    [[nodiscard]] float timeToCanvasX(float time) const;

    /**
     * @brief Convert world Y coordinate to canvas Y pixel coordinate
     * 
     * @param world_y World Y coordinate
     * @return Canvas Y position in pixels (0 = top)
     */
    [[nodiscard]] float worldYToCanvasY(float world_y) const;

    // ========================================================================
    // Data Value Conversions (for analog series)
    // ========================================================================

    /**
     * @brief Convert canvas Y to analog data value using a layout transform
     * 
     * Uses the inverse of the provided Y transform to convert from canvas
     * coordinates back to the original data space.
     * 
     * @param canvas_y Canvas Y position in pixels
     * @param y_transform The LayoutTransform used for rendering the series
     * @return Data value in the series' native units
     */
    [[nodiscard]] float canvasYToAnalogValue(float canvas_y,
                                             CorePlotting::LayoutTransform const & y_transform) const;

    // ========================================================================
    // Tolerance Conversions (for hit testing)
    // ========================================================================

    /**
     * @brief Convert pixel tolerance to world X (time) units
     * 
     * Useful for hit testing: "click within 10 pixels" becomes a time tolerance.
     * 
     * @param pixels Tolerance in pixels
     * @return Tolerance in world X (time) units
     */
    [[nodiscard]] float pixelToleranceToWorldX(float pixels) const;

    /**
     * @brief Convert pixel tolerance to world Y units
     * 
     * @param pixels Tolerance in pixels
     * @return Tolerance in world Y units
     */
    [[nodiscard]] float pixelToleranceToWorldY(float pixels) const;

    // ========================================================================
    // Accessors
    // ========================================================================

    /**
     * @brief Get the time axis parameters
     * @return Const reference to TimeAxisParams
     */
    [[nodiscard]] CorePlotting::TimeAxisParams const & timeAxisParams() const {
        return _time_params;
    }

    /**
     * @brief Get the Y axis parameters
     * @return Const reference to YAxisParams
     */
    [[nodiscard]] CorePlotting::YAxisParams const & yAxisParams() const {
        return _y_params;
    }

    /**
     * @brief Get the canvas width
     * @return Width in pixels
     */
    [[nodiscard]] int width() const { return _time_params.viewport_width_px; }

    /**
     * @brief Get the canvas height
     * @return Height in pixels
     */
    [[nodiscard]] int height() const { return _y_params.viewport_height_px; }

    /**
     * @brief Get time units per pixel (for tolerance calculations)
     * @return Time units per pixel
     */
    [[nodiscard]] float timeUnitsPerPixel() const;

    /**
     * @brief Get world Y units per pixel
     * @return World Y units per pixel
     */
    [[nodiscard]] float worldYUnitsPerPixel() const;

private:
    CorePlotting::TimeAxisParams _time_params;
    CorePlotting::YAxisParams _y_params;
};

}// namespace DataViewer

#endif// DATAVIEWER_COORDINATES_HPP
