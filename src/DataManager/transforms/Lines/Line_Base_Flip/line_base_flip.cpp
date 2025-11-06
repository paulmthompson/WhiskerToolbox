#include "line_base_flip.hpp"

#include "Lines/Line_Data.hpp"
#include "CoreGeometry/lines.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <algorithm>
#include <cmath>

bool LineBaseFlipTransform::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<LineData>(dataVariant);
}

DataTypeVariant LineBaseFlipTransform::execute(DataTypeVariant const & dataVariant,
                                              TransformParametersBase const * transformParameters) {
    // Default progress callback that does nothing
    auto defaultCallback = [](int) {};
    return execute(dataVariant, transformParameters, defaultCallback);
}

DataTypeVariant LineBaseFlipTransform::execute(DataTypeVariant const & dataVariant,
                                              TransformParametersBase const * transformParameters,
                                              ProgressCallback progressCallback) {
    if (!canApply(dataVariant)) {
        return dataVariant;
    }

    auto const & input_line_data = std::get<std::shared_ptr<LineData>>(dataVariant);
    if (!input_line_data) {
        return dataVariant;
    }

    // Cast parameters
    auto const * params = dynamic_cast<LineBaseFlipParameters const *>(transformParameters);
    if (!params) {
        return dataVariant;
    }

    // Create new LineData for output
    auto output_line_data = std::make_shared<LineData>();
    
    // Copy image size from input
    output_line_data->setImageSize(input_line_data->getImageSize());

    // Get all times with data for progress tracking
    auto times_with_data = input_line_data->getTimesWithData();
    if (times_with_data.empty()) {
        progressCallback(100);
        return output_line_data;
    }

    progressCallback(0);

    size_t processed_times = 0;
    for (auto time : times_with_data) {
        // Get lines at this time
        auto const & lines = input_line_data->getAtTime(time);

        // Process each line in the frame
        for (auto const & line : lines) {
            if (line.empty()) {
                continue;
            }
            
            Line2D processed_line;
            if (shouldFlipLine(line, params->reference_point)) {
                processed_line = flipLine(line);
            } else {
                processed_line = line;
            }

            // Add processed line to output
            if (!processed_line.empty()) {
                output_line_data->addAtTime(time, processed_line, false);
            }
        }

        // Update progress
        processed_times++;
        int progress = static_cast<int>(
            std::round(static_cast<double>(processed_times) / static_cast<double>(times_with_data.size()) * 100.0)
        );
        progressCallback(progress);
    }

    progressCallback(100);
    return output_line_data;
}

float LineBaseFlipTransform::distanceSquared(Point2D<float> const & p1, Point2D<float> const & p2) {
    float const dx = p1.x - p2.x;
    float const dy = p1.y - p2.y;
    return dx * dx + dy * dy;
}

bool LineBaseFlipTransform::shouldFlipLine(Line2D const & line, Point2D<float> const & reference_point) {
    if (line.empty() || line.size() < 2) {
        return false; // Can't flip a line with less than 2 points
    }

    // Get the current base (first point) and end (last point)
    Point2D<float> const current_base = line.front();
    Point2D<float> const current_end = line.back();

    // Calculate squared distances to avoid expensive sqrt operations
    float const base_distance_sq = distanceSquared(current_base, reference_point);
    float const end_distance_sq = distanceSquared(current_end, reference_point);

    // Flip if the current base is farther from reference than the end
    return base_distance_sq > end_distance_sq;
}

Line2D LineBaseFlipTransform::flipLine(Line2D const & line) {
    if (line.empty()) {
        return line;
    }

    // Create a new line with points in reverse order
    std::vector<Point2D<float>> flipped_points;
    flipped_points.reserve(line.size());

    // Add points in reverse order
    for (auto it = line.end() - 1; it >= line.begin(); --it) {
        flipped_points.push_back(*it);
    }

    return Line2D{std::move(flipped_points)};
}
