#ifndef POLYNOMIAL_FIT_HPP
#define POLYNOMIAL_FIT_HPP

#include "CoreGeometry/points.hpp" // For Point2D

#include <vector>

class Line2D;

/**
 * @brief Fit a polynomial of the specified order to the given data
 *
 * @param x X-coordinates of the data points
 * @param y Y-coordinates of the data points
 * @param order The polynomial order to fit
 * @return std::vector<double> Polynomial coefficients (empty if fitting failed)
 *
 * @pre x and y have the same size (enforcement: runtime_check)
 * @pre order >= 0 (enforcement: runtime_check)
 * @pre x.size() > order for a successful fit; otherwise returns empty (enforcement: runtime_check)
 */
std::vector<double> fit_polynomial(std::vector<double> const & x, std::vector<double> const & y, int order);

/**
 * @brief Evaluate polynomial at a given point
 *
 * @param coeffs The polynomial coefficients (c0, c1, ... for c0 + c1*x + c2*x^2 + ...)
 * @param x The x value at which to evaluate the polynomial
 * @return double The evaluated polynomial value
 *
 * @pre For a non-constant-zero result, coeffs must not be empty (enforcement: none)
 * @post If coeffs is empty, returns 0.0
 */
double evaluate_polynomial(std::vector<double> const & coeffs, double x);

/**
 * @brief Evaluate polynomial derivative at a given point
 *
 * @param coeffs The polynomial coefficients (c0, c1, ... for poly c0 + c1*x + ...)
 * @param x The x value at which to evaluate the derivative
 * @return double The evaluated derivative value
 *
 * @pre For a non-constant-zero derivative, coeffs.size() >= 2 (enforcement: none)
 * @post If coeffs is empty or size 1, returns 0.0
 */
double evaluate_polynomial_derivative(std::vector<double> const & coeffs, double x);

/**
 * @brief Evaluate polynomial second derivative at a given point
 *
 * @param coeffs The polynomial coefficients (c0, c1, ... for poly c0 + c1*t + ...)
 * @param t The parameter value at which to evaluate the second derivative
 * @return double The evaluated second derivative value
 *
 * @pre For a non-constant-zero second derivative, coeffs.size() >= 3 (enforcement: none)
 * @post If coeffs.size() < 3, returns 0.0
 */
double evaluate_polynomial_second_derivative(std::vector<double> const & coeffs, double t);


/**
 * @brief Calculate angle at a position along a line using polynomial fit derivative
 *
 * Fits parametric polynomials x(t) and y(t) to the line, then returns the angle of the
 * tangent (dx/dt, dy/dt) at the given position, normalized with respect to the reference
 * vector. Falls back to direct angle calculation if the line has too few points.
 *
 * @param line The line to evaluate
 * @param position Fractional position along the line (0.0 to 1.0)
 * @param polynomial_order Order of polynomial to fit
 * @param reference_x X component of reference direction for angle zero
 * @param reference_y Y component of reference direction for angle zero
 * @return Angle in degrees relative to the reference vector
 *
 * @pre line.size() >= 2 for polynomial fit path; otherwise falls back (enforcement: none)
 * @pre polynomial_order >= 0 (enforcement: none; propagates to fit_polynomial)
 * @pre line.size() > polynomial_order for polynomial fit; otherwise falls back to direct angle
 */
float calculate_polynomial_angle(Line2D const & line, float position, int polynomial_order,
                                 float reference_x, float reference_y);

/**
 * @brief Extract a subsegment of a line using parametric polynomial interpolation
 *
 * Fits parametric polynomials x(t) and y(t) to the line over [0,1], then evaluates
 * at evenly spaced t in [start_pos, end_pos]. Falls back to distance-based extraction
 * if the line has too few points or fitting fails.
 *
 * @param line The line to extract from
 * @param start_pos Start of subsegment (0.0 to 1.0), clamped internally
 * @param end_pos End of subsegment (0.0 to 1.0), clamped internally
 * @param polynomial_order Order of polynomial to fit
 * @param output_points Number of points in the output subsegment
 * @return Points along the subsegment, or empty if start_pos >= end_pos or line empty
 *
 * @pre polynomial_order >= 0 (enforcement: runtime_check; fallback if line too small)
 * @pre output_points >= 2 for polynomial path; otherwise division by zero (enforcement: none)
 */
std::vector<Point2D<float>> extract_parametric_subsegment(Line2D const & line,
                                                          float start_pos,
                                                          float end_pos,
                                                          int polynomial_order,
                                                          int output_points);

#endif// POLYNOMIAL_FIT_HPP
