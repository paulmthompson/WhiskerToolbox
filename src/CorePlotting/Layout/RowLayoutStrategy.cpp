#include "RowLayoutStrategy.hpp"
#include "LayoutTransform.hpp"

namespace CorePlotting {

LayoutResponse RowLayoutStrategy::compute(LayoutRequest const & request) const {
    LayoutResponse response;

    if (request.series.empty()) {
        return response;
    }

    int const total_rows = static_cast<int>(request.series.size());

    // Compute layout for each row
    for (size_t i = 0; i < request.series.size(); ++i) {
        SeriesInfo const & series_info = request.series[i];
        SeriesLayout layout = computeRowLayout(series_info, static_cast<int>(i), total_rows, request);
        response.layouts.push_back(layout);
    }

    return response;
}

SeriesLayout RowLayoutStrategy::computeRowLayout(
        SeriesInfo const & series_info,
        int row_index,
        int total_rows,
        LayoutRequest const & request) const {

    // Handle edge case: no rows
    if (total_rows <= 0) {
        float const viewport_height = request.viewport_y_max - request.viewport_y_min;
        LayoutTransform y_transform(0.0f, viewport_height);
        return SeriesLayout(series_info.id, y_transform, row_index);
    }

    // Calculate equal height allocation for each row
    float const viewport_height = request.viewport_y_max - request.viewport_y_min;
    float const row_height = viewport_height / static_cast<float>(total_rows);

    // Calculate center Y coordinate for this row
    // Rows are stacked top-to-bottom starting at viewport_y_min
    // Center is at the middle of each row band
    float const row_center =
            request.viewport_y_min + row_height * (static_cast<float>(row_index) + 0.5f);

    // Transform: y_world = y_normalized * (row_height / 2) + row_center
    float const gain = row_height * 0.5f;
    float const offset = row_center;

    LayoutTransform y_transform(offset, gain);
    return SeriesLayout(series_info.id, y_transform, row_index);
}

}// namespace CorePlotting
