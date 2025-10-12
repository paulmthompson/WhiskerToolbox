#ifndef WHISKERTOOLBOX_MASK_PARTICLE_FILTER_HPP
#define WHISKERTOOLBOX_MASK_PARTICLE_FILTER_HPP

#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <random>
#include <unordered_set>
#include <vector>

namespace StateEstimation {

// ============================================================================
// Utility Functions for Discrete Mask-Based Tracking
// ============================================================================

/**
 * @brief Hash function for Point2D<uint32_t> to use in unordered_set
 */
struct PointHash {
    std::size_t operator()(Point2D<uint32_t> const& p) const noexcept {
        // Combine x and y using a simple hash combination
        return std::hash<uint32_t>{}(p.x) ^ (std::hash<uint32_t>{}(p.y) << 1);
    }
};

/**
 * @brief Equality function for Point2D<uint32_t>
 */
struct PointEqual {
    bool operator()(Point2D<uint32_t> const& a, Point2D<uint32_t> const& b) const noexcept {
        return a.x == b.x && a.y == b.y;
    }
};

/**
 * @brief Compute Euclidean distance between two points
 */
inline float pointDistance(Point2D<uint32_t> const& a, Point2D<uint32_t> const& b) {
    float dx = static_cast<float>(a.x) - static_cast<float>(b.x);
    float dy = static_cast<float>(a.y) - static_cast<float>(b.y);
    return std::sqrt(dx * dx + dy * dy);
}

/**
 * @brief Find the closest pixel in a mask to a given point
 * 
 * @param target_point The point to find the nearest mask pixel to
 * @param mask The mask containing candidate pixels
 * @return The closest pixel in the mask, or the target point if mask is empty
 */
inline Point2D<uint32_t> findNearestMaskPixel(Point2D<uint32_t> const& target_point, Mask2D const& mask) {
    if (mask.empty()) {
        return target_point;
    }
    
    Point2D<uint32_t> nearest = mask[0];
    float min_dist = pointDistance(target_point, nearest);
    
    for (size_t i = 1; i < mask.size(); ++i) {
        float dist = pointDistance(target_point, mask[i]);
        if (dist < min_dist) {
            min_dist = dist;
            nearest = mask[i];
        }
    }
    
    return nearest;
}

// ============================================================================
// Particle Structure
// ============================================================================

/**
 * @brief A weighted particle representing a discrete state (pixel location)
 */
struct Particle {
    Point2D<uint32_t> position;  // Current pixel position
    float weight;                 // Particle weight (unnormalized log weight)
    
    Particle() : position{0, 0}, weight(0.0f) {}
    Particle(Point2D<uint32_t> pos, float w) : position(pos), weight(w) {}
};

// ============================================================================
// Single Point Discrete Particle Filter
// ============================================================================

/**
 * @brief A discrete particle filter for tracking a single point through mask data
 * 
 * This filter maintains particles that are constrained to lie on mask pixels.
 * It performs forward filtering and backward smoothing between ground truth labels.
 */
class MaskPointTracker {
public:
    /**
     * @brief Construct a new MaskPointTracker
     * 
     * @param num_particles Number of particles to use
     * @param transition_radius Maximum distance a particle can move in one time step (in pixels)
     * @param random_walk_prob Probability of random walk vs staying on nearby mask pixels
     */
    explicit MaskPointTracker(size_t num_particles = 1000, 
                              float transition_radius = 10.0f,
                              float random_walk_prob = 0.1f)
        : num_particles_(num_particles),
          transition_radius_(transition_radius),
          random_walk_prob_(random_walk_prob) {
        rng_.seed(std::random_device{}());
    }
    
    /**
     * @brief Track a point through a sequence of masks between two ground truth labels
     * 
     * @param start_point Ground truth starting point
     * @param end_point Ground truth ending point
     * @param masks Vector of masks for each time frame (in order)
     * @return Vector of tracked points (one per mask frame)
     */
    std::vector<Point2D<uint32_t>> track(
        Point2D<uint32_t> const& start_point,
        Point2D<uint32_t> const& end_point,
        std::vector<Mask2D> const& masks);
    
private:
    // Core particle filter operations
    void initializeParticles(Point2D<uint32_t> const& start_point, Mask2D const& first_mask);
    void predict(Mask2D const& current_mask);
    void resample();
    Point2D<uint32_t> getWeightedMeanPosition() const;
    
