#include "ViewState.hpp"
#include <algorithm>
#include <cmath>

namespace ViewUtils {

void calculateProjectionBounds(const ViewState& state, 
                             float& left, float& right, 
                             float& bottom, float& top) {
    if (!state.data_bounds_valid || state.widget_width <= 0 || state.widget_height <= 0) {
        left = right = bottom = top = 0.0f;
        return;
    }
    
    float data_width = state.data_bounds.width();
    float data_height = state.data_bounds.height();
    float center_x = state.data_bounds.center_x();
    float center_y = state.data_bounds.center_y();
    
    if (data_width <= 0 || data_height <= 0) {
        left = right = bottom = top = 0.0f;
        return;
    }
    
    // Apply zoom and pan transformations
    float aspect = static_cast<float>(state.widget_width) / std::max(1, state.widget_height);
    float half_w, half_h;
    
    if (aspect > 1.0f) {
        half_w = (data_width * state.padding_factor * aspect) / (2.0f * state.zoom_level_x);
        half_h = (data_height * state.padding_factor) / (2.0f * state.zoom_level_y);
    } else {
        half_w = (data_width * state.padding_factor) / (2.0f * state.zoom_level_x);
        half_h = (data_height * state.padding_factor / aspect) / (2.0f * state.zoom_level_y);
    }
    
    float pan_world_x = (state.pan_offset_x * data_width) / state.zoom_level_x;
    float pan_world_y = (state.pan_offset_y * data_height) / state.zoom_level_y;
    
    left = center_x - half_w + pan_world_x;
    right = center_x + half_w + pan_world_x;
    bottom = center_y - half_h + pan_world_y;
    top = center_y + half_h + pan_world_y;
}

void applyBoxZoom(ViewState& state, 
                 float min_x, float max_x, 
                 float min_y, float max_y) {
    if (!state.data_bounds_valid) {
        return;
    }
    
    float data_width = state.data_bounds.width();
    float data_height = state.data_bounds.height();
    float center_x = state.data_bounds.center_x();
    float center_y = state.data_bounds.center_y();
    float target_width = std::max(1e-6f, max_x - min_x);
    float target_height = std::max(1e-6f, max_y - min_y);
    float aspect_ratio = static_cast<float>(state.widget_width) / std::max(1, state.widget_height);
    float padding = state.padding_factor;
    
    float zfx, zfy;
    if (aspect_ratio > 1.0f) {
        zfx = target_width / (aspect_ratio * data_width * padding);
        zfy = target_height / (data_height * padding);
    } else {
        zfx = target_width / (data_width * padding);
        zfy = (target_height * aspect_ratio) / (data_height * padding);
    }
    
    state.zoom_level_x = std::clamp(1.0f / zfx, 0.1f, 10.0f);
    state.zoom_level_y = std::clamp(1.0f / zfy, 0.1f, 10.0f);
    
    float target_cx = 0.5f * (min_x + max_x);
    float target_cy = 0.5f * (min_y + max_y);
    float pan_norm_x = (target_cx - center_x) / (data_width * (1.0f / state.zoom_level_x));
    float pan_norm_y = (target_cy - center_y) / (data_height * (1.0f / state.zoom_level_y));
    
    state.pan_offset_x = pan_norm_x;
    state.pan_offset_y = pan_norm_y;
}

void resetView(ViewState& state) {
    state.zoom_level_x = 1.0f;
    state.zoom_level_y = 1.0f;
    state.pan_offset_x = 0.0f;
    state.pan_offset_y = 0.0f;
}

Point2D<float> screenToWorld(const ViewState& state, 
                            int screen_x, int screen_y) {
    if (!state.data_bounds_valid || state.widget_width <= 0 || state.widget_height <= 0) {
        return {0.0f, 0.0f};
    }
    
    // Convert screen to NDC
    float x_ndc = (2.0f * screen_x) / std::max(1, state.widget_width) - 1.0f;
    float y_ndc = 1.0f - (2.0f * screen_y) / std::max(1, state.widget_height);
    
    // Get current projection bounds
    float left, right, bottom, top;
    calculateProjectionBounds(state, left, right, bottom, top);
    
    // Convert NDC to world coordinates
    float world_x = left + (x_ndc + 1.0f) * 0.5f * (right - left);
    float world_y = bottom + (y_ndc + 1.0f) * 0.5f * (top - bottom);
    
    return {world_x, world_y};
}

Point2D<uint32_t> worldToScreen(const ViewState& state, 
                          float world_x, float world_y) {
    if (!state.data_bounds_valid || state.widget_width <= 0 || state.widget_height <= 0) {
        return {0, 0};
    }
    
    // Get current projection bounds
    float left, right, bottom, top;
    calculateProjectionBounds(state, left, right, bottom, top);
    
    // Convert world to NDC
    float x_ndc = 2.0f * (world_x - left) / (right - left) - 1.0f;
    float y_ndc = 2.0f * (world_y - bottom) / (top - bottom) - 1.0f;
    
    // Convert NDC to screen coordinates
    uint32_t screen_x = static_cast<uint32_t>((x_ndc + 1.0f) * 0.5f * state.widget_width);
    uint32_t screen_y = static_cast<uint32_t>((1.0f - y_ndc) * 0.5f * state.widget_height);

    return {screen_x, screen_y};
}

void computeCameraWorldView(const ViewState& state,
                           float& center_x, float& center_y,
                           float& world_width, float& world_height) {
    if (!state.data_bounds_valid) {
        center_x = center_y = world_width = world_height = 0.0f;
        return;
    }
    
    float data_width = state.data_bounds.width();
    float data_height = state.data_bounds.height();
    float data_center_x = state.data_bounds.center_x();
    float data_center_y = state.data_bounds.center_y();
    
    // Calculate visible world extents based on zoom
    float aspect = static_cast<float>(state.widget_width) / std::max(1, state.widget_height);
    
    if (aspect > 1.0f) {
        world_width = (data_width * state.padding_factor * aspect) / state.zoom_level_x;
        world_height = (data_height * state.padding_factor) / state.zoom_level_y;
    } else {
        world_width = (data_width * state.padding_factor) / state.zoom_level_x;
        world_height = (data_height * state.padding_factor / aspect) / state.zoom_level_y;
    }
    
    // Calculate camera center including pan offset
    float pan_world_x = (state.pan_offset_x * data_width) / state.zoom_level_x;
    float pan_world_y = (state.pan_offset_y * data_height) / state.zoom_level_y;
    
    center_x = data_center_x + pan_world_x;
    center_y = data_center_y + pan_world_y;
}

} // namespace ViewUtils
