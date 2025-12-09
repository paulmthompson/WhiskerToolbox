#include "line_min_point_dist.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "CoreGeometry/line_geometry.hpp"
#include "CoreGeometry/point_geometry.hpp"
#include "transforms/utils/variant_type_check.hpp"

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

///////////////////////////////////////////////////////////////////////////////

std::string LineMinPointDistOperation::getName() const {
    return "Calculate Line to Point Distance";
}

std::type_index LineMinPointDistOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LineMinPointDistOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<LineData>(dataVariant);
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

    std::map<int, float> distances;

    if (!line_data || !point_data) {
        std::cerr << "LineMinPointDist: Null LineData or PointData provided." << std::endl;
        if (progressCallback) progressCallback(100);// Complete progress
        return std::make_shared<AnalogTimeSeries>();
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
        return std::make_shared<AnalogTimeSeries>();
    }

    size_t total_time_points = line_times.size();
    size_t processed_time_points = 0;
    if (progressCallback) progressCallback(0);// Initial progress

    // Process each time that has line data
    for (auto time: line_times) {
        // Get lines at this time
        auto const & lines = line_data->getAtTime(time);
        if (lines.empty()) {
            continue;// Skip if no lines at this time
        }

        // Get points at this time
        auto const & points = point_data->getAtTime(time);
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
        distances[static_cast<int>(time.getValue())] = min_distance;

        processed_time_points++;
        if (progressCallback) {
            int current_progress = static_cast<int>(std::round(static_cast<double>(processed_time_points) / static_cast<double>(total_time_points) * 100.0));
            progressCallback(current_progress);
        }
    }

    if (progressCallback) progressCallback(100);// Final progress update
    return std::make_shared<AnalogTimeSeries>(distances);
}
