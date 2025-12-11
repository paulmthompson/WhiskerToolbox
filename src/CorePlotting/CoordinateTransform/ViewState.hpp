#ifndef COREPLOTTING_COORDINATETRANSFORM_VIEWSTATE_HPP
#define COREPLOTTING_COORDINATETRANSFORM_VIEWSTATE_HPP

#include "CoreGeometry/boundingbox.hpp"
#include <glm/glm.hpp>

namespace CorePlotting {

/**
 * @brief Encapsulates the state of the 2D Camera/Viewport.
 * 
 * This struct holds the logical state of the view (zoom, pan, data bounds).
 * It is used to generate the View and Projection matrices for the RenderableScene.
 * 
 * Moved from Analysis_Dashboard to CorePlotting to serve as the standard
 * camera model for all plotting widgets.
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

}// namespace CorePlotting

#endif// COREPLOTTING_COORDINATETRANSFORM_VIEWSTATE_HPP
