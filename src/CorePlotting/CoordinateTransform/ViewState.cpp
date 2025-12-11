#include "ViewState.hpp"
#include "Matrices.hpp"
#include <algorithm>
#include <cmath>

namespace CorePlotting {

BoundingBox calculateVisibleWorldBounds(ViewState const & state) {
    if (!state.data_bounds_valid || state.viewport_width <= 0 || state.viewport_height <= 0) {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }

    float data_width = state.data_bounds.width();
    float data_height = state.data_bounds.height();
    float center_x = state.data_bounds.center_x();
    float center_y = state.data_bounds.center_y();

    if (data_width <= 0 || data_height <= 0) {
        // Fallback for empty data
        return {center_x - 1.0f, center_y - 1.0f, center_x + 1.0f, center_y + 1.0f};
    }

    // Calculate Aspect Ratio
    float aspect = static_cast<float>(state.viewport_width) / static_cast<float>(std::max(1, state.viewport_height));

    float half_w, half_h;

    // Logic to maintain aspect ratio while fitting data
    // If widget is wider than tall (aspect > 1), we might need to show more width to keep height correct?
    // The original logic seems to try to fit the data bounds into the viewport.

    if (aspect > 1.0f) {
        half_w = (data_width * state.padding_factor * aspect) / (2.0f * state.zoom_level_x);
        half_h = (data_height * state.padding_factor) / (2.0f * state.zoom_level_y);
    } else {
        half_w = (data_width * state.padding_factor) / (2.0f * state.zoom_level_x);
        half_h = (data_height * state.padding_factor / aspect) / (2.0f * state.zoom_level_y);
    }

    // Apply Pan (Normalized to Data Dimensions)
    float pan_world_x = (state.pan_offset_x * data_width) / state.zoom_level_x;
    float pan_world_y = (state.pan_offset_y * data_height) / state.zoom_level_y;

    float left = center_x - half_w + pan_world_x;
    float right = center_x + half_w + pan_world_x;
    float bottom = center_y - half_h + pan_world_y;
    float top = center_y + half_h + pan_world_y;

    return {left, bottom, right, top};
}

void computeMatricesFromViewState(ViewState const & state, glm::mat4 & view_matrix, glm::mat4 & proj_matrix) {
    // 1. Calculate the visible world rectangle
    BoundingBox visible = calculateVisibleWorldBounds(state);

    // 2. View Matrix
    // In this strategy, the "Camera" is implicitly defined by the Projection window.
    // So View Matrix is Identity.
    view_matrix = glm::mat4(1.0f);

    // 3. Projection Matrix
    // Orthographic projection of the visible bounds.
    // Note: OpenGL standard is usually -1 to 1 for Z.
    proj_matrix = createOrthoProjection(visible.min_x, visible.max_x, visible.min_y, visible.max_y, -1.0f, 1.0f);
}

glm::vec2 screenToWorld(ViewState const & state, int screen_x, int screen_y) {
    if (!state.data_bounds_valid || state.viewport_width <= 0 || state.viewport_height <= 0) {
        return {0.0f, 0.0f};
    }

    // 1. Screen to NDC
    float x_ndc = static_cast<float>(2 * screen_x) / static_cast<float>(std::max(1, state.viewport_width)) - 1.0f;
    float y_ndc = 1.0f - static_cast<float>(2 * screen_y) / static_cast<float>(std::max(1, state.viewport_height));// Flip Y

    // 2. Get Visible Bounds (The "Projection Window")
    BoundingBox bounds = calculateVisibleWorldBounds(state);

    // 3. NDC to World
    // Map [-1, 1] to [min_x, max_x]
    float world_x = bounds.min_x + (x_ndc + 1.0f) * 0.5f * (bounds.width());
    float world_y = bounds.min_y + (y_ndc + 1.0f) * 0.5f * (bounds.height());

    return {world_x, world_y};
}

glm::vec2 worldToScreen(ViewState const & state, float world_x, float world_y) {
    if (!state.data_bounds_valid || state.viewport_width <= 0 || state.viewport_height <= 0) {
        return {0.0f, 0.0f};
    }

    // Get current projection bounds
    BoundingBox bounds = calculateVisibleWorldBounds(state);

    // Convert world to NDC
    float x_ndc = 2.0f * (world_x - bounds.min_x) / (bounds.width()) - 1.0f;
    float y_ndc = 2.0f * (world_y - bounds.min_y) / (bounds.height()) - 1.0f;

    // Convert NDC to screen coordinates
    // Note: y_ndc is -1 at bottom, 1 at top. Screen Y is 0 at top.
    // So screen_y = (1 - y_ndc) * 0.5 * height
    float screen_x = (x_ndc + 1.0f) * 0.5f * static_cast<float>(state.viewport_width);
    float screen_y = (1.0f - y_ndc) * 0.5f * static_cast<float>(state.viewport_height);

    return {screen_x, screen_y};
}

void applyBoxZoom(ViewState & state, BoundingBox const & bounds) {
    if (!state.data_bounds_valid) {
        return;
    }

    float data_width = state.data_bounds.width();
    float data_height = state.data_bounds.height();
    float center_x = state.data_bounds.center_x();
    float center_y = state.data_bounds.center_y();

    float target_width = std::max(1e-6f, bounds.max_x - bounds.min_x);
    float target_height = std::max(1e-6f, bounds.max_y - bounds.min_y);

    float aspect_ratio = static_cast<float>(state.viewport_width) / static_cast<float>(std::max(1, state.viewport_height));
    float padding = state.padding_factor;

    float zfx, zfy;
    if (aspect_ratio > 1.0f) {
        zfx = target_width / (aspect_ratio * data_width * padding);
        zfy = target_height / (data_height * padding);
    } else {
        zfx = target_width / (data_width * padding);
        zfy = (target_height * aspect_ratio) / (data_height * padding);
    }

    state.zoom_level_x = std::clamp(1.0f / zfx, 0.1f, 10000.0f);// Increased max zoom
    state.zoom_level_y = std::clamp(1.0f / zfy, 0.1f, 10000.0f);

    float target_cx = 0.5f * (bounds.min_x + bounds.max_x);
    float target_cy = 0.5f * (bounds.min_y + bounds.max_y);

    // Calculate pan offset required to center the target
    float pan_norm_x = (target_cx - center_x) / (data_width * (1.0f / state.zoom_level_x));
    float pan_norm_y = (target_cy - center_y) / (data_height * (1.0f / state.zoom_level_y));

    state.pan_offset_x = pan_norm_x;
    state.pan_offset_y = pan_norm_y;
}

void resetView(ViewState & state) {
    state.zoom_level_x = 1.0f;
    state.zoom_level_y = 1.0f;
    state.pan_offset_x = 0.0f;
    state.pan_offset_y = 0.0f;
}

}// namespace CorePlotting
