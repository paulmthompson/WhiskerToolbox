#include "LineFeatureExtractor.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace StateEstimation {

// ========== LineFeatureExtractor Implementation ==========

LineFeatureExtractor::LineFeatureExtractor(Config config)
    : config_(config) {}

FeatureVector LineFeatureExtractor::extractFeatures(Line2D const & line) const {
    FeatureVector features;

    if (line.empty()) {
        // Return empty features for empty lines
        return features;
    }

    // Extract centroid
    if (config_.extract_centroid) {
        Eigen::Vector2d centroid = calculateCentroid(line);
        centroid *= config_.position_scale;
        features.addFeature("centroid", FeatureType::POSITION, centroid, true);
    }

    // Extract length
    if (config_.extract_length) {
        double length = calculateLength(line);
        length *= config_.length_scale;

        Eigen::VectorXd length_vec(1);
        length_vec[0] = length;
        features.addFeature("length", FeatureType::SCALE, length_vec, false);
    }

    // Extract orientation
    if (config_.extract_orientation) {
        double orientation = calculateOrientation(line);
        if (config_.normalize_orientation) {
            // Normalize to [-pi, pi]
            while (orientation > std::numbers::pi) orientation -= 2.0 * std::numbers::pi;
            while (orientation <= -std::numbers::pi) orientation += 2.0 * std::numbers::pi;
        }

        Eigen::VectorXd orientation_vec(1);
        orientation_vec[0] = orientation;
        features.addFeature("orientation", FeatureType::ORIENTATION, orientation_vec, false);
    }

    // Extract bounding box
    if (config_.extract_bounding_box) {
        Eigen::Vector2d bbox_size = calculateBoundingBoxSize(line);
        bbox_size *= config_.position_scale;
        features.addFeature("bounding_box", FeatureType::SCALE, bbox_size, false);
    }

    // Extract endpoints
    if (config_.extract_endpoints) {
        auto endpoints = getEndpoints(line);

        Eigen::VectorXd endpoint_vec(4);// [start_x, start_y, end_x, end_y]
        endpoint_vec[0] = endpoints.first[0] * config_.position_scale;
        endpoint_vec[1] = endpoints.first[1] * config_.position_scale;
        endpoint_vec[2] = endpoints.second[0] * config_.position_scale;
        endpoint_vec[3] = endpoints.second[1] * config_.position_scale;

        features.addFeature("endpoints", FeatureType::POSITION, endpoint_vec, true);
    }

    // Extract curvature statistics
    if (config_.extract_curvature) {
        Eigen::Vector2d curvature_stats = calculateCurvatureStats(line);
        features.addFeature("curvature", FeatureType::SHAPE, curvature_stats, false);
    }

    return features;
}

std::vector<std::string> LineFeatureExtractor::getFeatureNames() const {
    std::vector<std::string> names;

    if (config_.extract_centroid) names.push_back("centroid");
    if (config_.extract_length) names.push_back("length");
    if (config_.extract_orientation) names.push_back("orientation");
    if (config_.extract_bounding_box) names.push_back("bounding_box");
    if (config_.extract_endpoints) names.push_back("endpoints");
    if (config_.extract_curvature) names.push_back("curvature");

    return names;
}

std::size_t LineFeatureExtractor::getFeatureDimension() const {
    std::size_t total_dim = 0;

    if (config_.extract_centroid) total_dim += 2;    // 2D centroid
    if (config_.extract_length) total_dim += 1;      // 1D length
    if (config_.extract_orientation) total_dim += 1; // 1D orientation
    if (config_.extract_bounding_box) total_dim += 2;// 2D bounding box
    if (config_.extract_endpoints) total_dim += 4;   // 4D endpoints (2 points)
    if (config_.extract_curvature) total_dim += 2;   // 2D curvature stats

    return total_dim;
}

Eigen::Vector2d LineFeatureExtractor::calculateCentroid(Line2D const & line) const {
    return LineFeatureUtils::calculateLineCentroid(line);
}

