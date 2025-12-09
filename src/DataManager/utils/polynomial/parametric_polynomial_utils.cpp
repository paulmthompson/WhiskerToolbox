#include "parametric_polynomial_utils.hpp"

#include "CoreGeometry/lines.hpp" // For Line2D
#include "CoreGeometry/line_geometry.hpp"
#include "polynomial_fit.hpp"

#include "armadillo" // For arma::mat, arma::vec, arma::solve, arma::conv_to

#include <cmath>     // For std::sqrt, std::pow, std::isnan, std::isinf
#include <optional>
#include <vector>

// Helper function to compute t-values based on cumulative distance
std::vector<double> compute_t_values(Line2D const & line) {
    if (line.empty()) {
        return {};
    }
    
    std::vector<double> distances;
    distances.reserve(line.size());
    distances.push_back(0.0);  // First point has distance 0
    
    double total_distance = 0.0;
    for (size_t i = 1; i < line.size(); ++i) {
        double dx = line[i].x - line[i-1].x;
        double dy = line[i].y - line[i-1].y;
        double segment_distance = std::sqrt(dx*dx + dy*dy);
        total_distance += segment_distance;
        distances.push_back(total_distance);
    }
    
    std::vector<double> t_values;
    t_values.reserve(line.size());
    if (total_distance > 0.0) {
        for (double d : distances) {
            t_values.push_back(d / total_distance);
        }
    } else {
        for (size_t i = 0; i < line.size(); ++i) {
            t_values.push_back(static_cast<double>(i) / static_cast<double>(line.size() > 1 ? line.size() - 1 : 1));
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

    arma::mat X_vandermonde(t_values.size(), static_cast<arma::uword>(order) + 1);
    arma::vec Y_coords(const_cast<double*>(dimension_coords.data()), dimension_coords.size(), false); // Use const_cast and non-copy for efficiency

    for (size_t i = 0; i < t_values.size(); ++i) {
        for (int j = 0; j <= order; ++j) {
            X_vandermonde(i, static_cast<arma::uword>(j)) = std::pow(t_values[i], j);
        }
    }

    arma::vec coeffs_arma;
    bool success = arma::solve(coeffs_arma, X_vandermonde, Y_coords);

    if (!success) {
        return {};
    }
    return arma::conv_to<std::vector<double>>::from(coeffs_arma);
}

ParametricCoefficients fit_parametric_polynomials(Line2D const & points, int order) {
    if (points.size() <= static_cast<size_t>(order)) {
        return {{}, {}, false};
    }

    std::vector<double> t_values = compute_t_values(points);
    if (t_values.empty()) {
        return {{}, {}, false};
    }

    std::vector<double> x_coords;
    std::vector<double> y_coords;
    x_coords.reserve(points.size());
    y_coords.reserve(points.size());

    for (auto const & point: points) {
        x_coords.push_back(static_cast<double>(point.x));
        y_coords.push_back(static_cast<double>(point.y));
    }

    std::vector<double> x_coeffs_fit = fit_single_dimension_polynomial_internal(x_coords, t_values, order);
    std::vector<double> y_coeffs_fit = fit_single_dimension_polynomial_internal(y_coords, t_values, order);

    if (x_coeffs_fit.empty() || y_coeffs_fit.empty()) {
        return {{}, {}, false};
    }

    return {x_coeffs_fit, y_coeffs_fit, true};
}

Line2D generate_smoothed_line(
        Line2D const & original_points,
        std::vector<double> const & x_coeffs,
        std::vector<double> const & y_coeffs,
        int order,
        float target_spacing) {

    static_cast<void>(order);

    if (original_points.empty() || x_coeffs.empty() || y_coeffs.empty()) {
        return {};
    }

    // Estimate total arc length from original points for determining number of samples
    float total_length = 0.0f;
    if (original_points.size() > 1) {
        for (size_t i = 1; i < original_points.size(); ++i) {
            float dx = original_points[i].x - original_points[i - 1].x;
            float dy = original_points[i].y - original_points[i - 1].y;
            total_length += std::sqrt(dx * dx + dy * dy);
        }
    }

    // If total length is very small or zero, or only one point, just return the (fitted) first point
    if (total_length < 1e-6f || original_points.size() <= 1 || target_spacing <= 1e-6f) {
        if (!original_points.empty()) {
            // Evaluate polynomial at t=0 (or use the first point if no coeffs)
            if (!x_coeffs.empty() && !y_coeffs.empty()) {
                float x = static_cast<float>(evaluate_polynomial(x_coeffs, 0.0));
                float y = static_cast<float>(evaluate_polynomial(y_coeffs, 0.0));
                return Line2D({{x, y}});
            } else {
                return Line2D({original_points[0]});
            }
        } else {
            return {};
        }
    }

    // Determine number of samples based on target_spacing
    auto num_samples = static_cast<size_t>(std::max(2.0f, std::round(total_length / target_spacing)));

    std::vector<Point2D<float>> smoothed_line;
    smoothed_line.reserve(num_samples);

    for (size_t i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(num_samples - 1);// t in [0,1]
        float x = static_cast<float>(evaluate_polynomial(x_coeffs, t));
        float y = static_cast<float>(evaluate_polynomial(y_coeffs, t));
        smoothed_line.push_back({x, y});
    }
    return smoothed_line;
}

std::vector<float> calculate_fitting_errors(Line2D const & points,
                                            std::vector<double> const & x_coeffs,
                                            std::vector<double> const & y_coeffs) {
    if (points.empty()) {
        return {};
    }

    // Calculate t values using the helper function
    std::vector<double> t_values = compute_t_values(points);
    if (t_values.empty()) {
        return {};
    }

    std::vector<float> errors;
    errors.reserve(points.size());

    for (size_t i = 0; i < points.size(); ++i) {
        double fitted_x = evaluate_polynomial(x_coeffs, t_values[i]);
        double fitted_y = evaluate_polynomial(y_coeffs, t_values[i]);

        // Use squared error directly (no square root)
        float error_squared = static_cast<float>(std::pow(static_cast<double>(points[i].x) - fitted_x, 2) +
                              std::pow(static_cast<double>(points[i].y) - fitted_y, 2));
        errors.push_back(error_squared);
    }

    return errors;
}

Line2D remove_outliers_recursive(Line2D const & points,
                                 float error_threshold_squared,
                                 int polynomial_order,
                                 int max_iterations) {
    if (points.size() < static_cast<size_t>(polynomial_order + 2) || max_iterations <= 0) {
        return points;// Base case: not enough points or max iterations reached
    }

    // Calculate t values for parameterization
    std::vector<double> t_values_recursive = compute_t_values(points);
    if (t_values_recursive.empty()) {
        return points;
    }

    // Extract x and y coordinates
    std::vector<double> x_coords_recursive;
    std::vector<double> y_coords_recursive;

    x_coords_recursive.reserve(points.size());
    y_coords_recursive.reserve(points.size());

    for (auto const & point: points) {
        x_coords_recursive.push_back(static_cast<double>(point.x));
        y_coords_recursive.push_back(static_cast<double>(point.y));
    }

    // Create Armadillo matrices
    arma::mat X(t_values_recursive.size(), static_cast<arma::uword>(polynomial_order) + 1);
    arma::vec X_vec(x_coords_recursive.data(), x_coords_recursive.size());
    arma::vec Y_vec(y_coords_recursive.data(), y_coords_recursive.size());

    // Build Vandermonde matrix
    for (size_t i = 0; i < t_values_recursive.size(); ++i) {
        for (int j = 0; j <= polynomial_order; ++j) {
            X(i, static_cast<arma::uword>(j)) = std::pow(t_values_recursive[i], static_cast<double>(j));
        }
    }

    // Solve least squares problems
    arma::vec x_coeffs_recursive;
    arma::vec y_coeffs_recursive;
    bool success_x = arma::solve(x_coeffs_recursive, X, X_vec);
    bool success_y = arma::solve(y_coeffs_recursive, X, Y_vec);

    if (!success_x || !success_y) {
        return points;// Failed to fit, return original points
    }

    // Calculate errors and filter points
    std::vector<Point2D<float>> filtered_points;
    filtered_points.reserve(points.size());
    bool any_points_removed = false;

    for (size_t i = 0; i < points.size(); ++i) {
        double fitted_x = 0.0;
        double fitted_y = 0.0;

        for (int j = 0; j <= polynomial_order; ++j) {
            fitted_x += x_coeffs_recursive(static_cast<arma::uword>(j)) * std::pow(t_values_recursive[i], static_cast<double>(j));
            fitted_y += y_coeffs_recursive(static_cast<arma::uword>(j)) * std::pow(t_values_recursive[i], static_cast<double>(j));
        }

        // Use squared error directly (no square root)
        float error_squared = static_cast<float>(std::pow(static_cast<double>(points[i].x) - fitted_x, 2) +
                              std::pow(static_cast<double>(points[i].y) - fitted_y, 2));

        // Keep point if error is below threshold
        if (error_squared <= error_threshold_squared) {
            filtered_points.push_back(points[i]);
        } else {
            any_points_removed = true;
        }
    }

    // If we filtered too many points, return original set
    if (filtered_points.size() < static_cast<size_t>(polynomial_order + 2)) {
        return points;
    }

    // If we removed points, recursively call with filtered points
    if (any_points_removed) {
        return remove_outliers_recursive(filtered_points, error_threshold_squared, polynomial_order, max_iterations - 1);
    } else {
        return filtered_points;// No more points to remove
    }
}

Line2D remove_outliers(Line2D const & points,
                       float error_threshold,
                       int polynomial_order) {
    if (points.size() < static_cast<size_t>(polynomial_order + 2)) {
        return points;// Not enough points to fit and filter
    }

    // Convert threshold to squared threshold to avoid square roots in comparisons
    float error_threshold_squared = error_threshold * error_threshold;

    // Call recursive helper with a reasonable iteration limit
    return remove_outliers_recursive(points, error_threshold_squared, polynomial_order, 10);
}

std::optional<float> calculate_polynomial_curvature(
        Line2D const & line,
        float t_position,
        int polynomial_order,
        float fitting_window_percentage) {

    if (line.size() < static_cast<size_t>(polynomial_order + 1) || line.size() < 2) {
        return std::nullopt;
    }

    std::vector<double> t_values_full_line = compute_t_values(line);
    if (t_values_full_line.empty()) {
        return std::nullopt;
    }

    std::vector<double> x_coords, y_coords;
    x_coords.reserve(line.size());
    y_coords.reserve(line.size());
    for (auto const & p: line) {
        x_coords.push_back(static_cast<double>(p.x));
        y_coords.push_back(static_cast<double>(p.y));
    }

    std::vector<double> x_coeffs = fit_single_dimension_polynomial_internal(x_coords, t_values_full_line, polynomial_order);
    std::vector<double> y_coeffs = fit_single_dimension_polynomial_internal(y_coords, t_values_full_line, polynomial_order);

    if (x_coeffs.empty() || y_coeffs.empty()) {
        return std::nullopt;
    }

    // t_eval is the point at which we want to calculate curvature, relative to the [0,1] of the whole line.
    double t_eval = std::max(0.0, std::min(1.0, static_cast<double>(t_position)));

    // h is half the fitting_window_percentage, as it's the step from t_eval to t_eval+h or t_eval-h
    fitting_window_percentage = std::max(0.001f, std::min(1.0f, fitting_window_percentage));
    double h = static_cast<double>(fitting_window_percentage) / 2.0;

    if (h == 0.0) {
        return 0.0f;
    }

    // Points for central differences, clamped to [0,1] interval
    double t_minus_h = std::max(0.0, t_eval - h);
    double t_plus_h = std::min(1.0, t_eval + h);

    // Evaluate polynomial at t_eval, t_eval-h, and t_eval+h
    double x_t = evaluate_polynomial(x_coeffs, t_eval);
    double y_t = evaluate_polynomial(y_coeffs, t_eval);
    double x_t_minus_h = evaluate_polynomial(x_coeffs, t_minus_h);
    double y_t_minus_h = evaluate_polynomial(y_coeffs, t_minus_h);
    double x_t_plus_h = evaluate_polynomial(x_coeffs, t_plus_h);
    double y_t_plus_h = evaluate_polynomial(y_coeffs, t_plus_h);

    double x_prime, y_prime, x_double_prime, y_double_prime;

    // First derivatives: (f(t+h) - f(t-h)) / (2h)
    if ((t_plus_h - t_minus_h) < 1e-9) {
        x_prime = 0;
        y_prime = 0;
    } else {
        x_prime = (x_t_plus_h - x_t_minus_h) / (t_plus_h - t_minus_h);
        y_prime = (y_t_plus_h - y_t_minus_h) / (t_plus_h - t_minus_h);
    }

    // Second derivatives: (f(t+h) - 2f(t) + f(t-h)) / h^2
    double h_for_ddot_sq = h * h;
    if (h_for_ddot_sq < 1e-9) {
        x_double_prime = 0;
        y_double_prime = 0;
    } else {
        x_double_prime = (x_t_plus_h - 2.0 * x_t + x_t_minus_h) / h_for_ddot_sq;
        y_double_prime = (y_t_plus_h - 2.0 * y_t + y_t_minus_h) / h_for_ddot_sq;
    }

    double numerator = x_prime * y_double_prime - y_prime * x_double_prime;
    double denominator_sq_term = x_prime * x_prime + y_prime * y_prime;

    if (std::abs(denominator_sq_term) < 1e-9) {
        return 0.0f;
    }

    double curvature = numerator / std::pow(denominator_sq_term, 1.5);

    if (std::isnan(curvature) || std::isinf(curvature)) {
        return std::nullopt;
    }

    return static_cast<float>(curvature);
}

/**
 * @brief Extract point using parametric polynomial interpolation
 */
std::optional<Point2D<float>> extract_parametric_point(
        Line2D const & line,
        float position,
        int polynomial_order) {
    
    if (line.empty()) {
        return std::nullopt;
    }
    
    if (line.size() == 1) {
        return line[0];
    }
    
    // Ensure valid range
    position = std::max(0.0f, std::min(1.0f, position));
    
    // Need enough points for polynomial fitting
    if (line.size() < static_cast<size_t>(polynomial_order + 1)) {
        // Fall back to direct method
        return point_at_fractional_position(line, position, true);
    }
    
    // Compute t-values for the entire line
    std::vector<double> t_values = compute_t_values(line);
    if (t_values.empty()) {
        return point_at_fractional_position(line, position, true);
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
        return point_at_fractional_position(line, position, true);
    }
    
    // Evaluate polynomials at the specified position
    double t_eval = static_cast<double>(position);
    double x = evaluate_polynomial(x_coeffs, t_eval);
    double y = evaluate_polynomial(y_coeffs, t_eval);
    
    return Point2D<float>{static_cast<float>(x), static_cast<float>(y)};
}
