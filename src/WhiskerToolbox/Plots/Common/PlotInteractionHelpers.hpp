#ifndef PLOT_INTERACTION_HELPERS_HPP
#define PLOT_INTERACTION_HELPERS_HPP

/**
 * @file PlotInteractionHelpers.hpp
 * @brief Shared free functions for common plot interaction math
 *
 * These template helpers consolidate the duplicated panning, zooming,
 * coordinate-transform, and projection-matrix logic that was copy-pasted
 * across every OpenGL plot widget (EventPlot, LinePlot, Heatmap, PSTH,
 * ScatterPlot, ACF, TemporalProjection).
 *
 * The functions are parameterized on the data ranges so that each widget
 * can resolve its own x/y ranges (from view state, axis states, or fixed
 * values) and then delegate to the common math.
 *
 * ## Usage (view state with bounds: x_min, x_max, y_min, y_max)
 *
 * @code
 * // In updateMatrices():
 * _projection_matrix = WhiskerToolbox::Plots::computeOrthoProjection(_cached_view_state);
 * _view_matrix = glm::mat4(1.0f);
 *
 * // In handlePanning():
 * WhiskerToolbox::Plots::handlePanning(*_state, _cached_view_state,
 *     delta_x, delta_y, _widget_width, _widget_height);
 *
 * // In handleZoom():
 * WhiskerToolbox::Plots::handleZoom(*_state, _cached_view_state, delta, y_only, both_axes);
 *
 * // In screenToWorld():
 * QPointF world = WhiskerToolbox::Plots::screenToWorld(
 *     _projection_matrix, _widget_width, _widget_height, screen_pos);
 * @endcode
 *
 * For view states without bounds (zoom/pan only), use the 4-arg
 * computeOrthoProjection(view_state, x_range, x_center, y_range, y_center)
 * and the 8-arg handlePanning(..., x_range, y_range, ...).
 *
 * @see PlotAlignmentGather.hpp for trial-aligned data gathering helpers
 */

#include <QPoint>
#include <QPointF>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <concepts>

