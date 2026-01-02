#ifndef COREPLOTTING_COORDINATETRANSFORM_INVERSETRANSFORM_HPP
#define COREPLOTTING_COORDINATETRANSFORM_INVERSETRANSFORM_HPP

#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>

namespace CorePlotting {
struct LayoutTransform;
}

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
[[nodiscard]] glm::vec2 canvasToNDC(
        float canvas_x, float canvas_y,
        int viewport_width, int viewport_height);

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
[[nodiscard]] glm::vec2 ndcToCanvas(
        float ndc_x, float ndc_y,
        int viewport_width, int viewport_height);

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
[[nodiscard]] glm::vec2 ndcToWorld(
        glm::vec2 ndc_pos,
        glm::mat4 const & inverse_vp);

/**
 * @brief Convert world coordinates to NDC using VP matrices
 * 
 * Forward transform (for completeness).
 * 
 * @param world_pos Position in world space
 * @param vp (projection × view) matrix
 * @return NDC coordinates
 */
[[nodiscard]] glm::vec2 worldToNDC(
        glm::vec2 world_pos,
        glm::mat4 const & vp);

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
[[nodiscard]] glm::vec2 canvasToWorld(
        float canvas_x, float canvas_y,
        int viewport_width, int viewport_height,
        glm::mat4 const & view_matrix,
        glm::mat4 const & projection_matrix);

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
[[nodiscard]] glm::vec2 worldToCanvas(
        float world_x, float world_y,
        int viewport_width, int viewport_height,
        glm::mat4 const & view_matrix,
        glm::mat4 const & projection_matrix);

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
[[nodiscard]] float worldYToDataY(float world_y, LayoutTransform const & y_transform);

/**
 * @brief Convert data Y coordinate to world Y using LayoutTransform
 * 
 * Forward transform (for consistency).
 * 
 * @param data_y Data Y value (original scale)
 * @param y_transform The LayoutTransform for this series
 * @return World Y coordinate
 */
[[nodiscard]] float dataYToWorldY(float data_y, LayoutTransform const & y_transform);

// ============================================================================
// Combined Canvas → Data Conversion
// ============================================================================

/**
 * @brief Result of canvas to data coordinate conversion
 */
struct CanvasToDataResult {
    int64_t time_index{0};///< X as TimeFrameIndex
    float data_y{0.0f};   ///< Y in data space
    float world_x{0.0f};  ///< Intermediate world X (for reference)
    float world_y{0.0f};  ///< Intermediate world Y (for reference)
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
[[nodiscard]] CanvasToDataResult canvasToData(
        float canvas_x, float canvas_y,
        int viewport_width, int viewport_height,
        glm::mat4 const & view_matrix,
        glm::mat4 const & projection_matrix,
        LayoutTransform const & y_transform);

}// namespace CorePlotting

#endif// COREPLOTTING_COORDINATETRANSFORM_INVERSETRANSFORM_HPP
