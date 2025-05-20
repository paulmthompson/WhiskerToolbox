
#include "polynomial_fit.hpp"

#include <armadillo>

#include <cmath>

// Helper function to fit a polynomial of the specified order to the given data
std::vector<double> fit_polynomial(std::vector<double> const &x, std::vector<double> const &y, int order) {
    if (x.size() != y.size() || x.size() <= order) {
        return {};  // Not enough data points or size mismatch
    }

    // Create Armadillo matrix for Vandermonde matrix
    arma::mat X(x.size(), order + 1);
    arma::vec Y(y.data(), y.size());

    // Build Vandermonde matrix
    for (size_t i = 0; i < x.size(); ++i) {
        for (int j = 0; j <= order; ++j) {
            X(i, j) = std::pow(x[i], j);
        }
    }

    // Solve least squares problem: X * coeffs = Y
    arma::vec coeffs;
    bool success = arma::solve(coeffs, X, Y);

    if (!success) {
        return {}; // Failed to solve
    }

    // Convert Armadillo vector to std::vector
    std::vector<double> result(coeffs.begin(), coeffs.end());
    return result;
}

// Helper function to evaluate polynomial derivative at a given point
double evaluate_polynomial_derivative(std::vector<double> const &coeffs, double x) {
    double result = 0.0;
    for (size_t i = 1; i < coeffs.size(); ++i) {
        result += i * coeffs[i] * std::pow(x, i - 1);
    }
    return result;
}

// Add the evaluate_polynomial function definition
double evaluate_polynomial(std::vector<double> const &coeffs, double x) {
    double result = 0.0;
    for (size_t i = 0; i < coeffs.size(); ++i) {
        result += coeffs[i] * std::pow(x, i);
    }
    return result;
}
