#include "line_min_point_dist.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>

// Calculate the minimum distance^2 from a point to any segment of a line
float point_to_line_min_distance2(Point2D<float> const & point, Line2D const & line) {
    if (line.size() < 2) {
        // If the line has fewer than 2 points, it's not a valid line
        return std::numeric_limits<float>::max();
    }

    float min_distance = std::numeric_limits<float>::max();

    // Check each segment of the line
    for (size_t i = 0; i < line.size() - 1; ++i) {
        Point2D<float> const & segment_start = line[i];
        Point2D<float> const & segment_end = line[i + 1];

        float distance = point_to_line_segment_distance2(point, segment_start, segment_end);
        min_distance = std::min(min_distance, distance);
    }

    return min_distance;
}

// Helper function to scale a point from one image size to another
Point2D<float> scale_point(Point2D<float> const & point, ImageSize const & from_size, ImageSize const & to_size) {
    // Handle invalid image sizes
    if (from_size.width <= 0 || from_size.height <= 0 ||
        to_size.width <= 0 || to_size.height <= 0) {
        return point;// Return original point if any dimension is invalid
    }

    float scale_x = static_cast<float>(to_size.width) / static_cast<float>(from_size.width);
    float scale_y = static_cast<float>(to_size.height) / static_cast<float>(from_size.height);

    return Point2D<float>{
            point.x * scale_x,
            point.y * scale_y};
}

std::string LineMinPointDistOperation::getName() const {
    return "Calculate Line to Point Distance";
}

std::type_index LineMinPointDistOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LineMinPointDistOperation::canApply(DataTypeVariant const & dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<LineData>>(dataVariant)) {
        return false;
    }

    auto const * ptr_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);

    return ptr_ptr && *ptr_ptr;
}

std::unique_ptr<TransformParametersBase> LineMinPointDistOperation::getDefaultParameters() const {
    return std::make_unique<LineMinPointDistParameters>();
}

DataTypeVariant LineMinPointDistOperation::execute(DataTypeVariant const & dataVariant,
                                                   TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, [](int) {});
}

DataTypeVariant LineMinPointDistOperation::execute(DataTypeVariant const & dataVariant,
                                                   TransformParametersBase const * transformParameters,
                                                   ProgressCallback progressCallback) {
    auto const * ptr_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);
    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "LineMinPointDistOperation::execute (with progress): Incompatible variant type or null data." << std::endl;
        if (progressCallback) progressCallback(100);
        return {};
    }

    auto line_data = *ptr_ptr;

    auto const * typed_params =
            transformParameters ? dynamic_cast<LineMinPointDistParameters const *>(transformParameters) : nullptr;

    if (!typed_params || !typed_params->point_data) {
        std::cerr << "LineMinPointDistOperation::execute (with progress): Missing point data in parameters." << std::endl;
        if (progressCallback) progressCallback(100);
        return {};
    }

    if (progressCallback) progressCallback(0);// Initial call before the main work
    std::shared_ptr<AnalogTimeSeries> result = line_min_point_dist(
            line_data.get(),
            typed_params->point_data.get(),
            progressCallback// Pass the callback
    );

    if (!result) {
        std::cerr << "LineMinPointDistOperation::execute (with progress): Calculation failed." << std::endl;
        return {};
    }

    std::cout << "LineMinPointDistOperation executed successfully (with progress)." << std::endl;
    return result;
}

std::shared_ptr<AnalogTimeSeries> line_min_point_dist(LineData const * line_data,
                                                      PointData const * point_data) {

    return line_min_point_dist(line_data, point_data, [](int) { /* Default no-op progress */ });
}

