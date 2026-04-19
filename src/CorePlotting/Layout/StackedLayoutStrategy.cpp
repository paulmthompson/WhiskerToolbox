#include "StackedLayoutStrategy.hpp"
#include "LayoutTransform.hpp"

#include <algorithm>

namespace CorePlotting {

LayoutResponse StackedLayoutStrategy::compute(LayoutRequest const & request) const {
    LayoutResponse response;

    if (request.series.empty()) {
        return response;
    }

    // Count stackable series to determine height allocation
    int const total_stackable = request.countStackableSeries();

    bool const use_fixed_height = (request.lane_sizing_policy == LaneSizingPolicy::FixedHeight);

    // Track stackable index separately from global index
    int stackable_index = 0;

    // Compute layout for each series
    for (size_t i = 0; i < request.series.size(); ++i) {
        SeriesInfo const & series_info = request.series[i];

        SeriesLayout layout;

        if (series_info.is_stackable) {
            if (use_fixed_height) {
                layout = computeFixedHeightLayout(
                        series_info, static_cast<int>(i), stackable_index, request);
            } else {
                layout = computeStackableLayout(
                        series_info, static_cast<int>(i), stackable_index, total_stackable, request);
            }
            ++stackable_index;
        } else {
            // Full-canvas series: use entire viewport
            layout = computeFullCanvasLayout(series_info, static_cast<int>(i), request);
        }

        response.layouts.push_back(layout);
    }

    return response;
}

SeriesLayout StackedLayoutStrategy::computeStackableLayout(
        SeriesInfo const & series_info,
        int series_index,
        int stackable_index,
        int total_stackable,
        LayoutRequest const & request) {

    // Handle edge case: no stackable series
    if (total_stackable <= 0) {
        float const viewport_height = request.viewport_y_max - request.viewport_y_min;
        // Identity gain, offset to center of viewport
        LayoutTransform const y_transform(0.0f, viewport_height);
        return {series_info.id, y_transform, series_index};
    }

    // Calculate equal height allocation for each stackable series
    float const viewport_height = request.viewport_y_max - request.viewport_y_min;
    float const allocated_height = viewport_height / static_cast<float>(total_stackable);

    // Calculate center Y coordinate
    // Series are stacked top-to-bottom starting at viewport_y_min
    // Center is at the middle of each allocated band
    float const allocated_center =
            request.viewport_y_min + allocated_height * (static_cast<float>(stackable_index) + 0.5f);

    // Create Y transform that:
    // - Scales data to fit allocated height (assumes normalized [-1,1] input maps to allocated_height)
    // - Translates to the allocated center position
    // Transform: y_world = y_normalized * (allocated_height / 2) + allocated_center
    float const gain = allocated_height * 0.5f;// Map [-1,1] to allocated height
    float const offset = allocated_center;

    LayoutTransform const y_transform(offset, gain);
    return {series_info.id, y_transform, series_index};
}

SeriesLayout StackedLayoutStrategy::computeFixedHeightLayout(
        SeriesInfo const & series_info,
        int series_index,
        int stackable_index,
        LayoutRequest const & request) {

    float const lane_height = request.lane_height;
    float const lane_gap = request.lane_gap;

    // Each lane occupies lane_height, with lane_gap between adjacent lanes.
    // Total stride per lane = lane_height + lane_gap
    float const stride = lane_height + lane_gap;

    // Center of lane i starting from viewport_y_min
    float const allocated_center =
            request.viewport_y_min + stride * static_cast<float>(stackable_index) + lane_height * 0.5f;

    // Gain maps normalized [-1,1] input to lane_height
    float const gain = lane_height * 0.5f;

    LayoutTransform const y_transform(allocated_center, gain);
    return {series_info.id, y_transform, series_index};
}

SeriesLayout StackedLayoutStrategy::computeFullCanvasLayout(
        SeriesInfo const & series_info,
        int series_index,
        LayoutRequest const & request) {

    // Full-canvas series span entire viewport
    float const viewport_height = request.viewport_y_max - request.viewport_y_min;
    float const allocated_center = (request.viewport_y_min + request.viewport_y_max) * 0.5f;

    // Transform: y_world = y_normalized * (viewport_height / 2) + allocated_center
    float const gain = viewport_height * 0.5f;
    float const offset = allocated_center;

    LayoutTransform const y_transform(offset, gain);
    return {series_info.id, y_transform, series_index};
}

}// namespace CorePlotting
