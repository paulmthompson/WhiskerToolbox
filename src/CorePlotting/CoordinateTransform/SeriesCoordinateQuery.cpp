#include "SeriesCoordinateQuery.hpp"

namespace CorePlotting {

std::optional<SeriesQueryResult> findSeriesAtWorldY(
        float world_y,
        LayoutResponse const & layout_response,
        float tolerance /*= 0.0f*/) {
    for (auto const & series_layout: layout_response.layouts) {
        // y_transform: offset = center, gain = half_height
        float const y_center = series_layout.y_transform.offset;
        float const half_height = series_layout.y_transform.gain;

        float const y_min = y_center - half_height - tolerance;
        float const y_max = y_center + half_height + tolerance;

        if (world_y >= y_min && world_y <= y_max) {
            float const local_y = world_y - y_center;
            float const normalized = (half_height > 0.0f)
                                             ? local_y / half_height
                                             : 0.0f;

            // Check if strictly within bounds (no tolerance)
            bool const strictly_within =
                    world_y >= (y_center - half_height) &&
                    world_y <= (y_center + half_height);

            return SeriesQueryResult(
                    series_layout.series_id,
                    local_y,
                    normalized,
                    strictly_within,
                    series_layout.series_index);
        }
    }

    return std::nullopt;
}

std::optional<SeriesQueryResult> findClosestSeriesAtWorldY(
        float world_y,
        LayoutResponse const & layout_response) {
    if (layout_response.layouts.empty()) {
        return std::nullopt;
    }

    SeriesLayout const * closest = nullptr;
    float min_distance = std::numeric_limits<float>::max();

    for (auto const & series_layout: layout_response.layouts) {
        float const distance = std::abs(world_y - series_layout.y_transform.offset);
        if (distance < min_distance) {
            min_distance = distance;
            closest = &series_layout;
        }
    }

    if (!closest) {
        return std::nullopt;
    }

    // y_transform: offset = center, gain = half_height
    float const y_center = closest->y_transform.offset;
    float const half_height = closest->y_transform.gain;
    float const local_y = world_y - y_center;
    float const normalized = (half_height > 0.0f) ? local_y / half_height : 0.0f;

    bool const within_bounds =
            world_y >= (y_center - half_height) &&
            world_y <= (y_center + half_height);

    return SeriesQueryResult(
            closest->series_id,
            local_y,
            normalized,
            within_bounds,
            closest->series_index);
}


}// namespace CorePlotting