    // Helper functions
    std::vector<Point2D<uint32_t>> getNeighborPixels(
        Point2D<uint32_t> const& center, 
        Mask2D const& mask, 
        float radius) const;
    
    Point2D<uint32_t> sampleFromNeighbors(
        Point2D<uint32_t> const& current, 
        Mask2D const& mask) const;
    
    // Backward smoothing
    std::vector<Point2D<uint32_t>> backwardSmooth(
        std::vector<std::vector<Particle>> const& forward_history,
        std::vector<Mask2D> const& masks,
        Point2D<uint32_t> const& end_point) const;
    
    Point2D<uint32_t> selectBestParticle(
        std::vector<Particle> const& particles,
        Point2D<uint32_t> const& next_selected) const;
    
    // Parameters
    size_t num_particles_;
    float transition_radius_;
    float random_walk_prob_;
    
    // State
    std::vector<Particle> particles_;
    mutable std::mt19937 rng_;
};

// ============================================================================
// Multi-Point Correlated Tracker
// ============================================================================

/**
 * @brief State for tracking multiple correlated points
 */
struct MultiPointState {
    std::vector<Point2D<uint32_t>> points;
    
    MultiPointState() = default;
    explicit MultiPointState(size_t n) : points(n) {}
    explicit MultiPointState(std::vector<Point2D<uint32_t>> pts) : points(std::move(pts)) {}
};

/**
 * @brief Particle for multi-point tracking
 */
struct MultiPointParticle {
    MultiPointState state;
    float weight;
    
    MultiPointParticle() : weight(0.0f) {}
    MultiPointParticle(MultiPointState s, float w) : state(std::move(s)), weight(w) {}
};

/**
 * @brief A discrete particle filter for tracking multiple correlated points
 * 
 * This filter tracks multiple points that are expected to maintain consistent
 * relative spacing (e.g., points along the same whisker/line). It uses correlation
 * constraints to improve tracking accuracy.
 */
class CorrelatedMaskPointTracker {
public:
    /**
     * @brief Construct a new CorrelatedMaskPointTracker
     * 
     * @param num_particles Number of particles to use
     * @param transition_radius Maximum distance each point can move in one time step
     * @param correlation_strength Strength of correlation constraint [0, 1]
     *                            0 = independent tracking, 1 = rigid constraint
     */
    explicit CorrelatedMaskPointTracker(size_t num_particles = 1000,
                                        float transition_radius = 10.0f,
                                        float correlation_strength = 0.7f)
        : num_particles_(num_particles),
          transition_radius_(transition_radius),
          correlation_strength_(std::max(0.0f, std::min(1.0f, correlation_strength))) {
        rng_.seed(std::random_device{}());
    }
    
    /**
     * @brief Track multiple points through mask sequences
     * 
     * @param start_points Ground truth starting points
     * @param end_points Ground truth ending points
     * @param masks Vector of masks for each time frame
     * @return Vector of multi-point states (one per frame)
     */
    std::vector<MultiPointState> track(
        std::vector<Point2D<uint32_t>> const& start_points,
        std::vector<Point2D<uint32_t>> const& end_points,
        std::vector<Mask2D> const& masks);

private:
    void initializeParticles(std::vector<Point2D<uint32_t>> const& start_points, 
                            Mask2D const& first_mask);
    void predict(Mask2D const& current_mask);
    void applyCorrelationConstraint();
    void resample();
    MultiPointState getWeightedMeanState() const;
    
    std::vector<MultiPointState> backwardSmooth(
        std::vector<std::vector<MultiPointParticle>> const& forward_history,
        std::vector<Mask2D> const& masks,
        std::vector<Point2D<uint32_t>> const& end_points) const;
    
    MultiPointState selectBestParticle(
        std::vector<MultiPointParticle> const& particles,
        MultiPointState const& next_selected) const;
    
    float computeStateDistance(MultiPointState const& a, MultiPointState const& b) const;
    
    // Parameters
    size_t num_particles_;
    float transition_radius_;
    float correlation_strength_;
    
    // State
    std::vector<MultiPointParticle> particles_;
    std::vector<float> initial_distances_;  // Expected distances between consecutive points
    mutable std::mt19937 rng_;
};

}// namespace StateEstimation

#endif// WHISKERTOOLBOX_MASK_PARTICLE_FILTER_HPP