std::shared_ptr<AnalogTimeSeries> line_min_point_dist(LineData const * line_data,
                                                      PointData const * point_data,
                                                      ProgressCallback progressCallback) {

    auto analog_time_series = std::make_shared<AnalogTimeSeries>();
    std::map<int, float> distances;

    if (!line_data || !point_data) {
        std::cerr << "LineMinPointDist: Null LineData or PointData provided." << std::endl;
        if (progressCallback) progressCallback(100);// Complete progress
        return analog_time_series;
    }

    // Get the image sizes to check if scaling is needed
    ImageSize line_image_size = line_data->getImageSize();
    ImageSize point_image_size = point_data->getImageSize();

    bool need_scaling = (line_image_size.width != point_image_size.width ||
                         line_image_size.height != point_image_size.height) &&
                        (line_image_size.width > 0 && line_image_size.height > 0 &&
                         point_image_size.width > 0 && point_image_size.height > 0);

    if (need_scaling) {
        std::cout << "Scale needed: Line image size (" << line_image_size.width << "x" << line_image_size.height
                  << "), Point image size (" << point_image_size.width << "x" << point_image_size.height << ")"
                  << std::endl;
    }

    // Get all times that have line data
    auto line_times = line_data->getTimesWithData();

    if (line_times.empty()) {
        if (progressCallback) progressCallback(100);
        return analog_time_series;
    }

    size_t total_time_points = line_times.size();
    size_t processed_time_points = 0;
    if (progressCallback) progressCallback(0);// Initial progress

    // Process each time that has line data
    for (int time: line_times) {
        // Get lines at this time
        auto const & lines = line_data->getLinesAtTime(time);
        if (lines.empty()) {
            continue;// Skip if no lines at this time
        }

        // Get points at this time
        auto const & points = point_data->getPointsAtTime(time);
        if (points.empty()) {
            continue;
        }

        // Use only the first line for simplicity
        Line2D const & line = lines[0];

        // Find minimum distance from any point to the line
        float min_distance_squared = std::numeric_limits<float>::max();
        for (auto const & point: points) {
            float distance_squared;

            if (need_scaling) {
                // Scale the point to match line data's coordinate system
                Point2D<float> scaled_point = scale_point(point, point_image_size, line_image_size);
                distance_squared = point_to_line_min_distance2(scaled_point, line);
            } else {
                // Use the original point if no scaling is needed
                distance_squared = point_to_line_min_distance2(point, line);
            }

            min_distance_squared = std::min(min_distance_squared, distance_squared);
        }

        // If still max, then no points are on the line
        if (min_distance_squared == std::numeric_limits<float>::max()) {
            continue;
        }

        // Convert squared distance back to actual distance by taking square root
        float min_distance = std::sqrt(min_distance_squared);
        distances[time] = min_distance;

        processed_time_points++;
        if (progressCallback) {
            int current_progress = static_cast<int>(std::round(static_cast<double>(processed_time_points) / total_time_points * 100.0));
            progressCallback(current_progress);
        }
    }

    // Create the analog time series from the distances map
    analog_time_series->setData(distances);
    if (progressCallback) progressCallback(100);// Final progress update
    return analog_time_series;
}

float point_to_line_segment_distance2(
        Point2D<float> const & point,
        Point2D<float> const & line_start,
        Point2D<float> const & line_end) {

    // If start and end are the same point, just return distance to that point
    if (line_start.x == line_end.x && line_start.y == line_end.y) {
        float dx = point.x - line_start.x;
        float dy = point.y - line_start.y;
        return dx * dx + dy * dy;
    }

    // Calculate the squared length of the line segment
    float line_length_squared = (line_end.x - line_start.x) * (line_end.x - line_start.x) +
                                (line_end.y - line_start.y) * (line_end.y - line_start.y);

    // Calculate the projection of point onto the line segment
    float t = ((point.x - line_start.x) * (line_end.x - line_start.x) +
               (point.y - line_start.y) * (line_end.y - line_start.y)) /
              line_length_squared;

    // Clamp t to range [0, 1] to ensure we get distance to a point on the segment
    t = std::max(0.0f, std::min(1.0f, t));

    // Calculate the closest point on the segment
    float closest_x = line_start.x + t * (line_end.x - line_start.x);
    float closest_y = line_start.y + t * (line_end.y - line_start.y);

    // Calculate the distance from original point to closest point
    float dx = point.x - closest_x;
    float dy = point.y - closest_y;

    return dx * dx + dy * dy;
}
