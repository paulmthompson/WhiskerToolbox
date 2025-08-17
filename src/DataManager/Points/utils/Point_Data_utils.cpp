#include "Point_Data_utils.hpp"

#include "Points/Point_Data.hpp"

#include <limits>

BoundingBox calculateBoundsForPointData(PointData const * point_data) {
    if (!point_data) {
        return BoundingBox(0, 0, 0, 0);
    }

    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();

    bool has_points = false;

    for (auto const & time_points_pair: point_data->GetAllPointsAsRange()) {
        for (auto const & point: time_points_pair.points) {
            min_x = std::min(min_x, point.x);
            max_x = std::max(max_x, point.x);
            min_y = std::min(min_y, point.y);
            max_y = std::max(max_y, point.y);
            has_points = true;
        }
    }

    if (!has_points) {
        return BoundingBox(0, 0, 0, 0);
    }

    // Add some padding
    float padding_x = (max_x - min_x) * 0.1f;
    float padding_y = (max_y - min_y) * 0.1f;

    return BoundingBox(min_x - padding_x, min_y - padding_y,
                       max_x + padding_x, max_y + padding_y);
}
