#ifndef COREPLOTTING_COORDINATETRANSFORM_INVERSETRANSFORM_HPP
#define COREPLOTTING_COORDINATETRANSFORM_INVERSETRANSFORM_HPP

#include "CorePlotting/Layout/LayoutTransform.hpp"

#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>

namespace CorePlotting {

/**
 * @file InverseTransform.hpp
 * @brief Utilities for converting from canvas/screen coordinates back to world/data space
 * 
 * These functions complement the forward mapping (data → canvas) with inverse
 * operations needed for user interaction (canvas → data).
 * 
 * **Coordinate Spaces**:
 * - Canvas: Pixel coordinates with origin at top-left, Y increasing downward
 * - NDC: Normalized Device Coordinates [-1, 1] × [-1, 1]
 * - World: Plotting space (X = time, Y = data value or layout position)
 * - Data: Native data space (TimeFrameIndex, raw analog values)
 * 
 * **Typical Usage**:
 * 1. User clicks at canvas (px, py)
 * 2. canvasToNDC() → (ndc_x, ndc_y)
 * 3. ndcToWorld() with inverse VP matrices → (world_x, world_y)
 * 4. worldXToTimeIndex() → TimeFrameIndex for X
 * 5. worldYToDataY() with inverse LayoutTransform → data Y value
 */

// ============================================================================
// Canvas ↔ NDC Conversions
// ============================================================================

/**
 * @brief Convert canvas pixel coordinates to Normalized Device Coordinates
 * 
 * Assumes standard OpenGL convention where NDC X is [-1, 1] left to right,
 * and NDC Y is [-1, 1] bottom to top. Canvas Y is flipped (top = 0).
 * 
 * @param canvas_x X coordinate in pixels (0 = left edge)
 * @param canvas_y Y coordinate in pixels (0 = top edge)
 * @param viewport_width Viewport width in pixels
 * @param viewport_height Viewport height in pixels
 * @return NDC coordinates in range [-1, 1]
 */
[[nodiscard]] inline glm::vec2 canvasToNDC(
        float canvas_x, float canvas_y,
        int viewport_width, int viewport_height) {
    
    // X: 0 → -1, width → +1
    float const ndc_x = (2.0f * canvas_x / static_cast<float>(viewport_width)) - 1.0f;
    
    // Y: 0 (top) → +1, height (bottom) → -1 (flip for OpenGL convention)
    float const ndc_y = 1.0f - (2.0f * canvas_y / static_cast<float>(viewport_height));
    
    return {ndc_x, ndc_y};
}

/**
 * @brief Convert NDC to canvas pixel coordinates
 * 
 * Inverse of canvasToNDC().
 * 
 * @param ndc_x NDC X coordinate [-1, 1]
 * @param ndc_y NDC Y coordinate [-1, 1]
 * @param viewport_width Viewport width in pixels
 * @param viewport_height Viewport height in pixels
 * @return Canvas coordinates in pixels
 */
[[nodiscard]] inline glm::vec2 ndcToCanvas(
        float ndc_x, float ndc_y,
        int viewport_width, int viewport_height) {
    
    float const canvas_x = (ndc_x + 1.0f) * 0.5f * static_cast<float>(viewport_width);
    float const canvas_y = (1.0f - ndc_y) * 0.5f * static_cast<float>(viewport_height);
    
    return {canvas_x, canvas_y};
}

// ============================================================================
// NDC ↔ World Conversions
// ============================================================================

/**
 * @brief Convert NDC coordinates to world space using inverse VP matrices
 * 
 * Applies the inverse of (Projection × View) to unproject from NDC to world.
 * 
 * @param ndc_pos Position in NDC [-1, 1]
 * @param inverse_vp Inverse of (projection × view) matrix
 * @return World-space coordinates
 */
[[nodiscard]] inline glm::vec2 ndcToWorld(
        glm::vec2 ndc_pos,
        glm::mat4 const& inverse_vp) {
    
    // Use z=0 for 2D plotting (we're on the near plane)
    glm::vec4 const ndc_point(ndc_pos.x, ndc_pos.y, 0.0f, 1.0f);
    glm::vec4 const world_point = inverse_vp * ndc_point;
    
    // Perspective divide (for orthographic, w should be 1.0)
    if (std::abs(world_point.w) > 1e-10f) {
        return {world_point.x / world_point.w, world_point.y / world_point.w};
    }
    return {world_point.x, world_point.y};
}

/**
 * @brief Convert world coordinates to NDC using VP matrices
 * 
 * Forward transform (for completeness).
 * 
 * @param world_pos Position in world space
 * @param vp (projection × view) matrix
 * @return NDC coordinates
 */
[[nodiscard]] inline glm::vec2 worldToNDC(
        glm::vec2 world_pos,
        glm::mat4 const& vp) {
    
    glm::vec4 const world_point(world_pos.x, world_pos.y, 0.0f, 1.0f);
    glm::vec4 const ndc_point = vp * world_point;
    
    if (std::abs(ndc_point.w) > 1e-10f) {
        return {ndc_point.x / ndc_point.w, ndc_point.y / ndc_point.w};
    }
    return {ndc_point.x, ndc_point.y};
}

// ============================================================================
// Combined Canvas ↔ World Conversions
// ============================================================================

/**
 * @brief Convert canvas pixel coordinates directly to world space
 * 
 * Convenience function combining canvasToNDC and ndcToWorld.
 * 
 * @param canvas_x X coordinate in pixels
 * @param canvas_y Y coordinate in pixels
 * @param viewport_width Viewport width in pixels
 * @param viewport_height Viewport height in pixels
 * @param view_matrix View (camera) matrix
 * @param projection_matrix Projection matrix
 * @return World-space coordinates
 */
[[nodiscard]] inline glm::vec2 canvasToWorld(
        float canvas_x, float canvas_y,
        int viewport_width, int viewport_height,
        glm::mat4 const& view_matrix,
        glm::mat4 const& projection_matrix) {
    
    // Canvas → NDC
    glm::vec2 const ndc = canvasToNDC(canvas_x, canvas_y, viewport_width, viewport_height);
    
    // NDC → World (need inverse VP)
    glm::mat4 const vp = projection_matrix * view_matrix;
    glm::mat4 const inverse_vp = glm::inverse(vp);
    
    return ndcToWorld(ndc, inverse_vp);
}

/**
 * @brief Convert world coordinates to canvas pixels
 * 
 * Inverse of canvasToWorld().
 * 
 * @param world_x World X coordinate
 * @param world_y World Y coordinate
 * @param viewport_width Viewport width in pixels
 * @param viewport_height Viewport height in pixels
 * @param view_matrix View (camera) matrix
 * @param projection_matrix Projection matrix
 * @return Canvas coordinates in pixels
 */
[[nodiscard]] inline glm::vec2 worldToCanvas(
        float world_x, float world_y,
        int viewport_width, int viewport_height,
        glm::mat4 const& view_matrix,
        glm::mat4 const& projection_matrix) {
    
    // World → NDC
    glm::mat4 const vp = projection_matrix * view_matrix;
    glm::vec2 const ndc = worldToNDC({world_x, world_y}, vp);
    
    // NDC → Canvas
    return ndcToCanvas(ndc.x, ndc.y, viewport_width, viewport_height);
}

// ============================================================================
// World ↔ Data Conversions
// ============================================================================

/**
 * @brief Convert world X coordinate to TimeFrameIndex
 * 
 * For time-series plots, world X is typically absolute time (from TimeFrame).
 * This function performs simple rounding to the nearest integer index.
 * 
 * @param world_x World X coordinate (time)
 * @return TimeFrameIndex value (integer)
 */
[[nodiscard]] inline int64_t worldXToTimeIndex(float world_x) {
    return static_cast<int64_t>(std::round(world_x));
}

/**
 * @brief Convert TimeFrameIndex to world X coordinate
 * 
 * Trivial conversion for consistency.
 * 
 * @param time_index TimeFrameIndex value
 * @return World X coordinate
 */
[[nodiscard]] inline float timeIndexToWorldX(int64_t time_index) {
    return static_cast<float>(time_index);
}

/**
 * @brief Convert world Y coordinate to data Y using inverse LayoutTransform
 * 
 * Undoes the Y-axis layout transform to recover the original data value.
 * 
 * @param world_y World Y coordinate (after layout transform)
 * @param y_transform The LayoutTransform applied to Y values for this series
 * @return Data Y value (original scale)
 */
[[nodiscard]] inline float worldYToDataY(float world_y, LayoutTransform const& y_transform) {
    return y_transform.inverse(world_y);
}

/**
 * @brief Convert data Y coordinate to world Y using LayoutTransform
 * 
 * Forward transform (for consistency).
 * 
 * @param data_y Data Y value (original scale)
 * @param y_transform The LayoutTransform for this series
 * @return World Y coordinate
 */
[[nodiscard]] inline float dataYToWorldY(float data_y, LayoutTransform const& y_transform) {
    return y_transform.apply(data_y);
}

// ============================================================================
// Combined Canvas → Data Conversion
// ============================================================================

/**
 * @brief Result of canvas to data coordinate conversion
 */
struct CanvasToDataResult {
    int64_t time_index{0};  ///< X as TimeFrameIndex
    float data_y{0.0f};     ///< Y in data space
    float world_x{0.0f};    ///< Intermediate world X (for reference)
    float world_y{0.0f};    ///< Intermediate world Y (for reference)
};

/**
 * @brief Convert canvas coordinates to data coordinates
 * 
 * Full conversion pipeline: Canvas → NDC → World → Data
 * 
 * @param canvas_x X coordinate in pixels
 * @param canvas_y Y coordinate in pixels
 * @param viewport_width Viewport width in pixels
 * @param viewport_height Viewport height in pixels
 * @param view_matrix View (camera) matrix
 * @param projection_matrix Projection matrix
 * @param y_transform LayoutTransform for Y-axis (series-specific)
 * @return Complete data coordinates with intermediate values
 */
[[nodiscard]] inline CanvasToDataResult canvasToData(
        float canvas_x, float canvas_y,
        int viewport_width, int viewport_height,
        glm::mat4 const& view_matrix,
        glm::mat4 const& projection_matrix,
        LayoutTransform const& y_transform) {
    
    CanvasToDataResult result;
    
    // Canvas → World
    glm::vec2 const world = canvasToWorld(
            canvas_x, canvas_y,
            viewport_width, viewport_height,
            view_matrix, projection_matrix);
    
    result.world_x = world.x;
    result.world_y = world.y;
    
    // World → Data
    result.time_index = worldXToTimeIndex(world.x);
    result.data_y = worldYToDataY(world.y, y_transform);
    
    return result;
}

} // namespace CorePlotting

#endif // COREPLOTTING_COORDINATETRANSFORM_INVERSETRANSFORM_HPP
