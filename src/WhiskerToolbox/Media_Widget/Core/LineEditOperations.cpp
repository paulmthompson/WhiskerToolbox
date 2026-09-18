/**
 * @file LineEditOperations.cpp
 * @brief Qt-free helpers for in-place line geometry edits in the Media Viewer
 */

#include "LineEditOperations.hpp"

#include "CoreGeometry/point_geometry.hpp"
#include "CoreMath/polynomial_fit.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace {

/**
 * @brief Find inclusive vertex index range touched by a circular brush
 * @param line Source polyline
 * @param center Brush center
 * @param radius_px Brush radius
 * @return Inclusive [begin, end] indices, or nullopt when no vertices are inside the brush
 */
[[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> findVertexRangeInRadius(
        Line2D const & line,
        Point2D<float> const & center,
        float radius_px) {
    if (line.empty()) {
        return std::nullopt;
    }

    std::size_t begin = line.size();
    std::size_t end = 0;
    bool found = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        if (calc_distance(center, line[i]) <= radius_px) {
            begin = std::min(begin, i);
            end = std::max(end, i);
            found = true;
        }
    }

    if (!found) {
        return std::nullopt;
    }

    return std::make_pair(begin, end);
}

/**
 * @brief Apply moving-average smoothing to a subrange copy
 * @param sub Line subrange to smooth in place
 * @param strength Number of smoothing passes (clamped to 1-3)
 */
void applyMovingAverageSmoothing(Line2D & sub, int strength) {
    int const passes = std::clamp(strength, 1, 3);
    for (int pass = 0; pass < passes; ++pass) {
        smooth_line(sub);
    }
}

/**
 * @brief Replace an inclusive vertex subrange with new geometry
 * @param line Full line to modify
 * @param begin Inclusive start index
 * @param end Inclusive end index
 * @param replacement Replacement vertices for the subrange
 */
void spliceLineSubrange(Line2D & line,
                        std::size_t begin,
                        std::size_t end,
                        Line2D const & replacement) {
    Line2D updated;

    for (std::size_t i = 0; i < begin; ++i) {
        updated.push_back(line[i]);
    }
    for (Point2D<float> const & point: replacement) {
        updated.push_back(point);
    }
    for (std::size_t i = end + 1; i < line.size(); ++i) {
        updated.push_back(line[i]);
    }

    line = std::move(updated);
}

/**
 * @brief Whether two lines differ within floating-point tolerance
 */
[[nodiscard]] bool linesDiffer(Line2D const & lhs, Line2D const & rhs) {
    if (lhs.size() != rhs.size()) {
        return true;
    }

    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (std::fabs(lhs[i].x - rhs[i].x) > 1.0e-4f || std::fabs(lhs[i].y - rhs[i].y) > 1.0e-4f) {
            return true;
        }
    }

    return false;
}

}// namespace

void applyPolynomialFitToLine(Line2D & line, int order) {
    assert(order >= 0 && "applyPolynomialFitToLine: order must be non-negative");

    if (line.size() <= static_cast<std::size_t>(order)) {
        return;
    }

    std::vector<double> t(line.size());
    std::vector<double> x_coords(line.size());
    std::vector<double> y_coords(line.size());

    for (std::size_t i = 0; i < line.size(); ++i) {
        t[i] = static_cast<double>(i) / static_cast<double>(line.size() - 1);
        x_coords[i] = line[i].x;
        y_coords[i] = line[i].y;
    }

    std::vector<double> const x_coeffs = fit_polynomial(t, x_coords, order);
    std::vector<double> const y_coeffs = fit_polynomial(t, y_coords, order);

    if (x_coeffs.empty() || y_coeffs.empty()) {
        smooth_line(line);
        return;
    }

    int const num_points = std::max(100, static_cast<int>(line.size()) * 2);
    std::vector<Point2D<float>> fitted_points;
    fitted_points.reserve(static_cast<std::size_t>(num_points));

    for (int i = 0; i < num_points; ++i) {
        double const t_param = static_cast<double>(i) / static_cast<double>(num_points - 1);
        double const x_val = evaluate_polynomial(x_coeffs, t_param);
        double const y_val = evaluate_polynomial(y_coeffs, t_param);
        fitted_points.emplace_back(static_cast<float>(x_val), static_cast<float>(y_val));
    }

    line = Line2D(std::move(fitted_points));
}

bool smoothPolylineLocally(Line2D & line, LineSmoothBrushParams const & params) {
    if (line.size() < 2) {
        return false;
    }

    auto const range = findVertexRangeInRadius(line, params.center, params.radius_px);
    if (!range.has_value()) {
        return false;
    }

    auto const [begin, end] = range.value();
    Line2D const original_line = line;

    Line2D sub;
    for (std::size_t i = begin; i <= end; ++i) {
        sub.push_back(line[i]);
    }

    switch (params.algorithm) {
        case LineSmoothAlgorithm::MovingAverage:
            applyMovingAverageSmoothing(sub, params.strength);
            break;
        case LineSmoothAlgorithm::PolynomialFit:
            applyPolynomialFitToLine(sub, params.polynomial_order);
            break;
    }

    spliceLineSubrange(line, begin, end, sub);
    return linesDiffer(line, original_line);
}
