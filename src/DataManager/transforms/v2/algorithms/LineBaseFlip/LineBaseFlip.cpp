#include "LineBaseFlip.hpp"

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/point_geometry.hpp"
#include "transforms/v2/core/ComputeContext.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace WhiskerToolbox::Transforms::V2::Examples {

// ============================================================================
// Helper Functions Implementation
// ============================================================================

bool shouldFlipLine(Line2D const & line, Point2D<float> const & reference_point) {
    if (line.empty() || line.size() < 2) {
        return false; // Can't flip a line with less than 2 points
    }

    // Get the current base (first point) and end (last point)
    Point2D<float> const current_base = line.front();
    Point2D<float> const current_end = line.back();

    // Calculate squared distances to avoid expensive sqrt operations
    float const base_distance_sq = calc_distance2(current_base, reference_point);
    float const end_distance_sq = calc_distance2(current_end, reference_point);

    // Flip if the current base is farther from reference than the end
    return base_distance_sq > end_distance_sq;
}

Line2D flipLine(Line2D const & line) {
    if (line.empty()) {
        return line;
    }

    // Create a new line with points in reverse order
    std::vector<Point2D<float>> flipped_points;
    flipped_points.reserve(line.size());

    // Add points in reverse order using index-based access
    for (size_t i = line.size(); i > 0; --i) {
        flipped_points.push_back(line[i - 1]);
    }

    return Line2D{std::move(flipped_points)};
}

// ============================================================================
// Transform Implementation
// ============================================================================

Line2D flipLineBase(
        Line2D const & line,
        LineBaseFlipParams const & params) {
    
    if (line.empty() || line.size() < 2) {
        // Return unchanged for empty or single-point lines
        return line;
    }

    Point2D<float> const reference_point = params.getReferencePoint();

    if (shouldFlipLine(line, reference_point)) {
        return flipLine(line);
    }

    return line;
}

Line2D flipLineBaseWithContext(
        Line2D const & line,
        LineBaseFlipParams const & params,
        ComputeContext const & ctx) {
    
    if (ctx.shouldCancel()) {
        return line;
    }

    auto result = flipLineBase(line, params);
    ctx.reportProgress(1.0f);

    return result;
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
