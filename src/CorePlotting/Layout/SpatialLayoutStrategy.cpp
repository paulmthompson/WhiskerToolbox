#include "SpatialLayoutStrategy.hpp"

#include "LayoutTransform.hpp"

#include <algorithm>
#include <cmath>

namespace CorePlotting {

SpatialLayoutResponse SpatialLayoutStrategy::compute(
        BoundingBox const& data_bounds,
        BoundingBox const& viewport_bounds,
        float padding) const {
    
    SpatialLayoutResponse response;
    response.viewport_bounds = viewport_bounds;
    
    // Apply padding to data bounds
    float const data_width = data_bounds.width();
    float const data_height = data_bounds.height();
    float const pad_x = data_width * padding;
    float const pad_y = data_height * padding;
    
    BoundingBox padded_bounds(
        data_bounds.min_x - pad_x,
        data_bounds.min_y - pad_y,
        data_bounds.max_x + pad_x,
        data_bounds.max_y + pad_y
    );
    response.effective_data_bounds = padded_bounds;
    
    // Compute transform based on mode
    SpatialTransform transform;
    switch (_mode) {
        case Mode::Fit:
            transform = computeFitTransform(padded_bounds, viewport_bounds);
            break;
        case Mode::Fill:
            transform = computeFillTransform(padded_bounds, viewport_bounds);
            break;
        case Mode::Identity:
            transform = SpatialTransform::identity();
            break;
    }
    
    // Create standard SeriesLayout for compatibility
    // For spatial data, we use the viewport center and full height
    float const viewport_center_y = (viewport_bounds.min_y + viewport_bounds.max_y) / 2.0f;
    float const viewport_height = viewport_bounds.height();
    
    // For spatial layout, gain maps data range to viewport height
    // offset centers the output at viewport center
    LayoutTransform const y_transform(viewport_center_y, viewport_height * 0.5f);
    SeriesLayout series_layout("spatial", y_transform, 0);
    
    response.layout = SpatialSeriesLayout(series_layout, transform);
    
    return response;
}

LayoutResponse SpatialLayoutStrategy::computeFromRequest(LayoutRequest const& request) const {
    LayoutResponse response;
    
    // For standard LayoutRequest, create simple viewport-filling layout
    // This is a fallback for when spatial bounds aren't available
    float const viewport_height = request.viewport_y_max - request.viewport_y_min;
    float const viewport_center = (request.viewport_y_min + request.viewport_y_max) / 2.0f;
    
    for (size_t i = 0; i < request.series.size(); ++i) {
        SeriesInfo const& series_info = request.series[i];
        LayoutTransform const y_transform(viewport_center, viewport_height * 0.5f);
        SeriesLayout layout(series_info.id, y_transform, static_cast<int>(i));
        response.layouts.push_back(layout);
    }
    
    return response;
}

SpatialTransform SpatialLayoutStrategy::computeFitTransform(
        BoundingBox const& data_bounds,
        BoundingBox const& viewport_bounds) const {
    
    SpatialTransform transform;
    
    float const data_width = data_bounds.width();
    float const data_height = data_bounds.height();
    float const viewport_width = viewport_bounds.width();
    float const viewport_height = viewport_bounds.height();
    
    // Handle degenerate cases
    if (data_width <= 0.0f || data_height <= 0.0f) {
        // No valid data bounds - return identity
        return SpatialTransform::identity();
    }
    
    if (viewport_width <= 0.0f || viewport_height <= 0.0f) {
        // No valid viewport - return identity
        return SpatialTransform::identity();
    }
    
    // Compute uniform scale to fit data into viewport
    float const scale_x = viewport_width / data_width;
    float const scale_y = viewport_height / data_height;
    float const uniform_scale = std::min(scale_x, scale_y);
    
    transform.x_scale = uniform_scale;
    transform.y_scale = uniform_scale;
    
    // Compute offsets to center the data in the viewport
    float const scaled_data_width = data_width * uniform_scale;
    float const scaled_data_height = data_height * uniform_scale;
    
    // Center of data after scaling (relative to data origin)
    float const data_center_x = data_bounds.min_x + data_width / 2.0f;
    float const data_center_y = data_bounds.min_y + data_height / 2.0f;
    
    // Center of viewport
    float const viewport_center_x = viewport_bounds.min_x + viewport_width / 2.0f;
    float const viewport_center_y = viewport_bounds.min_y + viewport_height / 2.0f;
    
    // Offset to map scaled data center to viewport center
    // output = input * scale + offset
    // viewport_center = data_center * scale + offset
    // offset = viewport_center - data_center * scale
    transform.x_offset = viewport_center_x - data_center_x * uniform_scale;
    transform.y_offset = viewport_center_y - data_center_y * uniform_scale;
    
    return transform;
}

SpatialTransform SpatialLayoutStrategy::computeFillTransform(
        BoundingBox const& data_bounds,
        BoundingBox const& viewport_bounds) const {
    
    SpatialTransform transform;
    
    float const data_width = data_bounds.width();
    float const data_height = data_bounds.height();
    float const viewport_width = viewport_bounds.width();
    float const viewport_height = viewport_bounds.height();
    
    // Handle degenerate cases
    if (data_width <= 0.0f || data_height <= 0.0f) {
        return SpatialTransform::identity();
    }
    
    if (viewport_width <= 0.0f || viewport_height <= 0.0f) {
        return SpatialTransform::identity();
    }
    
    // Non-uniform scale to fill viewport completely
    transform.x_scale = viewport_width / data_width;
    transform.y_scale = viewport_height / data_height;
    
    // Offset to map data min to viewport min
    // output = input * scale + offset
    // viewport_min = data_min * scale + offset
    // offset = viewport_min - data_min * scale
    transform.x_offset = viewport_bounds.min_x - data_bounds.min_x * transform.x_scale;
    transform.y_offset = viewport_bounds.min_y - data_bounds.min_y * transform.y_scale;
    
    return transform;
}

} // namespace CorePlotting
