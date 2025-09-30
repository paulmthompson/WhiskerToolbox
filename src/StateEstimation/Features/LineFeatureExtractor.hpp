#ifndef STATE_ESTIMATION_LINE_FEATURE_EXTRACTOR_HPP
#define STATE_ESTIMATION_LINE_FEATURE_EXTRACTOR_HPP

#include "CoreGeometry/lines.hpp"
#include "Features/FeatureVector.hpp"

#include <Eigen/Dense>

namespace StateEstimation {


/**
     * @brief Configuration for line feature extraction
     */
struct Config {
    bool extract_centroid = true;     // Extract centroid position
    bool extract_length = true;       // Extract line length
    bool extract_orientation = true;  // Extract line orientation
    bool extract_bounding_box = false;// Extract bounding box dimensions
    bool extract_endpoints = false;   // Extract endpoint positions
    bool extract_curvature = false;   // Extract curvature statistics

    // Normalization parameters
    double position_scale = 1.0;      // Scale factor for positions
    double length_scale = 1.0;        // Scale factor for lengths
    bool normalize_orientation = true;// Normalize orientation to [-pi, pi]
};

/**
 * @brief Feature extractor for Line2D objects
 * 
 * Extracts various geometric features from Line2D objects including:
 * - Centroid position
 * - Length
 * - Orientation
 * - Bounding box properties
 * - Shape descriptors
 */
class LineFeatureExtractor : public FeatureExtractor<Line2D> {
public:
    /**
     * @brief Construct with configuration
     */
    explicit LineFeatureExtractor(Config config = Config());

    /**
     * @brief Extract features from a Line2D object
     */
    FeatureVector extractFeatures(Line2D const & line) const override;

    /**
     * @brief Get feature names produced by this extractor
     */
    std::vector<std::string> getFeatureNames() const override;

    /**
     * @brief Get expected feature dimension
     */
    std::size_t getFeatureDimension() const override;

    /**
     * @brief Update configuration
     */
    void setConfig(Config config) { config_ = config; }

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] Config const & getConfig() const { return config_; }

private:
    Config config_;

    /**
     * @brief Calculate centroid of a line
     */
    Eigen::Vector2d calculateCentroid(Line2D const & line) const;

    /**
     * @brief Calculate total length of a line
     */
    double calculateLength(Line2D const & line) const;

    /**
     * @brief Calculate dominant orientation of a line
     */
    double calculateOrientation(Line2D const & line) const;

    /**
     * @brief Calculate bounding box dimensions
     */
    Eigen::Vector2d calculateBoundingBoxSize(Line2D const & line) const;

    /**
     * @brief Get endpoint positions
     */
    std::pair<Eigen::Vector2d, Eigen::Vector2d> getEndpoints(Line2D const & line) const;

    /**
     * @brief Calculate curvature statistics
     */
    Eigen::Vector2d calculateCurvatureStats(Line2D const & line) const;
};

/**
 * @brief Utility functions for line feature extraction
 */
namespace LineFeatureUtils {

/**
     * @brief Calculate centroid of a line (same as existing function)
     */
Eigen::Vector2d calculateLineCentroid(Line2D const & line);

/**
     * @brief Calculate principal component analysis of line points
     */
struct PCAResult {
    Eigen::Vector2d mean;
    Eigen::Vector2d primary_direction;
    Eigen::Vector2d secondary_direction;
    double primary_variance;
    double secondary_variance;
};

PCAResult calculateLinePCA(Line2D const & line);

/**
     * @brief Calculate line curvature at each point
     */
std::vector<double> calculateCurvature(Line2D const & line, int window_size = 3);

/**
     * @brief Fit line to points and return residuals
     */
struct LineFitResult {
    Eigen::Vector2d point_on_line;
    Eigen::Vector2d direction;
    double total_residual;
    std::vector<double> point_residuals;
};

LineFitResult fitLineToPoints(Line2D const & line);
}// namespace LineFeatureUtils

}// namespace StateEstimation

#endif// STATE_ESTIMATION_LINE_FEATURE_EXTRACTOR_HPP