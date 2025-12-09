#ifndef PARAMETRIC_POLYNOMIAL_UTILS_HPP
#define PARAMETRIC_POLYNOMIAL_UTILS_HPP

#include "CoreGeometry/points.hpp"// For Point2D

#include <optional>
#include <vector>

class Line2D;

// Helper function to compute t-values based on cumulative distance
std::vector<double> compute_t_values(Line2D const & line);

// Helper function to fit a single dimension (x or y) of a parametric polynomial.
std::vector<double> fit_single_dimension_polynomial_internal(
        std::vector<double> const & dimension_coords,
        std::vector<double> const & t_values,
        int order);

/**
 * @brief Calculate the curvature of a line at a specific position using polynomial fitting
 * 
 * @param line The line to calculate curvature for
 * @param t_position Fractional position along the line (0.0 to 1.0)
 * @param polynomial_order Order of polynomial to fit
 * @param fitting_window_percentage Percentage of the line to use for fitting (0.0 to 1.0)
 * @return The curvature at the specified position, or std::nullopt if calculation fails
 */
std::optional<float> calculate_polynomial_curvature(
        Line2D const & line,
        float t_position,
        int polynomial_order,
        float fitting_window_percentage);

std::optional<Point2D<float>> extract_parametric_point(Line2D const & line,
                                                       float position,
                                                       int polynomial_order);

#endif// PARAMETRIC_POLYNOMIAL_UTILS_HPP
