#ifndef STATE_ESTIMATION_LINE_BASE_POINT_EXTRACTOR_HPP
#define STATE_ESTIMATION_LINE_BASE_POINT_EXTRACTOR_HPP

#include "IFeatureExtractor.hpp"
#include "CoreGeometry/lines.hpp"

#include <Eigen/Dense>

namespace StateEstimation {

/**
 * @brief Feature extractor that extracts the base point (first point) of a line
 * 
 * This extractor computes the first point in the line on-demand.
 * If the line is empty, it returns (0, 0).
 * 
 * Features returned:
 * - Filter features: [x_base, y_base] (2D position of first point)
 * - Initial state: [x, y, vx, vy] (position + zero velocity, high uncertainty)
 */
class LineBasePointExtractor : public IFeatureExtractor<Line2D> {
public:
    /**
     * @brief Extract base point features for Kalman filtering
     * 
     * Returns the 2D position of the first point in the line.
     * 
     * @param line The Line2D to extract features from (zero-copy reference)
     * @return 2D vector containing [x_base, y_base]
     */
    Eigen::VectorXd getFilterFeatures(Line2D const& line) const override {
        Eigen::Vector2d base_point = getBasePoint(line);
        Eigen::VectorXd features(2);
        features << base_point.x(), base_point.y();
        return features;
    }
    
    /**
     * @brief Extract all available features for assignment
     * 
     * For this extractor, we only compute base points, so this returns
     * the same features as getFilterFeatures() but wrapped in a cache.
     * 
     * @param line The Line2D to extract features from (zero-copy reference)
     * @return FeatureCache with "line_base_point" feature
     */
    FeatureCache getAllFeatures(Line2D const& line) const override {
        FeatureCache cache;
        cache[getFilterFeatureName()] = getFilterFeatures(line);
        return cache;
    }
    
    /**
     * @brief Get the name identifier for filter features
     * 
     * @return "line_base_point"
     */
    std::string getFilterFeatureName() const override {
        return "line_base_point";
    }
    
    /**
     * @brief Create initial filter state from first observation
     * 
     * Initializes a 4D state vector [x, y, vx, vy] with:
     * - Position from line base point
     * - Zero initial velocity
     * - High covariance (100.0) to indicate high initial uncertainty
     * 
     * @param line The Line2D to initialize from (zero-copy reference)
     * @return FilterState with 4D state (position + velocity)
     */
    FilterState getInitialState(Line2D const& line) const override {
        Eigen::Vector2d base_point = getBasePoint(line);
        
        Eigen::VectorXd state(4);
        state << base_point.x(), base_point.y(), 0.0, 0.0;  // Position + zero velocity
        
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
        return std::make_unique<LineBasePointExtractor>(*this);
    }
    
    /**
     * @brief Get metadata for this feature
     * 
     * Line base point is a 2D kinematic feature (position with velocity).
     * 
     * @return FeatureMetadata with KINEMATIC_2D type
     */
    FeatureMetadata getMetadata() const override {
        return FeatureMetadata::create("line_base_point", 2, FeatureTemporalType::KINEMATIC_2D);
    }
    
private:
    /**
     * @brief Get the base point (first point) of a line
     * 
     * Returns the first point in the line. If the line is empty, returns (0, 0).
     * 
     * @param line The Line2D to get the base point from
     * @return 2D base point position
     */
    static Eigen::Vector2d getBasePoint(Line2D const& line) {
        if (line.empty()) {
            return Eigen::Vector2d::Zero();
        }
        
        auto const& point = line.front();
        return Eigen::Vector2d(static_cast<double>(point.x), static_cast<double>(point.y));
    }
};

} // namespace StateEstimation

#endif // STATE_ESTIMATION_LINE_BASE_POINT_EXTRACTOR_HPP
