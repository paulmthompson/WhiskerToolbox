#ifndef LINE_EDIT_OPERATIONS_HPP
#define LINE_EDIT_OPERATIONS_HPP

/**
 * @file LineEditOperations.hpp
 * @brief Qt-free helpers for in-place line geometry edits in the Media Viewer
 */

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"

#include <cstddef>

/**
 * @brief Local smoothing algorithm for brush-based line edits
 */
enum class LineSmoothAlgorithm {
    MovingAverage,///< 3-point moving average on the affected vertex subrange
    PolynomialFit ///< Parametric polynomial fit and resample on the subrange
};

/**
 * @brief Parameters for local brush smoothing on a polyline
 */
struct LineSmoothBrushParams {
    Point2D<float> center;  ///< Brush center in line coordinates
    float radius_px = 10.0f;///< Brush radius in line coordinates
    LineSmoothAlgorithm algorithm = LineSmoothAlgorithm::MovingAverage;
    int polynomial_order = 3;///< Polynomial order when algorithm is PolynomialFit
    int strength = 1;        ///< Moving-average passes (1-3)
};

/**
 * @brief Apply a parametric polynomial fit to an entire line
 * @param line Line geometry to smooth in place
 * @param order Polynomial order
 * @pre order >= 0
 * @pre line.size() > order for a successful fit
 */
void applyPolynomialFitToLine(Line2D & line, int order);

/**
 * @brief Smooth vertices of a polyline within a circular brush neighborhood
 * @param line Line geometry to modify in place
 * @param params Brush center, radius, algorithm, and strength
 * @pre line must have at least 2 points for meaningful smoothing
 * @return True when any vertices were modified
 */
[[nodiscard]] bool smoothPolylineLocally(Line2D & line, LineSmoothBrushParams const & params);

#endif// LINE_EDIT_OPERATIONS_HPP
