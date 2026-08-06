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
 * _projection_matrix = Neuralyzer::Plots::computeOrthoProjection(_cached_view_state);
 * _view_matrix = glm::mat4(1.0f);
 *
 * // In handlePanning():
 * Neuralyzer::Plots::handlePanning(*_state, _cached_view_state,
 *     delta_x, delta_y, _widget_width, _widget_height);
 *
 * // In wheelEvent():
 * float const delta = Neuralyzer::Plots::wheelDeltaToZoomSteps(
 *     event->pixelDelta().y(), event->angleDelta().y(), event->angleDelta().x());
 * Neuralyzer::Plots::handleZoom(*_state, _cached_view_state, delta, y_only, both_axes);
 *
 * // In screenToWorld():
 * QPointF world = Neuralyzer::Plots::screenToWorld(
 *     _projection_matrix, _widget_width, _widget_height, screen_pos);
 * @endcode
 *
 * For view states without bounds (zoom/pan only), use the 4-arg
 * computeOrthoProjection(view_state, x_range, x_center, y_range, y_center)
 * and the 8-arg handlePanning(..., x_range, y_range, ...).
 *
 * @see PlotAlignmentGather.hpp for trial-aligned data gathering helpers
 */

#include "CorePlotting/CoordinateTransform/AxisMapping.hpp"

#include <QPoint>
#include <QPointF>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>