namespace WhiskerToolbox::Plots {

// =============================================================================
// Concepts
// =============================================================================

/**
 * @brief A view state struct that exposes zoom and pan as readable fields.
 *
 * Satisfied by EventPlotViewState, LinePlotViewState, HeatmapViewState,
 * PSTHViewState, ScatterPlotViewState, ACFViewState,
 * TemporalProjectionViewViewState, etc.
 */
template <typename T>
concept ViewStateLike = requires(T const & vs) {
    { vs.x_zoom } -> std::convertible_to<double>;
    { vs.y_zoom } -> std::convertible_to<double>;
    { vs.x_pan } -> std::convertible_to<double>;
    { vs.y_pan } -> std::convertible_to<double>;
};

/**
 * @brief A state object that supports setting zoom and pan.
 *
 * Satisfied by EventPlotState, LinePlotState, HeatmapState, PSTHState,
 * ScatterPlotState, ACFState, TemporalProjectionViewState, etc.
 */
template <typename T>
concept ZoomPanSettable = requires(T & state, float f) {
    state.setPan(f, f);
    state.setXZoom(f);
    state.setYZoom(f);
};

/**
 * @brief View state with explicit axis bounds (x_min, x_max, y_min, y_max).
 *
 * Satisfied by EventPlotViewState, LinePlotViewState, HeatmapViewState,
 * PSTHViewState, ScatterPlotViewState, ACFViewState,
 * TemporalProjectionViewViewState, OnionSkinViewViewState, etc.
 * Used by the one-arg computeOrthoProjection overload to derive ranges.
 */
template <typename T>
concept ViewStateWithBounds = ViewStateLike<T> && requires(T const & vs) {
    { vs.x_min } -> std::convertible_to<double>;
    { vs.x_max } -> std::convertible_to<double>;
    { vs.y_min } -> std::convertible_to<double>;
    { vs.y_max } -> std::convertible_to<double>;
};

// =============================================================================
// Coordinate Transforms
// =============================================================================

/**
 * @brief Convert screen pixel coordinates to normalized device coordinates (NDC).
 *
 * NDC X is in [-1, 1] (left to right), NDC Y is in [-1, 1] (bottom to top).
 * Same conversion used as the first step of screenToWorld; useful for
 * selection rectangles and hit-testing in NDC space.
 *
 * @param screen_pos    Mouse position in widget-local pixels.
 * @param widget_width  Widget width in pixels.
 * @param widget_height Widget height in pixels.
 * @return NDC (x, y) as glm::vec2.
 */
[[nodiscard]] inline glm::vec2 screenToNDC(
    QPoint const & screen_pos,
    int widget_width,
    int widget_height)
{
    float const ndc_x = (2.0f * screen_pos.x() / widget_width) - 1.0f;
    float const ndc_y = 1.0f - (2.0f * screen_pos.y() / widget_height);
    return glm::vec2(ndc_x, ndc_y);
}

/**
 * @brief Convert screen pixel coordinates to world coordinates.
 *
 * Uses the inverse of the projection matrix. Identical across all plot widgets.
 *
 * @param projection_matrix  The current orthographic projection.
 * @param widget_width       Widget width in pixels.
 * @param widget_height      Widget height in pixels.
 * @param screen_pos         Mouse position in widget-local pixels.
 * @return World (x, y) as a QPointF.
 */
[[nodiscard]] inline QPointF screenToWorld(
    glm::mat4 const & projection_matrix,
    int widget_width,
    int widget_height,
    QPoint const & screen_pos)
{
    float const ndc_x = (2.0f * screen_pos.x() / widget_width) - 1.0f;
    float const ndc_y = 1.0f - (2.0f * screen_pos.y() / widget_height); // Flip Y

    glm::mat4 const inv_proj = glm::inverse(projection_matrix);
    glm::vec4 const ndc(ndc_x, ndc_y, 0.0f, 1.0f);
    glm::vec4 const world = inv_proj * ndc;

    return QPointF(world.x, world.y);
}

/**
 * @brief Convert world coordinates to screen pixel coordinates.
 *
 * @param projection_matrix  The current orthographic projection.
 * @param widget_width       Widget width in pixels.
 * @param widget_height      Widget height in pixels.
 * @param world_x            World X coordinate.
 * @param world_y            World Y coordinate.
 * @return Screen (x, y) as a QPoint.
 */
[[nodiscard]] inline QPoint worldToScreen(
    glm::mat4 const & projection_matrix,
    int widget_width,
    int widget_height,
    float world_x,
    float world_y)
{
    glm::vec4 const world(world_x, world_y, 0.0f, 1.0f);
    glm::vec4 const ndc = projection_matrix * world;

    int const screen_x = static_cast<int>((ndc.x + 1.0f) * 0.5f * widget_width);
    int const screen_y = static_cast<int>((1.0f - ndc.y) * 0.5f * widget_height);

    return QPoint(screen_x, screen_y);
}

// =============================================================================
// Projection
// =============================================================================

/**
 * @brief Compute an orthographic projection matrix from zoom/pan view state.
 *
 * This consolidates the updateMatrices() logic from all plot widgets.
 * The caller provides the data ranges and centers (which vary per widget),
 * and this function applies zoom and pan to produce the final projection.
 *
 * @tparam ViewState  A type satisfying ViewStateLike.
 * @param view_state  The cached view state (zoom + pan fields).
 * @param x_range     Total data range on the X axis (e.g. x_max - x_min).
 * @param x_center    Center of the data range on X (e.g. (x_min + x_max) / 2).
 * @param y_range     Total data range on the Y axis.
 * @param y_center    Center of the data range on Y.
 * @return The orthographic projection matrix.
 */
template <ViewStateLike ViewState>
[[nodiscard]] glm::mat4 computeOrthoProjection(
    ViewState const & view_state,
    float x_range,
    float x_center,
    float y_range,
    float y_center)
{
    float const zoomed_x_range = x_range / static_cast<float>(view_state.x_zoom);
    float const zoomed_y_range = y_range / static_cast<float>(view_state.y_zoom);

    float const pan_x = static_cast<float>(view_state.x_pan);
    float const pan_y = static_cast<float>(view_state.y_pan);

    float const left   = x_center - zoomed_x_range / 2.0f + pan_x;
    float const right  = x_center + zoomed_x_range / 2.0f + pan_x;
    float const bottom = y_center - zoomed_y_range / 2.0f + pan_y;
    float const top    = y_center + zoomed_y_range / 2.0f + pan_y;

    return glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
}

/**
 * @brief Compute orthographic projection from view state with bounds.
 *
 * Convenience overload for view states that have x_min, x_max, y_min, y_max.
 * Derives x_range, x_center, y_range, y_center and calls the 4-arg
 * computeOrthoProjection.
 *
 * @tparam ViewState  A type satisfying ViewStateWithBounds.
 * @param view_state  The cached view state (zoom, pan, and axis bounds).
 * @return The orthographic projection matrix.
 */
template <ViewStateWithBounds ViewState>
[[nodiscard]] glm::mat4 computeOrthoProjection(ViewState const & view_state)
{
    float const x_range =
        static_cast<float>(view_state.x_max - view_state.x_min);
    float const x_center =
        static_cast<float>(view_state.x_min + view_state.x_max) / 2.0f;
    float const y_range =
        static_cast<float>(view_state.y_max - view_state.y_min);
    float const y_center =
        static_cast<float>(view_state.y_min + view_state.y_max) / 2.0f;
    return computeOrthoProjection(view_state, x_range, x_center, y_range,
                                 y_center);
}

// =============================================================================
// Panning
// =============================================================================

/**
 * @brief Apply a pixel-space drag delta as a pan update to the state.
 *
 * Converts the pixel delta to world-space using the provided data ranges,
 * widget dimensions, and current zoom level, then calls state.setPan().
 *
 * @tparam State      A type satisfying ZoomPanSettable (has setPan()).
 * @tparam ViewState  A type satisfying ViewStateLike.
 * @param state       The mutable state object (setPan will be called).
 * @param view_state  The cached view state snapshot.
 * @param delta_x     Horizontal mouse drag in pixels.
 * @param delta_y     Vertical mouse drag in pixels.
 * @param x_range     Total data range on the X axis.
 * @param y_range     Total data range on the Y axis.
 * @param widget_width  Widget width in pixels.
 * @param widget_height Widget height in pixels.
 */
template <ZoomPanSettable State, ViewStateLike ViewState>
void handlePanning(
    State & state,
    ViewState const & view_state,
    int delta_x,
    int delta_y,
    float x_range,
    float y_range,
    int widget_width,
    int widget_height)
{
    float const world_per_pixel_x =
        x_range / (widget_width * static_cast<float>(view_state.x_zoom));
    float const world_per_pixel_y =
        y_range / (widget_height * static_cast<float>(view_state.y_zoom));

    float const new_pan_x = static_cast<float>(view_state.x_pan) - delta_x * world_per_pixel_x;
    float const new_pan_y = static_cast<float>(view_state.y_pan) + delta_y * world_per_pixel_y;

    state.setPan(new_pan_x, new_pan_y);
}

/**
 * @brief Apply pan from view state with bounds (derives x_range/y_range).
 *
 * Convenience overload for view states that have x_min, x_max, y_min, y_max.
 * Derives x_range and y_range and calls the 8-arg handlePanning.
 *
 * @tparam State      A type satisfying ZoomPanSettable.
 * @tparam ViewState  A type satisfying ViewStateWithBounds.
 * @param state       The mutable state object (setPan will be called).
 * @param view_state  The cached view state snapshot (must have bounds).
 * @param delta_x     Horizontal mouse drag in pixels.
 * @param delta_y     Vertical mouse drag in pixels.
 * @param widget_width  Widget width in pixels.
 * @param widget_height Widget height in pixels.
 */
template <ZoomPanSettable State, ViewStateWithBounds ViewState>
void handlePanning(
    State & state,
    ViewState const & view_state,
    int delta_x,
    int delta_y,
    int widget_width,
    int widget_height)
{
    float const x_range =
        static_cast<float>(view_state.x_max - view_state.x_min);
    float const y_range =
        static_cast<float>(view_state.y_max - view_state.y_min);
    handlePanning(state, view_state, delta_x, delta_y, x_range, y_range,
                  widget_width, widget_height);
}

// =============================================================================
// Zooming
// =============================================================================

/**
 * @brief Apply a scroll-wheel zoom step to the state.
 *
 * The zoom factor is `1.1^delta`. Modifier keys select the axis:
 * - Default (no modifier): X-axis only
 * - @p y_only (Shift): Y-axis only
 * - @p both_axes (Ctrl): Both axes simultaneously
 *
 * @tparam State      A type satisfying ZoomPanSettable.
 * @tparam ViewState  A type satisfying ViewStateLike.
 * @param state       The mutable state object (setXZoom/setYZoom called).
 * @param view_state  The cached view state snapshot.
 * @param delta       Scroll wheel delta (positive = zoom in).
 * @param y_only      If true, zoom only the Y axis.
 * @param both_axes   If true, zoom both axes simultaneously.
 */
template <ZoomPanSettable State, ViewStateLike ViewState>
void handleZoom(
    State & state,
    ViewState const & view_state,
    float delta,
    bool y_only,
    bool both_axes)
{
    float const factor = std::pow(1.1f, delta);

    if (y_only) {
        state.setYZoom(view_state.y_zoom * factor);
    } else if (both_axes) {
        state.setXZoom(view_state.x_zoom * factor);
        state.setYZoom(view_state.y_zoom * factor);
    } else {
        state.setXZoom(view_state.x_zoom * factor);
    }
}

} // namespace WhiskerToolbox::Plots

#endif // PLOT_INTERACTION_HELPERS_HPP
