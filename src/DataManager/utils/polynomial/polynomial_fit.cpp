
#include "polynomial_fit.hpp"

#include "CoreGeometry/angle.hpp"
#include "CoreGeometry/line_geometry.hpp"
#include "CoreGeometry/lines.hpp"
#include "utils/polynomial/parametric_polynomial_utils.hpp"

#include <armadillo>

#include <cmath>

// Helper function to fit a polynomial of the specified order to the given data
std::vector<double> fit_polynomial(std::vector<double> const & x, std::vector<double> const & y, int order) {
    if (x.size() != y.size() || x.size() <= static_cast<size_t>(order)) {
        return {};// Not enough data points or size mismatch
    }

    // Create Armadillo matrix for Vandermonde matrix
    arma::mat X(x.size(), static_cast<arma::uword>(order) + 1);
    arma::vec Y(y.data(), y.size());

    // Build Vandermonde matrix
    for (size_t i = 0; i < x.size(); ++i) {
        for (int j = 0; j <= order; ++j) {
            X(i, static_cast<arma::uword>(j)) = std::pow(x[i], j);
        }
    }

    // Solve least squares problem: X * coeffs = Y
    arma::vec coeffs;
    bool success = arma::solve(coeffs, X, Y);

    if (!success) {
        return {};// Failed to solve
    }

    // Convert Armadillo vector to std::vector
    std::vector<double> result(coeffs.begin(), coeffs.end());
    return result;
}

// Helper function to evaluate polynomial derivative at a given point
double evaluate_polynomial_derivative(std::vector<double> const & coeffs, double x) {
    double result = 0.0;
    for (size_t i = 1; i < coeffs.size(); ++i) {
        result += static_cast<double>(i) * coeffs[i] * std::pow(x, static_cast<double>(i) - 1.0);
    }
    return result;
}

// Add the evaluate_polynomial function definition
double evaluate_polynomial(std::vector<double> const & coeffs, double x) {
    double result = 0.0;
    for (size_t i = 0; i < coeffs.size(); ++i) {
        result += coeffs[i] * std::pow(x, i);
    }
    return result;
}

// Helper function to compute 2nd derivative of a polynomial
double evaluate_polynomial_second_derivative(std::vector<double> const & coeffs, double t) {
    double result = 0.0;
    if (coeffs.size() >= 3) {// Need at least a quadratic for a non-zero 2nd derivative
        for (size_t i = 2; i < coeffs.size(); ++i) {
            result += static_cast<double>(i) * static_cast<double>(i - 1) * coeffs[i] * std::pow(t, static_cast<double>(i) - 2.0);
        }
    }
    return result;
}


// Calculate angle using polynomial parameterization
float calculate_polynomial_angle(Line2D const & line,
                                 float position,
                                 int polynomial_order,
                                 float reference_x,
                                 float reference_y) {
    if (line.size() < static_cast<size_t>(polynomial_order + 1)) {
        // Fall back to direct method if not enough points
        return calculate_direct_angle(line, position, reference_x, reference_y);
    }

    // Extract x and y coordinates
    std::vector<double> x_coords;
    std::vector<double> y_coords;

    auto length = calc_length(line);

    x_coords.reserve(line.size());
    y_coords.reserve(line.size());
    auto t_values_f = calc_cumulative_length_vector(line);
    for (size_t i = 0; i < line.size(); ++i) {
        // Normalize t_values to [0, 1]
        t_values_f[i] /= length;
    }

    std::vector<double> t_values(t_values_f.begin(), t_values_f.end());

    for (size_t i = 0; i < line.size(); ++i) {
        x_coords.push_back(static_cast<double>(line[i].x));
        y_coords.push_back(static_cast<double>(line[i].y));
    }

    // Fit polynomials to x(t) and y(t)
    std::vector<double> x_coeffs = fit_polynomial(t_values, x_coords, polynomial_order);
    std::vector<double> y_coeffs = fit_polynomial(t_values, y_coords, polynomial_order);

    if (x_coeffs.empty() || y_coeffs.empty()) {
        // Fall back to direct method if fitting failed
        return calculate_direct_angle(line, position, reference_x, reference_y);
    }

    // Evaluate derivatives at the specified position
    double t = static_cast<double>(position);
    double dx_dt = evaluate_polynomial_derivative(x_coeffs, t);
    double dy_dt = evaluate_polynomial_derivative(y_coeffs, t);

    // Calculate angle using the derivatives
    double raw_angle = std::atan2(dy_dt, dx_dt);

    // Convert to degrees
    float angle_degrees = static_cast<float>(raw_angle * 180.0 / std::numbers::pi);

    // Normalize with respect to the reference vector
    return normalize_angle(angle_degrees, reference_x, reference_y);
}

std::vector<Point2D<float>> extract_parametric_subsegment(
        Line2D const & line,
        float start_pos,
        float end_pos,
        int polynomial_order,
        int output_points) {
    
    if (line.empty()) {
        return {};
    }
    
    if (line.size() == 1) {
        return {line[0]};
    }
    
    // Ensure valid range
    start_pos = std::max(0.0f, std::min(1.0f, start_pos));
    end_pos = std::max(0.0f, std::min(1.0f, end_pos));
    
    if (start_pos >= end_pos) {
        return {};
    }
    
    // Need enough points for polynomial fitting
    if (line.size() < static_cast<size_t>(polynomial_order + 1)) {
        // Fall back to direct method
        return extract_line_subsegment_by_distance(line, start_pos, end_pos, false);
    }
    
    // Calculate cumulative distances for distance-based parameterization
    std::vector<float> distances = calc_cumulative_length_vector(line);
    float total_length = distances.back();
    
    if (total_length < 1e-6f) {
        // Line has no length, return first point
        return {line[0]};
    }
    
    // Convert fractional positions to actual distances
    //float start_distance = start_pos * total_length;
    //float end_distance = end_pos * total_length;
    
    // Create normalized t-values based on cumulative distance
    std::vector<double> t_values;
    t_values.reserve(line.size());
    for (float distance : distances) {
        t_values.push_back(static_cast<double>(distance / total_length));
    }
    
    // Extract coordinates
    std::vector<double> x_coords, y_coords;
    x_coords.reserve(line.size());
    y_coords.reserve(line.size());
    
    for (auto const & point : line) {
        x_coords.push_back(static_cast<double>(point.x));
        y_coords.push_back(static_cast<double>(point.y));
    }
    
    // Fit parametric polynomials
    std::vector<double> x_coeffs = fit_single_dimension_polynomial_internal(x_coords, t_values, polynomial_order);
    std::vector<double> y_coeffs = fit_single_dimension_polynomial_internal(y_coords, t_values, polynomial_order);
    
    if (x_coeffs.empty() || y_coeffs.empty()) {
        // Fall back to direct method if fitting failed
        return extract_line_subsegment_by_distance(line, start_pos, end_pos, false);
    }
    
    // Generate subsegment points using distance-based parameterization
    std::vector<Point2D<float>> subsegment;
    subsegment.reserve(static_cast<size_t>(output_points));
    
    for (int i = 0; i < output_points; ++i) {
        // Map from output point index to t-parameter in the subsegment range
        float t_local = static_cast<float>(i) / static_cast<float>(output_points - 1);
        float t_global = start_pos + t_local * (end_pos - start_pos);
        
        // Evaluate polynomials at this t-value
        double x = evaluate_polynomial(x_coeffs, static_cast<double>(t_global));
        double y = evaluate_polynomial(y_coeffs, static_cast<double>(t_global));
        
        subsegment.push_back({static_cast<float>(x), static_cast<float>(y)});
    }
    
    return subsegment;
}
