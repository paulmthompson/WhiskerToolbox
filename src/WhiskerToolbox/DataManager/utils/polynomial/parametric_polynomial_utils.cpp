#include "parametric_polynomial_utils.hpp"

#include "Points/points.hpp" // For Point2D

#include "armadillo" // For arma::mat, arma::vec, arma::solve, arma::conv_to

#include <vector>
#include <cmath> // For std::sqrt, std::pow

// Helper function to compute t-values based on cumulative distance
std::vector<double> compute_t_values(const std::vector<Point2D<float>>& points) {
    if (points.empty()) {
        return {};
    }
    
    std::vector<double> distances;
    distances.reserve(points.size());
    distances.push_back(0.0);  // First point has distance 0
    
    double total_distance = 0.0;
    for (size_t i = 1; i < points.size(); ++i) {
        double dx = points[i].x - points[i-1].x;
        double dy = points[i].y - points[i-1].y;
        double segment_distance = std::sqrt(dx*dx + dy*dy);
        total_distance += segment_distance;
        distances.push_back(total_distance);
    }
    
    std::vector<double> t_values;
    t_values.reserve(points.size());
    if (total_distance > 0.0) {
        for (double d : distances) {
            t_values.push_back(d / total_distance);
        }
    } else {
        for (size_t i = 0; i < points.size(); ++i) {
            t_values.push_back(static_cast<double>(i) / static_cast<double>(points.size() > 1 ? points.size() - 1 : 1));
        }
    }
    
    return t_values;
}




// Helper function to fit a single dimension (x or y) of a parametric polynomial.
std::vector<double> fit_single_dimension_polynomial_internal(
    const std::vector<double>& dimension_coords,
    const std::vector<double>& t_values,
    int order) {
    
    if (dimension_coords.size() <= static_cast<size_t>(order) || t_values.size() != dimension_coords.size()) {
        return {}; // Not enough data or mismatched sizes
    }

    arma::mat X_vandermonde(t_values.size(), order + 1);
    arma::vec Y_coords(const_cast<double*>(dimension_coords.data()), dimension_coords.size(), false); // Use const_cast and non-copy for efficiency

    for (size_t i = 0; i < t_values.size(); ++i) {
        for (int j = 0; j <= order; ++j) {
            X_vandermonde(i, j) = std::pow(t_values[i], j);
        }
    }

    arma::vec coeffs_arma;
    bool success = arma::solve(coeffs_arma, X_vandermonde, Y_coords);

    if (!success) {
        return {};
    }
    return arma::conv_to<std::vector<double>>::from(coeffs_arma);
} 
