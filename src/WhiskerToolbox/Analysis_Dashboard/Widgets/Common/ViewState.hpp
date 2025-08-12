#pragma once

#include "CoreGeometry/boundingbox.hpp"
#include "CoreGeometry/points.hpp"



/**
 * @brief Encapsulates view transformation state for plot widgets
 * 
 * This struct holds all the data needed for view transformations,
 * allowing view adapters to share common logic without duplicating code.
 */
struct ViewState {
    // Zoom levels (per-axis)
    float zoom_level_x = 1.0f;
    float zoom_level_y = 1.0f;
    
    // Pan offsets (normalized to data bounds)
    float pan_offset_x = 0.0f;
    float pan_offset_y = 0.0f;
    
    // View parameters
    float padding_factor = 1.1f;
    
    // Data bounds
    BoundingBox data_bounds{0.0f, 0.0f, 0.0f, 0.0f};
    bool data_bounds_valid = false;
    
    // Widget dimensions
    int widget_width = 1;
    int widget_height = 1;
};

/**
 * @brief Utility functions for view transformations
 * 
 * These free functions operate on ViewState and provide common
 * functionality that can be shared between different view adapters.
 */
namespace ViewUtils {
    
    /**
     * @brief Calculate orthographic projection bounds for current view
     * @param state Current view state
     * @param left Output: left bound
     * @param right Output: right bound
     * @param bottom Output: bottom bound
     * @param top Output: top bound
     */
    void calculateProjectionBounds(const ViewState& state, 
                                 float& left, float& right, 
                                 float& bottom, float& top);
    
    /**
     * @brief Apply box zoom to specified world rectangle
     * @param state View state to modify
     * @param min_x Left edge of zoom rectangle
     * @param max_x Right edge of zoom rectangle
     * @param min_y Bottom edge of zoom rectangle
     * @param max_y Top edge of zoom rectangle
     */
    void applyBoxZoom(ViewState& state, 
                     float min_x, float max_x, 
                     float min_y, float max_y);
    
    /**
     * @brief Reset view to fit all data
     * @param state View state to reset
     */
    void resetView(ViewState& state);
    
    /**
     * @brief Convert screen coordinates to world coordinates
     * @param state Current view state
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     * @return World coordinates
     */
    Point2D<float> screenToWorld(const ViewState& state, 
                                int screen_x, int screen_y);
    
    /**
     * @brief Convert world coordinates to screen coordinates
     * @param state Current view state
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     * @return Screen coordinates
     */
    Point2D<uint32_t> worldToScreen(const ViewState& state, 
                              float world_x, float world_y);
    
    /**
     * @brief Compute camera world view extents
     * @param state Current view state
     * @param center_x Output: camera center X
     * @param center_y Output: camera center Y
     * @param world_width Output: visible world width
     * @param world_height Output: visible world height
     */
    void computeCameraWorldView(const ViewState& state,
                               float& center_x, float& center_y,
                               float& world_width, float& world_height);
}
