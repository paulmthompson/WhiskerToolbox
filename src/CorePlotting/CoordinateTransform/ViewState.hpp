#ifndef COREPLOTTING_COORDINATETRANSFORM_VIEWSTATE_HPP
#define COREPLOTTING_COORDINATETRANSFORM_VIEWSTATE_HPP

#include "CoreGeometry/boundingbox.hpp"
#include "ViewStateData.hpp"

#include <glm/glm.hpp>

namespace CorePlotting {

/**
 * @brief Full runtime 2D camera / viewport state.
 *
 * Combines the serializable ViewStateData (zoom, pan, data bounds) with
 * runtime-only information (viewport pixel dimensions, validity flag,
 * padding factor).
 *
 * This struct is used to generate View and Projection matrices for the
 * RenderableScene.  It is the primary input to calculateVisibleWorldBounds(),
 * computeMatricesFromViewState(), screenToWorld(), worldToScreen(), etc.
 *
 * Widget code should store ViewStateData for serialization and promote it
 * to ViewState at render time via toRuntimeViewState().
 *
 * @see ViewStateData              — serializable subset
 * @see toRuntimeViewState()       — ViewStateData → ViewState
 */
struct ViewState {
    // Zoom levels (per-axis)
    // 1.0 = Fit to Data Bounds (with padding)
    float zoom_level_x = 1.0f;
    float zoom_level_y = 1.0f;

    // Pan offsets (normalized to data bounds width/height)
    // 0.0 = Centered
    float pan_offset_x = 0.0f;
    float pan_offset_y = 0.0f;

    // View parameters
    float padding_factor = 1.1f;

    // Data bounds (The "World" limits)
    BoundingBox data_bounds{0.0f, 0.0f, 0.0f, 0.0f};
    bool data_bounds_valid = false;

    // Viewport dimensions (Pixels)
    int viewport_width = 1;
    int viewport_height = 1;
};

/**
 * @brief Calculates the visible world rectangle based on the ViewState.
 * 
 * This logic determines what part of the world is currently seen by the camera.
 */
BoundingBox calculateVisibleWorldBounds(ViewState const & state);

/**
 * @brief Computes the View and Projection matrices from the ViewState.
 * 
 * This implements the "World Space Strategy":
 * - View Matrix: Identity (usually, unless we want to separate Camera Pan from Projection Window)
 * - Projection Matrix: Orthographic projection of the Visible World Bounds.
 * 
 * @param state The current ViewState.
 * @param[out] view_matrix Resulting View Matrix.
 * @param[out] proj_matrix Resulting Projection Matrix.
 */
void computeMatricesFromViewState(ViewState const & state, glm::mat4 & view_matrix, glm::mat4 & proj_matrix);

/**
 * @brief Converts screen coordinates to world coordinates using ViewState logic.
 */
glm::vec2 screenToWorld(ViewState const & state, int screen_x, int screen_y);

/**
 * @brief Converts world coordinates to screen coordinates using ViewState logic.
 */
glm::vec2 worldToScreen(ViewState const & state, float world_x, float world_y);

/**
 * @brief Applies a box zoom to the ViewState.
 * 
 * Adjusts zoom levels and pan offsets so that the specified world bounds
 * fill the viewport (respecting aspect ratio).
 */
void applyBoxZoom(ViewState & state, BoundingBox const & bounds);

/**
 * @brief Resets the view to fit the data bounds.
 */
void resetView(ViewState & state);

// =============================================================================
// ViewStateData ↔ ViewState conversion
// =============================================================================

/**
 * @brief Promote a serializable ViewStateData to a full runtime ViewState.
 *
 * This is the standard way to go from the persisted camera configuration
 * to the runtime struct consumed by calculateVisibleWorldBounds(),
 * computeMatricesFromViewState(), screenToWorld(), worldToScreen(), etc.
 *
 * The conversion maps the "relative zoom/pan" semantics used by the
 * Plots/ widgets into the "normalized pan" semantics of ViewState:
 *
 * | ViewStateData  | ViewState         | Notes                              |
 * |----------------|-------------------|------------------------------------|
 * | x_zoom, y_zoom | zoom_level_x/y    | Direct pass-through                |
 * | x_pan, y_pan   | pan_offset_x/y    | Normalized: pan / (range / zoom)   |
 * | x_min … y_max  | data_bounds       | Packed into BoundingBox             |
 *
 * @param data              The serializable view state data.
 * @param viewport_width    Current viewport width in pixels.
 * @param viewport_height   Current viewport height in pixels.
 * @param padding_factor    Optional padding around data bounds (default 1.0).
 * @return The runtime ViewState ready for matrix computation.
 */
[[nodiscard]] ViewState toRuntimeViewState(
    ViewStateData const & data,
    int viewport_width,
    int viewport_height,
    float padding_factor = 1.0f);

}// namespace CorePlotting

#endif// COREPLOTTING_COORDINATETRANSFORM_VIEWSTATE_HPP
