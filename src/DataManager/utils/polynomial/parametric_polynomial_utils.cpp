#include "parametric_polynomial_utils.hpp"
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
