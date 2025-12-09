#ifndef PARAMETRIC_POLYNOMIAL_UTILS_HPP
#define PARAMETRIC_POLYNOMIAL_UTILS_HPP

#include "CoreGeometry/points.hpp"// For Point2D

#include <optional>
#include <vector>

class Line2D;

/**
 * @brief Result structure for parametric polynomial fitting
 * 
 * Holds the polynomial coefficients for both x(t) and y(t) parametric curves,
 * where t is the normalized arc-length parameter in [0, 1].
 */
struct ParametricCoefficients {
    std::vector<double> x_coeffs;  ///< Polynomial coefficients for x(t)
    std::vector<double> y_coeffs;  ///< Polynomial coefficients for y(t)
    bool success = false;          ///< Whether the fitting was successful
};

/**
 * @brief Compute normalized arc-length parameter values for points in a line
 * 
 * Calculates t-values based on cumulative distance along the line, normalized
 * to the range [0, 1]. The first point has t=0, the last point has t=1.
 * 
 * @param line The line to compute t-values for
 * @return Vector of t-values, one per point, or empty if line is empty
 */
std::vector<double> compute_t_values(Line2D const & line);

/**
 * @brief Fit a single dimension polynomial using arc-length parameterization
 * 
 * Fits a polynomial of specified order to one dimension (x or y) of coordinate data
 * using least-squares fitting with a Vandermonde matrix.
 * 
 * @param dimension_coords The coordinate values to fit (either x or y values)
 * @param t_values The parameter values corresponding to each coordinate (typically from compute_t_values)
 * @param order The polynomial order to fit
 * @return Vector of polynomial coefficients [c0, c1, ..., cn] where f(t) = c0 + c1*t + c2*t^2 + ...,
 *         or empty vector if fitting fails
 */
std::vector<double> fit_single_dimension_polynomial_internal(
        std::vector<double> const & dimension_coords,
        std::vector<double> const & t_values,
        int order);

/**
 * @brief Fit parametric polynomials to a 2D line
 * 
 * Fits separate polynomials for x(t) and y(t) using arc-length parameterization.
 * This allows representing curves that may be multi-valued in either x or y.
 * 
 * @param points The line points to fit
 * @param order The polynomial order to fit
 * @return ParametricCoefficients containing x and y polynomial coefficients,
 *         with success=false if fitting fails
 */
ParametricCoefficients fit_parametric_polynomials(Line2D const & points, int order);

/**
 * @brief Generate a smoothed line from parametric polynomial coefficients
 * 
 * Creates a new line by evaluating the parametric polynomials at evenly-spaced
 * parameter values. The number of output points is determined by the target spacing
 * relative to the estimated arc length of the original line.
 * 
 * @param original_points The original line (used to estimate total arc length)
 * @param x_coeffs Polynomial coefficients for x(t)
 * @param y_coeffs Polynomial coefficients for y(t)
 * @param order The polynomial order (unused, kept for API compatibility)
 * @param target_spacing Approximate spacing in pixels between output points
 * @return A new line with smoothed, evenly-spaced points
 */
Line2D generate_smoothed_line(
        Line2D const & original_points,
        std::vector<double> const & x_coeffs,
        std::vector<double> const & y_coeffs,
        int order,
        float target_spacing);

/**
 * @brief Calculate squared fitting errors between points and parametric polynomial
 * 
 * Computes the squared Euclidean distance between each original point and the
 * corresponding point on the fitted parametric curve. Uses the same t-parameterization
 * as the original points.
 * 
 * @param points The original line points
 * @param x_coeffs Polynomial coefficients for x(t)
 * @param y_coeffs Polynomial coefficients for y(t)
 * @return Vector of squared errors, one per point, or empty if calculation fails
 * 
 * @note Returns squared errors (not absolute errors) for computational efficiency
 */
std::vector<float> calculate_fitting_errors(Line2D const & points,
                                            std::vector<double> const & x_coeffs,
                                            std::vector<double> const & y_coeffs);

/**
 * @brief Recursively remove outlier points from a line using polynomial fitting
 * 
 * Fits a parametric polynomial to the points, removes points with squared error
 * above the threshold, and repeats until no more outliers are found or max
 * iterations is reached.
 * 
 * @param points The line points to filter
 * @param error_threshold_squared Maximum allowed squared error for a point to be kept
 * @param polynomial_order The polynomial order used for fitting
 * @param max_iterations Maximum number of recursive iterations (default: 10)
 * @return Filtered line with outliers removed
 * 
 * @note This is the recursive implementation; prefer using remove_outliers() for the public API
 */
Line2D remove_outliers_recursive(Line2D const & points,
                                 float error_threshold_squared,
                                 int polynomial_order,
                                 int max_iterations = 10);

/**
 * @brief Remove outlier points from a line using iterative polynomial fitting
 * 
 * Iteratively fits a parametric polynomial to the line and removes points that
 * deviate more than the specified threshold from the fitted curve. This is useful
 * for cleaning up noisy data from skeletonization or edge detection.
 * 
 * @param points The line points to filter
 * @param error_threshold Maximum allowed error (in pixels) for a point to be kept
 * @param polynomial_order The polynomial order used for fitting
 * @return Filtered line with outliers removed
 * 
 * @note Requires at least (polynomial_order + 2) points to perform filtering
 */
Line2D remove_outliers(Line2D const & points,
                       float error_threshold,
                       int polynomial_order);

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

/**
 * @brief Extract a point at a fractional position along a line using polynomial interpolation
 * 
 * Uses parametric polynomial fitting to smoothly interpolate a point at the specified
 * fractional position along the line. Falls back to linear interpolation if there
 * aren't enough points for polynomial fitting.
 * 
 * @param line The line to extract a point from
 * @param position Fractional position along the line (0.0 = start, 1.0 = end)
 * @param polynomial_order Order of polynomial to use for fitting
 * @return The interpolated point, or std::nullopt if extraction fails
 */
std::optional<Point2D<float>> extract_parametric_point(Line2D const & line,
                                                       float position,
                                                       int polynomial_order);

#endif// PARAMETRIC_POLYNOMIAL_UTILS_HPP
