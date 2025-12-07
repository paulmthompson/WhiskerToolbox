#ifndef POLYNOMIAL_FIT_HPP
#define POLYNOMIAL_FIT_HPP

#include <vector>

class Line2D;

/**
 * @brief Fit a polynomial of the specified order to the given data
 *
 * @param x X-coordinates of the data points
 * @param y Y-coordinates of the data points
 * @param order The polynomial order to fit
 * @return std::vector<double> Polynomial coefficients (empty if fitting failed)
 */
std::vector<double> fit_polynomial(std::vector<double> const & x, std::vector<double> const & y, int order);

/**
 * @brief Evaluate polynomial at a given point
 *
 * @param coeffs The polynomial coefficients
 * @param x The x value at which to evaluate the polynomial
 * @return double The evaluated polynomial value
 */
double evaluate_polynomial(std::vector<double> const & coeffs, double x);

/**
 * @brief Evaluate polynomial derivative at a given point
 *
 * @param coeffs The polynomial coefficients
 * @param x The x value at which to evaluate the derivative
 * @return double The evaluated derivative value
 */
double evaluate_polynomial_derivative(std::vector<double> const & coeffs, double x);

double evaluate_polynomial_second_derivative(std::vector<double> const & coeffs, double t);


float calculate_polynomial_angle(Line2D const & line, float position, int polynomial_order,
                                 float reference_x, float reference_y);


#endif// POLYNOMIAL_FIT_HPP