double LineFeatureExtractor::calculateLength(Line2D const & line) const {
    if (line.size() < 2) {
        return 0.0;
    }

    double total_length = 0.0;
    for (std::size_t i = 1; i < line.size(); ++i) {
        double dx = static_cast<double>(line[i].x - line[i - 1].x);
        double dy = static_cast<double>(line[i].y - line[i - 1].y);
        total_length += std::sqrt(dx * dx + dy * dy);
    }

    return total_length;
}

double LineFeatureExtractor::calculateOrientation(Line2D const & line) const {
    if (line.size() < 2) {
        return 0.0;
    }

    // Use PCA to find dominant direction
    auto pca_result = LineFeatureUtils::calculateLinePCA(line);

    // Calculate orientation of primary direction
    double orientation = std::atan2(pca_result.primary_direction[1], pca_result.primary_direction[0]);

    return orientation;
}

Eigen::Vector2d LineFeatureExtractor::calculateBoundingBoxSize(Line2D const & line) const {
    if (line.empty()) {
        return Eigen::Vector2d::Zero();
    }

    double min_x = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double min_y = std::numeric_limits<double>::max();
    double max_y = std::numeric_limits<double>::lowest();

    for (auto const & point: line) {
        double x = static_cast<double>(point.x);
        double y = static_cast<double>(point.y);

        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y);
    }

    return Eigen::Vector2d(max_x - min_x, max_y - min_y);
}

std::pair<Eigen::Vector2d, Eigen::Vector2d> LineFeatureExtractor::getEndpoints(Line2D const & line) const {
    if (line.empty()) {
        return {Eigen::Vector2d::Zero(), Eigen::Vector2d::Zero()};
    }

    if (line.size() == 1) {
        Eigen::Vector2d point(static_cast<double>(line[0].x), static_cast<double>(line[0].y));
        return {point, point};
    }

    Eigen::Vector2d start(static_cast<double>(line.front().x), static_cast<double>(line.front().y));
    Eigen::Vector2d end(static_cast<double>(line.back().x), static_cast<double>(line.back().y));

    return {start, end};
}

Eigen::Vector2d LineFeatureExtractor::calculateCurvatureStats(Line2D const & line) const {
    auto curvatures = LineFeatureUtils::calculateCurvature(line);

    if (curvatures.empty()) {
        return Eigen::Vector2d::Zero();
    }

    // Calculate mean and standard deviation of curvatures
    double mean = 0.0;
    for (double curv: curvatures) {
        mean += curv;
    }
    mean /= static_cast<double>(curvatures.size());

    double variance = 0.0;
    for (double curv: curvatures) {
        double diff = curv - mean;
        variance += diff * diff;
    }
    variance /= static_cast<double>(curvatures.size());
    double std_dev = std::sqrt(variance);

    return Eigen::Vector2d(mean, std_dev);
}

// ========== LineFeatureUtils Implementation ==========

