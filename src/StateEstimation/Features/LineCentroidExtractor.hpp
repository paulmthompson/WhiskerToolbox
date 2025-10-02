#ifndef STATE_ESTIMATION_LINE_CENTROID_EXTRACTOR_HPP
#define STATE_ESTIMATION_LINE_CENTROID_EXTRACTOR_HPP

#include "IFeatureExtractor.hpp"
#include "CoreGeometry/lines.hpp"

#include <Eigen/Dense>

namespace StateEstimation {

/**
 * @brief Feature extractor that computes line centroids on-demand
 * 
 * This extractor computes the centroid (center of mass) of a Line2D object
 * when features are requested, rather than pre-computing and caching.
 * This approach is more memory-efficient and only computes features for
 * lines that are actually used in tracking.
 * 
 * The centroid is computed as the mean of all points in the line:
 *   centroid = (sum of all points) / (number of points)
 * 
 * Features returned:
 * - Filter features: [x_centroid, y_centroid] (2D position)
 * - Initial state: [x, y, vx, vy] (position + zero velocity, high uncertainty)
 */
class LineCentroidExtractor : public IFeatureExtractor<Line2D> {
public:
    /**
     * @brief Extract centroid features for Kalman filtering
     * 
     * Computes the 2D centroid of the line on-demand.
     * 
     * @param line The Line2D to extract features from (zero-copy reference)
     * @return 2D vector containing [x_centroid, y_centroid]
     */
    Eigen::VectorXd getFilterFeatures(Line2D const& line) const override {
        Eigen::Vector2d centroid = computeCentroid(line);
        Eigen::VectorXd features(2);
        features << centroid.x(), centroid.y();
        return features;
    }
    
    /**
     * @brief Extract all available features for assignment
     * 
     * For this extractor, we only compute centroids, so this returns
     * the same features as getFilterFeatures() but wrapped in a cache.
     * 
     * @param line The Line2D to extract features from (zero-copy reference)
     * @return FeatureCache with "line_centroid" feature
     */
    FeatureCache getAllFeatures(Line2D const& line) const override {
        FeatureCache cache;
        cache[getFilterFeatureName()] = getFilterFeatures(line);
        return cache;
    }
    
    /**
     * @brief Get the name identifier for filter features
     * 
     * @return "line_centroid"
     */
    std::string getFilterFeatureName() const override {
        return "line_centroid";
    }
    
    /**
     * @brief Create initial filter state from first observation
     * 
     * Initializes a 4D state vector [x, y, vx, vy] with:
     * - Position from line centroid
     * - Zero initial velocity
     * - High covariance (100.0) to indicate high initial uncertainty
     * 
     * @param line The Line2D to initialize from (zero-copy reference)
     * @return FilterState with 4D state (position + velocity)
     */
    FilterState getInitialState(Line2D const& line) const override {
        Eigen::Vector2d centroid = computeCentroid(line);
        
        Eigen::VectorXd state(4);
        state << centroid.x(), centroid.y(), 0.0, 0.0;  // Position + zero velocity
        
        Eigen::MatrixXd covariance(4, 4);
        covariance.setIdentity();
        covariance *= 100.0;  // High initial uncertainty
        
        return FilterState{
            .state_mean = state,
            .state_covariance = covariance
        };
    }
    
    /**
     * @brief Clone this feature extractor
     * 
     * @return A unique_ptr to a copy of this extractor
     */
    std::unique_ptr<IFeatureExtractor<Line2D>> clone() const override {
        return std::make_unique<LineCentroidExtractor>(*this);
    }
    
    /**
     * @brief Get metadata for this feature
     * 
     * Line centroid is a 2D kinematic feature (position with velocity).
     * 
     * @return FeatureMetadata with KINEMATIC_2D type
     */
    FeatureMetadata getMetadata() const override {
        return FeatureMetadata::create("line_centroid", 2, FeatureTemporalType::KINEMATIC_2D);
    }
    
private:
    /**
     * @brief Compute the centroid (center of mass) of a line
     * 
     * The centroid is the mean position of all points in the line.
     * If the line is empty, returns (0, 0).
     * 
     * @param line The Line2D to compute centroid for
     * @return 2D centroid position
     */
    static Eigen::Vector2d computeCentroid(Line2D const& line) {
        if (line.empty()) {
            return Eigen::Vector2d::Zero();
        }
        
        double sum_x = 0.0;
        double sum_y = 0.0;
        
        for (auto const& point : line) {
            sum_x += static_cast<double>(point.x);
            sum_y += static_cast<double>(point.y);
        }
        
        double count = static_cast<double>(line.size());
        return Eigen::Vector2d(sum_x / count, sum_y / count);
    }
};

} // namespace StateEstimation

#endif // STATE_ESTIMATION_LINE_CENTROID_EXTRACTOR_HPP
