#ifndef COREGEOMETRY_BEZIER_HPP
#define COREGEOMETRY_BEZIER_HPP

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include <vector>
#include "Eigen/Dense"

// The following code is a C++ adaptation of the Python code provided by the user.
// The Python code was modified from stackedoverflow
// https://stackoverflow.com/questions/12643079/b%C3%A9zier-curve-fitting-with-scipy
// Based on https://stackoverflow.com/questions/12643079/b%C3%A9zier-curve-fitting-with-scipy
// and probably on the 1998 thesis by Tim Andrew Pastva, "Bézier Curve Fitting".

namespace CoreGeometry {

    /**
     * @brief Least square qbezier fit using penrose pseudoinverse.
     * @param points A vector of 2D points.
     * @param degree The degree of the Bézier curve.
     * @return A vector of control points for the Bézier curve.
     */
    std::vector<Point2D<float>> getBezierParameters(const std::vector<Point2D<float>>& points, int degree = 3);

    /**
     * @brief Given a set of control points, return the Bézier curve defined by the control points.
     * @param points The control points.
     * @param nTimes The number of points to generate on the curve.
     * @return A Line2D representing the Bézier curve.
     */
    Line2D bezierCurve(const std::vector<Point2D<float>>& points, int nTimes = 50);

    /**
     * @brief Converts a Mask2D to a Line2D using a Bézier fit.
     * @param mask The mask to convert.
     * @param degree The degree of the Bézier curve.
     * @param n_points The number of points to generate on the curve.
     * @return A Line2D representing the best fit Bézier curve for the mask.
     */
    Line2D maskToLine(const Mask2D& mask, int degree, int n_points);

} // namespace CoreGeometry

#endif // COREGEOMETRY_BEZIER_HPP
