#ifndef STATE_ESTIMATION_LINE_LENGTH_EXTRACTOR_HPP
#define STATE_ESTIMATION_LINE_LENGTH_EXTRACTOR_HPP

#include "IFeatureExtractor.hpp"
#include "CoreGeometry/lines.hpp"

#include <Eigen/Dense>
#include <cmath>

namespace StateEstimation {

/**
 * @brief Feature extractor that computes line length
 * 
 * This extractor computes the total arc length of a line by summing
 * the Euclidean distances between consecutive points. Line length is
 * typically time-invariant or slowly varying, so it's treated as a
 * STATIC feature with no velocity tracking.
 * 
 * Features returned:
 * - Filter features: [length] (1D scalar)
 * - Initial state: [length] (1D, no velocity component)
 */
class LineLengthExtractor : public IFeatureExtractor<Line2D> {
public:
    /**
     * @brief Extract line length for Kalman filtering
     * 
     * Computes the total arc length of the line.
     * 
     * @param line The Line2D to extract features from (zero-copy reference)
     * @return 1D vector containing [length]
     */
    Eigen::VectorXd getFilterFeatures(Line2D const& line) const override {
        double length = computeLength(line);
        Eigen::VectorXd features(1);
        features << length;
        return features;
    }
    
    /**
     * @brief Extract all available features for assignment
     * 
     * For this extractor, we only compute length, so this returns
     * the same features as getFilterFeatures() but wrapped in a cache.
     * 
     * @param line The Line2D to extract features from (zero-copy reference)
     * @return FeatureCache with "line_length" feature
     */
    FeatureCache getAllFeatures(Line2D const& line) const override {
        FeatureCache cache;
        cache[getFilterFeatureName()] = getFilterFeatures(line);
        return cache;
    }
    
    /**
     * @brief Get the name identifier for filter features
     * 
     * @return "line_length"
     */
    std::string getFilterFeatureName() const override {
        return "line_length";
    }
    
    /**
     * @brief Create initial filter state from first observation
     * 
     * Initializes a 1D state vector [length] with moderate covariance
     * to indicate uncertainty in the length estimate.
     * 
     * @param line The Line2D to initialize from (zero-copy reference)
     * @return FilterState with 1D state (no velocity)
     */
    FilterState getInitialState(Line2D const& line) const override {
        double length = computeLength(line);
        
        Eigen::VectorXd state(1);
        state << length;
        
        Eigen::MatrixXd covariance(1, 1);
        covariance(0, 0) = 25.0;  // Moderate initial uncertainty (stddev ~5 pixels)
        
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
        return std::make_unique<LineLengthExtractor>(*this);
    }
    
    /**
     * @brief Get metadata for this feature
     * 
     * Line length is a STATIC feature (time-invariant, no velocity).
     * 
     * @return FeatureMetadata with STATIC type
     */
    FeatureMetadata getMetadata() const override {
        return FeatureMetadata::create("line_length", 1, FeatureTemporalType::STATIC);
    }
    
private:
    /**
     * @brief Compute the arc length of a line
     * 
     * Sums the Euclidean distances between consecutive points.
     * If the line has fewer than 2 points, returns 0.
     * 
     * @param line The Line2D to compute length for
     * @return Total arc length
     */
    static double computeLength(Line2D const& line) {
        if (line.size() < 2) {
            return 0.0;
        }
        
        double total_length = 0.0;
        
        for (size_t i = 1; i < line.size(); ++i) {
            auto const& p1 = line[i - 1];
            auto const& p2 = line[i];
            
            double dx = static_cast<double>(p2.x - p1.x);
            double dy = static_cast<double>(p2.y - p1.y);
            
            total_length += std::sqrt(dx * dx + dy * dy);
        }
        
        return total_length;
    }
};

} // namespace StateEstimation

#endif // STATE_ESTIMATION_LINE_LENGTH_EXTRACTOR_HPP
