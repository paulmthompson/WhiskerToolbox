#include "line_base_flip.hpp"

#include "Lines/Line_Data.hpp"
#include "CoreGeometry/lines.hpp"

#include <algorithm>
#include <cmath>

bool LineBaseFlipTransform::canApply(DataTypeVariant const & dataVariant) const {
    return std::holds_alternative<std::shared_ptr<LineData>>(dataVariant);
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

    // Create a copy of the input data for modification
    auto output_line_data = std::make_shared<LineData>(*input_line_data);

    // Get all times with data for progress tracking
    auto times_with_data = output_line_data->getTimesWithData();
    std::vector<TimeFrameIndex> time_vector(times_with_data.begin(), times_with_data.end());

    int total_frames = static_cast<int>(time_vector.size());
    int processed_frames = 0;

    // Process each frame
    for (auto time : time_vector) {
        // Get lines at this time
        auto const & lines = output_line_data->getAtTime(time);

        // Create a vector to hold the processed lines
        std::vector<Line2D> processed_lines;
        processed_lines.reserve(lines.size());

        // Process each line in the frame
        for (auto const & line : lines) {
            if (shouldFlipLine(line, params->reference_point)) {
                processed_lines.push_back(flipLine(line));
            } else {
                processed_lines.push_back(line);
            }
        }

        // Clear existing data at this time and add processed lines
        output_line_data->clearAtTime(time, false);
        for (auto const & processed_line : processed_lines) {
            output_line_data->addAtTime(time, processed_line, false);
        }

        // Update progress
        processed_frames++;
        int progress = (processed_frames * 100) / total_frames;
        progressCallback(progress);
    }

    progressCallback(100);
    return output_line_data;
}

float LineBaseFlipTransform::distanceSquared(Point2D<float> const & p1, Point2D<float> const & p2) {
    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;
    return dx * dx + dy * dy;
}

bool LineBaseFlipTransform::shouldFlipLine(Line2D const & line, Point2D<float> const & reference_point) {
    if (line.empty() || line.size() < 2) {
        return false; // Can't flip a line with less than 2 points
    }

    // Get the current base (first point) and end (last point)
    Point2D<float> current_base = line.front();
    Point2D<float> current_end = line.back();

    // Calculate squared distances to avoid expensive sqrt operations
    float base_distance_sq = distanceSquared(current_base, reference_point);
    float end_distance_sq = distanceSquared(current_end, reference_point);

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

    return Line2D(std::move(flipped_points));
}
