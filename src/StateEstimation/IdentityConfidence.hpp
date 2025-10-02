#ifndef STATE_ESTIMATION_IDENTITY_CONFIDENCE_HPP
#define STATE_ESTIMATION_IDENTITY_CONFIDENCE_HPP

#include <algorithm>
#include <cmath>

namespace StateEstimation {

/**
 * @brief Tracks identity confidence for multi-object tracking with assignment.
 * 
 * This class maintains a measure of how confident we are that we're tracking
 * the correct object identity. Confidence degrades with ambiguous assignments
 * and can only be fully restored by ground truth confirmation.
 * 
 * This is specifically for the discrete assignment problem in multi-object tracking,
 * not for general Kalman filtering (e.g., smoothing or outlier detection).
 * 
 * Use case: When identical objects enter occlusion together and emerge,
 * we may make an arbitrary assignment. Identity confidence captures this
 * persistent uncertainty until the next ground truth label.
 */
class IdentityConfidence {
public:
    /**
     * @brief Constructs with full confidence (1.0).
     */
    IdentityConfidence() = default;

    /**
     * @brief Updates confidence based on assignment quality.
     * 
     * Poor assignments (high cost) reduce confidence more than good assignments
     * increase it. Confidence has a floor to prevent complete loss of tracking.
     * 
     * @param assignment_cost The cost/distance from the assignment algorithm
     * @param cost_threshold The maximum cost for accepting an assignment
     */
    void updateOnAssignment(double assignment_cost, double cost_threshold) {
        // Normalize cost to [0, 1] range
        double normalized_cost = std::clamp(assignment_cost / cost_threshold, 0.0, 1.0);
        
        // Map to quality factor: 
        // cost = 0 → quality = 1.0 (excellent)
        // cost = threshold → quality = 0.0 (barely acceptable)
        double quality = 1.0 - normalized_cost;
        
        // Update confidence with strong penalty for poor quality
        // quality = 1.0 → factor = 1.0 (no change)
        // quality = 0.5 → factor = 0.95 (slight decrease)
        // quality = 0.0 → factor = 0.8 (large decrease)
        double decay_factor = 0.8 + 0.2 * quality;
        confidence_ *= decay_factor;
        
        // Maintain minimum confidence floor
        confidence_ = std::max(confidence_, min_confidence_floor_);
        
        // Track lowest confidence point since last anchor
        min_confidence_since_anchor_ = std::min(min_confidence_since_anchor_, confidence_);
    }

    /**
     * @brief Allows slow recovery with consistently good assignments.
     * 
     * This should be called for assignments with very low cost to allow
     * gradual confidence rebuilding. However, recovery is bounded - we can't
     * fully recover to 1.0 without ground truth.
     * 
     * @param assignment_cost The cost from assignment
     * @param excellent_threshold Cost below this is considered "excellent" (e.g., cost_threshold * 0.1)
     */
    void allowSlowRecovery(double assignment_cost, double excellent_threshold) {
        if (assignment_cost < excellent_threshold) {
            // Very good assignment - allow small confidence increase
            // But can't exceed 1.5× the minimum since last anchor
            double recovery_limit = std::min(1.0, min_confidence_since_anchor_ * 1.5);
            confidence_ = std::min(recovery_limit, confidence_ + recovery_rate_);
        }
    }

    /**
     * @brief Resets confidence to full (1.0) at ground truth frames.
     * 
     * Only explicit ground truth labels can fully restore identity confidence.
     */
    void resetOnGroundTruth() {
        confidence_ = 1.0;
        min_confidence_since_anchor_ = 1.0;
    }

    /**
     * @brief Gets the current confidence value [0.1, 1.0].
     */
    double getConfidence() const {
        return confidence_;
    }

    /**
     * @brief Computes the measurement noise scale factor based on confidence.
     * 
     * Low confidence inflates measurement noise R, making the filter less
     * certain about updates and easier to correct via backward smoothing.
     * 
     * Scaling function:
     * - confidence = 1.0 → scale = 1.0 (normal R)
     * - confidence = 0.5 → scale = 10.0
     * - confidence = 0.1 → scale = 100.0
     * 
     * @return Scale factor to multiply with base measurement noise matrix R
     */
    double getMeasurementNoiseScale() const {
        // Exponential scaling: R_scale = 10^(2 * (1 - confidence))
        // This creates strong inflation for low confidence
        return std::pow(10.0, 2.0 * (1.0 - confidence_));
    }

    /**
     * @brief Gets the minimum confidence reached since last ground truth.
     * 
     * This represents the "worst case" identity ambiguity in the current interval.
     */
    double getMinConfidenceSinceAnchor() const {
        return min_confidence_since_anchor_;
    }

    /**
     * @brief Sets the recovery rate for slow confidence rebuilding.
     * 
     * Default is 0.005 (0.5% per excellent assignment).
     * Set to 0.0 to disable recovery entirely (confidence only increases at ground truth).
     */
    void setRecoveryRate(double rate) {
        recovery_rate_ = std::clamp(rate, 0.0, 0.05);
    }

    /**
     * @brief Sets the minimum confidence floor.
     * 
     * Default is 0.1. Confidence will never drop below this value.
     */
    void setMinConfidenceFloor(double floor) {
        min_confidence_floor_ = std::clamp(floor, 0.01, 0.5);
    }

private:
    double confidence_ = 1.0;                        ///< Current confidence [min_floor, 1.0]
    double min_confidence_since_anchor_ = 1.0;       ///< Lowest confidence since last ground truth
    double recovery_rate_ = 0.005;                   ///< Recovery per excellent assignment
    double min_confidence_floor_ = 0.1;              ///< Absolute minimum confidence
};

} // namespace StateEstimation

#endif // STATE_ESTIMATION_IDENTITY_CONFIDENCE_HPP