namespace LineFeatureUtils {

Eigen::Vector2d calculateLineCentroid(Line2D const & line) {
    if (line.empty()) {
        return Eigen::Vector2d::Zero();
    }

    double sum_x = 0.0;
    double sum_y = 0.0;

    for (auto const & point: line) {
        sum_x += static_cast<double>(point.x);
        sum_y += static_cast<double>(point.y);
    }

    double count = static_cast<double>(line.size());
    return Eigen::Vector2d(sum_x / count, sum_y / count);
}

PCAResult calculateLinePCA(Line2D const & line) {
    PCAResult result;

    if (line.size() < 2) {
        if (!line.empty()) {
            result.mean = Eigen::Vector2d(static_cast<double>(line[0].x), static_cast<double>(line[0].y));
        } else {
            result.mean = Eigen::Vector2d::Zero();
        }
        result.primary_direction = Eigen::Vector2d(1.0, 0.0);
        result.secondary_direction = Eigen::Vector2d(0.0, 1.0);
        result.primary_variance = 0.0;
        result.secondary_variance = 0.0;
        return result;
    }

    // Calculate mean
    result.mean = calculateLineCentroid(line);

    // Build data matrix (centered)
    Eigen::MatrixXd data(2, line.size());
    for (std::size_t i = 0; i < line.size(); ++i) {
        data(0, i) = static_cast<double>(line[i].x) - result.mean[0];
        data(1, i) = static_cast<double>(line[i].y) - result.mean[1];
    }

    // Calculate covariance matrix
    Eigen::Matrix2d covariance = data * data.transpose() / static_cast<double>(line.size() - 1);

    // Eigen decomposition
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix2d> eigen_solver(covariance);

    if (eigen_solver.info() == Eigen::Success) {
        Eigen::Vector2d eigenvalues = eigen_solver.eigenvalues();
        Eigen::Matrix2d eigenvectors = eigen_solver.eigenvectors();

        // Sort by eigenvalue (largest first)
        if (eigenvalues[0] > eigenvalues[1]) {
            result.primary_variance = eigenvalues[0];
            result.secondary_variance = eigenvalues[1];
            result.primary_direction = eigenvectors.col(0);
            result.secondary_direction = eigenvectors.col(1);
        } else {
            result.primary_variance = eigenvalues[1];
            result.secondary_variance = eigenvalues[0];
            result.primary_direction = eigenvectors.col(1);
            result.secondary_direction = eigenvectors.col(0);
        }
    } else {
        // Fallback to axis-aligned directions
        result.primary_direction = Eigen::Vector2d(1.0, 0.0);
        result.secondary_direction = Eigen::Vector2d(0.0, 1.0);
        result.primary_variance = covariance(0, 0);
        result.secondary_variance = covariance(1, 1);
    }

    return result;
}

std::vector<double> calculateCurvature(Line2D const & line, int window_size) {
    std::vector<double> curvatures;

    if (line.size() < 3) {
        return curvatures;// Need at least 3 points for curvature
    }

    int half_window = window_size / 2;

    for (int i = half_window; i < static_cast<int>(line.size()) - half_window; ++i) {
        // Get points for curvature calculation
        int prev_idx = std::max(0, i - half_window);
        int next_idx = std::min(static_cast<int>(line.size()) - 1, i + half_window);

        Eigen::Vector2d p1(static_cast<double>(line[prev_idx].x), static_cast<double>(line[prev_idx].y));
        Eigen::Vector2d p2(static_cast<double>(line[i].x), static_cast<double>(line[i].y));
        Eigen::Vector2d p3(static_cast<double>(line[next_idx].x), static_cast<double>(line[next_idx].y));

        // Calculate curvature using the formula: k = |det(v1, v2)| / |v1|^3
        // where v1 = p2 - p1, v2 = p3 - p2
        Eigen::Vector2d v1 = p2 - p1;
        Eigen::Vector2d v2 = p3 - p2;

        double cross_product = v1[0] * v2[1] - v1[1] * v2[0];
        double v1_norm = v1.norm();

        if (v1_norm > 1e-8) {
            double curvature = std::abs(cross_product) / (v1_norm * v1_norm * v1_norm);
            curvatures.push_back(curvature);
        } else {
            curvatures.push_back(0.0);
        }
    }

    return curvatures;
}

LineFitResult fitLineToPoints(Line2D const & line) {
    LineFitResult result;

    if (line.size() < 2) {
        result.point_on_line = Eigen::Vector2d::Zero();
        result.direction = Eigen::Vector2d(1.0, 0.0);
        result.total_residual = 0.0;
        return result;
    }

    // Use PCA to fit line
    auto pca_result = calculateLinePCA(line);

    result.point_on_line = pca_result.mean;
    result.direction = pca_result.primary_direction;

    // Calculate residuals (distance from each point to the fitted line)
    result.point_residuals.reserve(line.size());
    result.total_residual = 0.0;

    for (auto const & point: line) {
        Eigen::Vector2d p(static_cast<double>(point.x), static_cast<double>(point.y));
        Eigen::Vector2d point_to_line = p - result.point_on_line;

        // Distance from point to line (perpendicular distance)
        double distance = std::abs(point_to_line.dot(Eigen::Vector2d(-result.direction[1], result.direction[0])));

        result.point_residuals.push_back(distance);
        result.total_residual += distance;
    }

    return result;
}

}// namespace LineFeatureUtils

}// namespace StateEstimation