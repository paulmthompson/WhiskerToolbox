#include "StackedLayoutStrategy.hpp"

#include <algorithm>

namespace CorePlotting {

LayoutResponse StackedLayoutStrategy::compute(LayoutRequest const & request) const {
    LayoutResponse response;

    if (request.series.empty()) {
        return response;
    }

    // Count stackable series to determine height allocation
    int const total_stackable = request.countStackableSeries();

    // Track stackable index separately from global index
    int stackable_index = 0;

    // Compute layout for each series
    for (size_t i = 0; i < request.series.size(); ++i) {
        SeriesInfo const & series_info = request.series[i];

        SeriesLayout layout;

        if (series_info.is_stackable) {
            // Stackable series: divide viewport among stackable series
            layout = computeStackableLayout(
                    series_info, static_cast<int>(i), stackable_index, total_stackable, request);
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
        LayoutRequest const & request) const {

    // Handle edge case: no stackable series
    if (total_stackable <= 0) {
        SeriesLayoutResult result(0.0f, request.viewport_y_max - request.viewport_y_min);
        return SeriesLayout(result, series_info.id, series_index);
    }

    // Calculate equal height allocation for each stackable series
    float const viewport_height = request.viewport_y_max - request.viewport_y_min;
    float const allocated_height = viewport_height / static_cast<float>(total_stackable);

    // Calculate center Y coordinate
    // Series are stacked top-to-bottom starting at viewport_y_min
    // Center is at the middle of each allocated band
    float const allocated_center =
            request.viewport_y_min + allocated_height * (static_cast<float>(stackable_index) + 0.5f);

    SeriesLayoutResult result(allocated_center, allocated_height);
    return SeriesLayout(result, series_info.id, series_index);
}

SeriesLayout StackedLayoutStrategy::computeFullCanvasLayout(
        SeriesInfo const & series_info,
        int series_index,
        LayoutRequest const & request) const {

    // Full-canvas series span entire viewport
    float const viewport_height = request.viewport_y_max - request.viewport_y_min;
    float const allocated_center = (request.viewport_y_min + request.viewport_y_max) * 0.5f;
    float const allocated_height = viewport_height;

    SeriesLayoutResult result(allocated_center, allocated_height);
    return SeriesLayout(result, series_info.id, series_index);
}

}// namespace CorePlotting