namespace Neuralyzer::Plots {

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
template<typename T>
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
template<typename T>
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
template<typename T>
concept ViewStateWithBounds = ViewStateLike<T> && requires(T const & vs) {
    { vs.x_min } -> std::convertible_to<double>;
    { vs.x_max } -> std::convertible_to<double>;
    { vs.y_min } -> std::convertible_to<double>;
    { vs.y_max } -> std::convertible_to<double>;
};

// =============================================================================
// Trial Row Mapping (EventPlot / raster plots)
// =============================================================================

/**
 * @brief Result of hit-testing an event in a raster plot.
 */
struct EventPlotEventHit {
    int trial_index{-1};         ///< Display row index (0 = bottom row)
    std::string event_name;      ///< Plot event name from EventPlotState
    float relative_time_ms{0.0f};///< Event time relative to alignment (world X)
    float world_y{0.0f};         ///< Event world Y coordinate
};

/**
 * @brief Derive trial index from world-space Y coordinate.
 *
 * Matches RowLayoutStrategy row placement and trialIndexAxis labeling:
 * trial 0 at world y ≈ -1 (bottom), trial (N-1) at world y ≈ +1 (top).
 *
 * @param world_y World Y coordinate in the raster plot viewport.
 * @param num_trials Number of trials (rows).
 * @return Trial index in [0, num_trials), or -1 if num_trials == 0.
 */
[[nodiscard]] inline int trialIndexFromWorldY(float world_y, std::size_t num_trials) {
    if (num_trials == 0) {
        return -1;
    }
    auto const mapping = CorePlotting::trialIndexAxis(num_trials);
    int const trial = static_cast<int>(mapping.worldToDomain(static_cast<double>(world_y)));
    return std::clamp(trial, 0, static_cast<int>(num_trials) - 1);
}

/**
 * @brief Compute world Y center for a raster row using RowLayoutStrategy geometry.
 *
 * @param row_index Zero-based row index (0 = bottom row).
 * @param num_trials Total number of rows.
 * @param viewport_y_min Minimum viewport Y (default -1).
 * @param viewport_y_max Maximum viewport Y (default +1).
 * @return World Y coordinate at the center of the row.
 */
[[nodiscard]] inline float rowWorldYCenter(
        int row_index,
        std::size_t num_trials,
        float viewport_y_min = -1.0f,
        float viewport_y_max = 1.0f) {
    if (num_trials == 0) {
        return (viewport_y_min + viewport_y_max) * 0.5f;
    }
    float const viewport_height = viewport_y_max - viewport_y_min;
    float const row_height = viewport_height / static_cast<float>(num_trials);
    return viewport_y_min + row_height * (static_cast<float>(row_index) + 0.5f);
}

/**
 * @brief Convert a hit event's relative time and trial alignment to absolute time.
 *
 * Used by EventPlot double-click navigation: absolute = alignment + relative.
 *
 * @param alignment_time Absolute alignment time for the trial (ClockTicks value).
 * @param relative_time_ms Event time relative to alignment (world X from hit test).
 * @return Absolute time in the same units as alignment_time.
 */
[[nodiscard]] inline int64_t absoluteTimeFromHit(
        int64_t alignment_time,
        float relative_time_ms) {
    return alignment_time + static_cast<int64_t>(relative_time_ms);
}

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
        int widget_height) {
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
        QPoint const & screen_pos) {
    float const ndc_x = (2.0f * screen_pos.x() / widget_width) - 1.0f;
    float const ndc_y = 1.0f - (2.0f * screen_pos.y() / widget_height);// Flip Y

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
        float world_y) {
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
template<ViewStateLike ViewState>
[[nodiscard]] glm::mat4 computeOrthoProjection(
        ViewState const & view_state,
        float x_range,
        float x_center,
        float y_range,
        float y_center) {
    // Guard against degenerate ranges that would produce NaN/inf in the
    // projection matrix.  This can happen transiently when bounds are
    // being initialised (e.g. y_min == y_max before unit count is known).
    constexpr float MIN_RANGE = 1e-6f;
    if (x_range < MIN_RANGE) {
        x_range = MIN_RANGE;
    }
    if (y_range < MIN_RANGE) {
        y_range = MIN_RANGE;
    }

    float const x_zoom = static_cast<float>(view_state.x_zoom);
    float const y_zoom = static_cast<float>(view_state.y_zoom);

    float const safe_x_zoom = (x_zoom > 0.0f) ? x_zoom : 1.0f;
    float const safe_y_zoom = (y_zoom > 0.0f) ? y_zoom : 1.0f;

    float const zoomed_x_range = x_range / safe_x_zoom;
    float const zoomed_y_range = y_range / safe_y_zoom;

    float const pan_x = static_cast<float>(view_state.x_pan);
    float const pan_y = static_cast<float>(view_state.y_pan);

    float const left = x_center - zoomed_x_range / 2.0f + pan_x;
    float const right = x_center + zoomed_x_range / 2.0f + pan_x;
    float const bottom = y_center - zoomed_y_range / 2.0f + pan_y;
    float const top = y_center + zoomed_y_range / 2.0f + pan_y;

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
template<ViewStateWithBounds ViewState>
[[nodiscard]] glm::mat4 computeOrthoProjection(ViewState const & view_state) {
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
template<ZoomPanSettable State, ViewStateLike ViewState>
void handlePanning(
        State & state,
        ViewState const & view_state,
        int delta_x,
        int delta_y,
        float x_range,
        float y_range,
        int widget_width,
        int widget_height) {
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
template<ZoomPanSettable State, ViewStateWithBounds ViewState>
void handlePanning(
        State & state,
        ViewState const & view_state,
        int delta_x,
        int delta_y,
        int widget_width,
        int widget_height) {
    float const x_range =
            static_cast<float>(view_state.x_max - view_state.x_min);
    float const y_range =
            static_cast<float>(view_state.y_max - view_state.y_min);
    handlePanning(state, view_state, delta_x, delta_y, x_range, y_range,
                  widget_width, widget_height);
}

// =============================================================================
// Wheel input normalization
// =============================================================================

/// Pixels of vertical trackpad movement equivalent to one mouse-wheel notch.
constexpr float DEFAULT_WHEEL_PIXELS_PER_ZOOM_STEP = 20.0f;

/// Qt angleDelta units per physical wheel notch (8 degrees x 15).
constexpr float WHEEL_ANGLE_DELTA_PER_NOTCH = 120.0f;

/**
 * @brief Convert raw wheel deltas to fractional zoom steps for handleZoom().
 *
 * Prefers @p pixel_delta_y when non-zero (trackpads and smooth scrolling).
 * Otherwise uses @p angle_delta_y, falling back to @p angle_delta_x on
 * platforms (e.g. WSL/X11) that redirect vertical scroll to the horizontal
 * axis.
 *
 * @param pixel_delta_y  Vertical pixel delta from QWheelEvent::pixelDelta().
 * @param angle_delta_y  Vertical angle delta from QWheelEvent::angleDelta().
 * @param angle_delta_x  Horizontal angle delta from QWheelEvent::angleDelta().
 * @param pixels_per_zoom_step Pixels of vertical movement per one zoom step.
 * @return Fractional zoom steps; positive = zoom in.
 */
[[nodiscard]] inline float wheelDeltaToZoomSteps(
        int pixel_delta_y,
        int angle_delta_y,
        int angle_delta_x,
        float pixels_per_zoom_step = DEFAULT_WHEEL_PIXELS_PER_ZOOM_STEP) {
    if (pixel_delta_y != 0) {
        return static_cast<float>(pixel_delta_y) / pixels_per_zoom_step;
    }

    int const angle_y = (angle_delta_y != 0) ? angle_delta_y : angle_delta_x;
    return static_cast<float>(angle_y) / WHEEL_ANGLE_DELTA_PER_NOTCH;
}

// =============================================================================
// Zooming
// =============================================================================

/// Visible-range fraction removed per zoom step (matches DataViewer adaptive normal mode).
constexpr float ADAPTIVE_ZOOM_RANGE_FRACTION = 0.03f;

/// Minimum visible-range scale factor in a single wheel event (prevents runaway zoom).
constexpr float MIN_ADAPTIVE_VISIBLE_RANGE_SCALE = 0.01f;

/**
 * @brief Compute the zoom multiplier for adaptive range-proportional wheel zoom.
 *
 * Each zoom step changes the visible axis range by @ref ADAPTIVE_ZOOM_RANGE_FRACTION
 * of the current visible span (3% per step, matching DataViewer adaptive normal mode).
 * Positive @p delta zooms in; negative @p delta zooms out.
 *
 * @param delta Fractional wheel steps from wheelDeltaToZoomSteps().
 * @return Multiplier to apply to the current axis zoom value.
 */
[[nodiscard]] inline float adaptiveZoomMultiplier(float delta) {
    if (delta == 0.0f) {
        return 1.0f;
    }

    float const visible_scale = std::max(
            1.0f - ADAPTIVE_ZOOM_RANGE_FRACTION * delta,
            MIN_ADAPTIVE_VISIBLE_RANGE_SCALE);
    return 1.0f / visible_scale;
}

/**
 * @brief Apply a scroll-wheel zoom step to the state.
 *
 * Uses adaptive range-proportional scaling: each wheel step changes the visible
 * span by 3% of the current range (same feel as DataViewer adaptive mode).
 * Modifier keys select the axis:
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
template<ZoomPanSettable State, ViewStateLike ViewState>
void handleZoom(
        State & state,
        ViewState const & view_state,
        float delta,
        bool y_only,
        bool both_axes) {
    float const factor = adaptiveZoomMultiplier(delta);

    if (y_only) {
        state.setYZoom(view_state.y_zoom * factor);
    } else if (both_axes) {
        state.setXZoom(view_state.x_zoom * factor);
        state.setYZoom(view_state.y_zoom * factor);
    } else {
        state.setXZoom(view_state.x_zoom * factor);
    }
}

}// namespace Neuralyzer::Plots

#endif// PLOT_INTERACTION_HELPERS_HPP
