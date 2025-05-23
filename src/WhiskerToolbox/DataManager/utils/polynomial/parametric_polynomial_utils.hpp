#ifndef PARAMETRIC_POLYNOMIAL_UTILS_HPP
#define PARAMETRIC_POLYNOMIAL_UTILS_HPP

#include "Points/points.hpp" // For Point2D

#include <vector>


// Helper function to compute t-values based on cumulative distance
std::vector<double> compute_t_values(const std::vector<Point2D<float>>& points);

// Helper function to fit a single dimension (x or y) of a parametric polynomial.
std::vector<double> fit_single_dimension_polynomial_internal(
    const std::vector<double>& dimension_coords,
    const std::vector<double>& t_values,
    int order);

#endif // PARAMETRIC_POLYNOMIAL_UTILS_HPP 
