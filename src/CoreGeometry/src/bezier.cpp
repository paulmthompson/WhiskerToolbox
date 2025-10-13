#include "CoreGeometry/bezier.hpp"
#include "CoreGeometry/points.hpp"
#include <stdexcept>

// The following code is a C++ adaptation of the Python code provided by the user.
// The Python code was modified from stackedoverflow
// https://stackoverflow.com/questions/12643079/b%C3%A9zier-curve-fitting-with-scipy
// Based on https://stackoverflow.com/questions/12643079/b%C3%A9zier-curve-fitting-with-scipy
// and probably on the 1998 thesis by Tim Andrew Pastva, "Bézier Curve Fitting".

namespace CoreGeometry {

    // Helper function to calculate combinations (nCk)
    long long combinations(int n, int k) {
        if (k < 0 || k > n) {
            return 0;
        }
        if (k == 0 || k == n) {
            return 1;
        }
        if (k > n / 2) {
            k = n - k;
        }
        long long res = 1;
        for (int i = 1; i <= k; ++i) {
            res = res * (n - i + 1) / i;
        }
        return res;
    }

    // Helper for getBezierParameters
    double bernstein_polynomial(int n, double t, int k) {
        return combinations(n, k) * std::pow(t, k) * std::pow(1 - t, n - k);
    }

    std::vector<Point2D<float>> getBezierParameters(const std::vector<Point2D<float>>& points, int degree) {
        if (degree < 1) {
            throw std::invalid_argument("degree must be 1 or greater.");
        }
        if (points.size() < static_cast<size_t>(degree + 1)) {
            throw std::invalid_argument("There must be at least " + std::to_string(degree + 1) +
                                       " points to determine the parameters of a degree " + std::to_string(degree) +
                                       " curve. Got only " + std::to_string(points.size()) + " points.");
        }

        Eigen::VectorXd T = Eigen::VectorXd::LinSpaced(points.size(), 0.0, 1.0);
        Eigen::MatrixXd M(points.size(), degree + 1);
        for (int i = 0; i < points.size(); ++i) {
            for (int j = 0; j <= degree; ++j) {
                M(i, j) = bernstein_polynomial(degree, T(i), j);
            }
        }

        Eigen::MatrixXd points_matrix(points.size(), 2);
        for (size_t i = 0; i < points.size(); ++i) {
            points_matrix(i, 0) = points[i].x();
            points_matrix(i, 1) = points[i].y();
        }

        Eigen::MatrixXd control_points_matrix = M.completeOrthogonalDecomposition().pseudoInverse() * points_matrix;

        std::vector<Point2D<float>> control_points;
        for (int i = 0; i < control_points_matrix.rows(); ++i) {
            control_points.emplace_back(control_points_matrix(i, 0), control_points_matrix(i, 1));
        }

        control_points.front() = points.front();
        control_points.back() = points.back();

        return control_points;
    }

    // Helper for bezierCurve
    double bernstein_poly(int i, int n, double t) {
        return combinations(n, i) * std::pow(t, i) * std::pow(1 - t, n - i);
    }


    Line2D bezierCurve(const std::vector<Point2D<float>>& points, int nTimes) {
        int nPoints = points.size();
        Eigen::VectorXf xPoints(nPoints);
        Eigen::VectorXf yPoints(nPoints);

        for(int i=0; i < nPoints; ++i) {
            xPoints(i) = points[i].x();
            yPoints(i) = points[i].y();
        }

        Eigen::VectorXd t = Eigen::VectorXd::LinSpaced(nTimes, 0.0, 1.0);
        Eigen::MatrixXd polynomial_array(nPoints, nTimes);

        for (int i = 0; i < nPoints; ++i) {
            for (int j = 0; j < nTimes; ++j) {
                polynomial_array(i, j) = bernstein_poly(i, nPoints - 1, t(j));
            }
        }

        Eigen::VectorXd xvals = xPoints.cast<double>().transpose() * polynomial_array;
        Eigen::VectorXd yvals = yPoints.cast<double>().transpose() * polynomial_array;

        Line2D curve;
        for (int i = 0; i < nTimes; ++i) {
            curve.push_back({static_cast<float>(xvals(i)), static_cast<float>(yvals(i))});
        }
        return curve;
    }

    Line2D maskToLine(const Mask2D& mask, int degree, int n_points) {
        if (mask.empty()) {
            return Line2D();
        }

        std::vector<Point2D<float>> float_points;
        float_points.reserve(mask.size());
        for(const auto& p : mask) {
            float_points.push_back({static_cast<float>(p.x()), static_cast<float>(p.y())});
        }

        auto bezier_coeffs = getBezierParameters(float_points, degree);
        return bezierCurve(bezier_coeffs, n_points);
    }

} // namespace CoreGeometry
