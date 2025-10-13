#include "MaskParticleFilter.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <iostream>

using namespace StateEstimation;

// ============================================================================
// Helper Functions for Synthetic Data Generation
// ============================================================================

/**
 * @brief Generate a circular mask with given center and radius
 */
Mask2D generateCircleMask(Point2D<uint32_t> const& center, float radius) {
    Mask2D mask;
    uint32_t r_sq = static_cast<uint32_t>(radius * radius);
    
    uint32_t min_x = (center.x > static_cast<uint32_t>(radius)) ? center.x - static_cast<uint32_t>(radius) : 0;
    uint32_t max_x = center.x + static_cast<uint32_t>(radius);
    uint32_t min_y = (center.y > static_cast<uint32_t>(radius)) ? center.y - static_cast<uint32_t>(radius) : 0;
    uint32_t max_y = center.y + static_cast<uint32_t>(radius);
    
    for (uint32_t x = min_x; x <= max_x; ++x) {
        for (uint32_t y = min_y; y <= max_y; ++y) {
            int32_t dx = static_cast<int32_t>(x) - static_cast<int32_t>(center.x);
            int32_t dy = static_cast<int32_t>(y) - static_cast<int32_t>(center.y);
            uint32_t dist_sq = static_cast<uint32_t>(dx * dx + dy * dy);
            
            if (dist_sq <= r_sq) {
                mask.push_back({x, y});
            }
        }
    }
    
    return mask;
}

/**
 * @brief Generate a line mask (vertical or horizontal)
 */
Mask2D generateLineMask(Point2D<uint32_t> start, Point2D<uint32_t> end, uint32_t thickness = 1) {
    Mask2D mask;
    
    // Bresenham's line algorithm with thickness
    int32_t dx = static_cast<int32_t>(end.x) - static_cast<int32_t>(start.x);
    int32_t dy = static_cast<int32_t>(end.y) - static_cast<int32_t>(start.y);
    
    int32_t steps = std::max(std::abs(dx), std::abs(dy));
    
    if (steps == 0) {
        mask.push_back(start);
        return mask;
    }
    
    float x_inc = static_cast<float>(dx) / static_cast<float>(steps);
    float y_inc = static_cast<float>(dy) / static_cast<float>(steps);
    
    float x = static_cast<float>(start.x);
    float y = static_cast<float>(start.y);
    
    for (int32_t i = 0; i <= steps; ++i) {
        Point2D<uint32_t> center{
            static_cast<uint32_t>(std::round(x)),
            static_cast<uint32_t>(std::round(y))
        };
        
        // Add thickness around the line
        for (uint32_t dx = 0; dx < thickness; ++dx) {
            for (uint32_t dy = 0; dy < thickness; ++dy) {
                mask.push_back({center.x + dx, center.y + dy});
            }
        }
        
        x += x_inc;
        y += y_inc;
    }
    
    return mask;
}

/**
 * @brief Generate a moving point trajectory (linear motion)
 */
std::vector<Point2D<uint32_t>> generateLinearTrajectory(
    Point2D<uint32_t> start,
    Point2D<uint32_t> end,
    size_t num_frames) {
    
    std::vector<Point2D<uint32_t>> trajectory;
    trajectory.reserve(num_frames);
    
    if (num_frames <= 1) {
        trajectory.push_back(start);
        return trajectory;
    }
    
    for (size_t i = 0; i < num_frames; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(num_frames - 1);
        
        uint32_t x = static_cast<uint32_t>(
            (1.0f - t) * static_cast<float>(start.x) + t * static_cast<float>(end.x));
        uint32_t y = static_cast<uint32_t>(
            (1.0f - t) * static_cast<float>(start.y) + t * static_cast<float>(end.y));
        
        trajectory.push_back({x, y});
    }
    
    return trajectory;
}

// ============================================================================
// Single Point Tracking Tests
// ============================================================================

TEST_CASE("MaskPointTracker: Track straight line", "[MaskPointTracker]") {
    // Create a simple scenario: point moves in a straight line
    Point2D<uint32_t> start{100, 100};
    Point2D<uint32_t> end{200, 100};
    size_t num_frames = 10;
    
    auto ground_truth = generateLinearTrajectory(start, end, num_frames);
    
    // Generate masks: circles around each ground truth point
    std::vector<Mask2D> masks;
    for (auto const& gt_point : ground_truth) {
        masks.push_back(generateCircleMask(gt_point, 20.0f));
    }
    
    // Track with particle filter
    MaskPointTracker tracker(500, 15.0f, 0.05f);
    auto tracked_points = tracker.track(start, end, masks);
    
    // Verify we got results for all frames
    REQUIRE(tracked_points.size() == num_frames);
    
    // Verify start and end are close to ground truth
    // Note: Particle filter is stochastic, so we use larger tolerance
    REQUIRE_THAT(static_cast<float>(tracked_points.front().x), Catch::Matchers::WithinAbs(start.x, 15.0f));
    REQUIRE_THAT(static_cast<float>(tracked_points.front().y), Catch::Matchers::WithinAbs(start.y, 15.0f));
    REQUIRE_THAT(static_cast<float>(tracked_points.back().x), Catch::Matchers::WithinAbs(end.x, 15.0f));
    REQUIRE_THAT(static_cast<float>(tracked_points.back().y), Catch::Matchers::WithinAbs(end.y, 15.0f));
    
    // Verify tracking is generally accurate (within reasonable error)
    float total_error = 0.0f;
    for (size_t i = 0; i < num_frames; ++i) {
        total_error += pointDistance(tracked_points[i], ground_truth[i]);
    }
    float avg_error = total_error / static_cast<float>(num_frames);
    
    std::cout << "Average tracking error: " << avg_error << " pixels\n";
    // Particle filter is stochastic - allow for reasonable variance
    REQUIRE(avg_error < 15.0f);
}

TEST_CASE("MaskPointTracker: Track with gaps", "[MaskPointTracker]") {
    // Test tracking when mask data has gaps
    Point2D<uint32_t> start{50, 50};
    Point2D<uint32_t> end{150, 150};
    size_t num_frames = 15;
    
    auto ground_truth = generateLinearTrajectory(start, end, num_frames);
    
    // Generate masks with some frames having larger uncertainty
    std::vector<Mask2D> masks;
    for (size_t i = 0; i < num_frames; ++i) {
        // Every 5th frame has a larger mask (more uncertainty)
        float radius = (i % 5 == 0) ? 30.0f : 15.0f;
        masks.push_back(generateCircleMask(ground_truth[i], radius));
    }
    
    MaskPointTracker tracker(1000, 20.0f, 0.1f);
    auto tracked_points = tracker.track(start, end, masks);
    
    REQUIRE(tracked_points.size() == num_frames);
    
    // Should still track reasonably well despite uncertainty
    float total_error = 0.0f;
    for (size_t i = 0; i < num_frames; ++i) {
        total_error += pointDistance(tracked_points[i], ground_truth[i]);
    }
    float avg_error = total_error / static_cast<float>(num_frames);
    
    std::cout << "Average tracking error with gaps: " << avg_error << " pixels\n";
    REQUIRE(avg_error < 15.0f);
}

TEST_CASE("MaskPointTracker: Track diagonal motion", "[MaskPointTracker]") {
    // Test diagonal motion
    Point2D<uint32_t> start{100, 100};
    Point2D<uint32_t> end{200, 200};
    size_t num_frames = 20;
    
    auto ground_truth = generateLinearTrajectory(start, end, num_frames);
    
    std::vector<Mask2D> masks;
    for (auto const& gt_point : ground_truth) {
        masks.push_back(generateCircleMask(gt_point, 15.0f));
    }
    
    MaskPointTracker tracker(500, 12.0f, 0.05f);
    auto tracked_points = tracker.track(start, end, masks);
    
    REQUIRE(tracked_points.size() == num_frames);
    
    // Check that motion is generally diagonal
    float dx_total = static_cast<float>(tracked_points.back().x) - static_cast<float>(tracked_points.front().x);
    float dy_total = static_cast<float>(tracked_points.back().y) - static_cast<float>(tracked_points.front().y);
    
    // For diagonal motion, dx and dy should be similar
    float ratio = std::abs(dx_total / dy_total);
    REQUIRE_THAT(ratio, Catch::Matchers::WithinAbs(1.0f, 0.3f));
}

TEST_CASE("MaskPointTracker: Empty mask handling", "[MaskPointTracker]") {
    // Test edge case: empty masks
    Point2D<uint32_t> start{100, 100};
    Point2D<uint32_t> end{200, 100};
    
    std::vector<Mask2D> empty_masks;
    
    MaskPointTracker tracker(100, 10.0f, 0.1f);
    auto tracked_points = tracker.track(start, end, empty_masks);
    
    REQUIRE(tracked_points.empty());
}

TEST_CASE("MaskPointTracker: Single frame tracking", "[MaskPointTracker]") {
    // Test with just one frame
    Point2D<uint32_t> point{100, 100};
    
    std::vector<Mask2D> masks;
    masks.push_back(generateCircleMask(point, 10.0f));
    
    MaskPointTracker tracker(100, 10.0f, 0.1f);
    auto tracked_points = tracker.track(point, point, masks);
    
    REQUIRE(tracked_points.size() == 1u);
    // Particle filter is stochastic - use larger tolerance
    REQUIRE_THAT(static_cast<float>(tracked_points[0].x), Catch::Matchers::WithinAbs(point.x, 15.0f));
    REQUIRE_THAT(static_cast<float>(tracked_points[0].y), Catch::Matchers::WithinAbs(point.y, 15.0f));
}

// ============================================================================
// Multi-Point Correlated Tracking Tests
// ============================================================================

TEST_CASE("CorrelatedMaskPointTracker: Track two points on line", "[CorrelatedMaskPointTracker]") {
    // Track two points that maintain constant spacing
    Point2D<uint32_t> start1{100, 100};
    Point2D<uint32_t> start2{150, 100};
    Point2D<uint32_t> end1{100, 200};
    Point2D<uint32_t> end2{150, 200};
    
    size_t num_frames = 15;
    
    auto traj1 = generateLinearTrajectory(start1, end1, num_frames);
    auto traj2 = generateLinearTrajectory(start2, end2, num_frames);
    
    // Generate masks containing both trajectories
    std::vector<Mask2D> masks;
    for (size_t i = 0; i < num_frames; ++i) {
        Mask2D mask = generateCircleMask(traj1[i], 20.0f);
        auto mask2 = generateCircleMask(traj2[i], 20.0f);
        mask.insert(mask.end(), mask2.begin(), mask2.end());
        masks.push_back(mask);
    }
    
    std::vector<Point2D<uint32_t>> start_points = {start1, start2};
    std::vector<Point2D<uint32_t>> end_points = {end1, end2};
    
    CorrelatedMaskPointTracker tracker(500, 15.0f, 0.8f);  // High correlation
    auto tracked_states = tracker.track(start_points, end_points, masks);
    
    REQUIRE(tracked_states.size() == num_frames);
    REQUIRE(tracked_states[0].points.size() == 2u);
    
    // Check that spacing between points is maintained
    float initial_spacing = pointDistance(start1, start2);
    
    float avg_spacing = 0.0f;
    for (auto const& state : tracked_states) {
        avg_spacing += pointDistance(state.points[0], state.points[1]);
    }
    avg_spacing /= static_cast<float>(num_frames);
    
    std::cout << "Initial spacing: " << initial_spacing << "\n";
    std::cout << "Average tracked spacing: " << avg_spacing << "\n";
    
    // Spacing should be maintained within reasonable tolerance
    REQUIRE_THAT(avg_spacing, Catch::Matchers::WithinAbs(initial_spacing, 10.0f));
}

TEST_CASE("CorrelatedMaskPointTracker: Independent vs correlated", "[CorrelatedMaskPointTracker]") {
    // Compare independent (correlation=0) vs correlated (correlation=1) tracking
    Point2D<uint32_t> start1{100, 100};
    Point2D<uint32_t> start2{150, 100};
    Point2D<uint32_t> end1{100, 200};
    Point2D<uint32_t> end2{150, 200};
    
    size_t num_frames = 10;
    
    auto traj1 = generateLinearTrajectory(start1, end1, num_frames);
    auto traj2 = generateLinearTrajectory(start2, end2, num_frames);
    
    std::vector<Mask2D> masks;
    for (size_t i = 0; i < num_frames; ++i) {
        Mask2D mask = generateCircleMask(traj1[i], 25.0f);
        auto mask2 = generateCircleMask(traj2[i], 25.0f);
        mask.insert(mask.end(), mask2.begin(), mask2.end());
        masks.push_back(mask);
    }
    
    std::vector<Point2D<uint32_t>> start_points = {start1, start2};
    std::vector<Point2D<uint32_t>> end_points = {end1, end2};
    
    // Track with low correlation
    CorrelatedMaskPointTracker tracker_independent(500, 15.0f, 0.0f);
    auto states_independent = tracker_independent.track(start_points, end_points, masks);
    
    // Track with high correlation
    CorrelatedMaskPointTracker tracker_correlated(500, 15.0f, 0.9f);
    auto states_correlated = tracker_correlated.track(start_points, end_points, masks);
    
    // Both should complete successfully
    REQUIRE(states_independent.size() == num_frames);
    REQUIRE(states_correlated.size() == num_frames);
    
    // Measure spacing variance for each
    float initial_spacing = pointDistance(start1, start2);
    
    float variance_independent = 0.0f;
    float variance_correlated = 0.0f;
    
    for (size_t i = 0; i < num_frames; ++i) {
        float spacing_indep = pointDistance(states_independent[i].points[0], states_independent[i].points[1]);
        float spacing_corr = pointDistance(states_correlated[i].points[0], states_correlated[i].points[1]);
        
        variance_independent += std::pow(spacing_indep - initial_spacing, 2);
        variance_correlated += std::pow(spacing_corr - initial_spacing, 2);
    }
    
    variance_independent /= static_cast<float>(num_frames);
    variance_correlated /= static_cast<float>(num_frames);
    
    std::cout << "Spacing variance (independent): " << variance_independent << "\n";
    std::cout << "Spacing variance (correlated): " << variance_correlated << "\n";
    
    // Correlated tracking should have lower variance in spacing
    REQUIRE(variance_correlated < variance_independent);
}

TEST_CASE("CorrelatedMaskPointTracker: Three point tracking", "[CorrelatedMaskPointTracker]") {
    // Track three points in a line
    Point2D<uint32_t> start1{100, 100};
    Point2D<uint32_t> start2{150, 100};
    Point2D<uint32_t> start3{200, 100};
    Point2D<uint32_t> end1{100, 200};
    Point2D<uint32_t> end2{150, 200};
    Point2D<uint32_t> end3{200, 200};
    
    size_t num_frames = 12;
    
    auto traj1 = generateLinearTrajectory(start1, end1, num_frames);
    auto traj2 = generateLinearTrajectory(start2, end2, num_frames);
    auto traj3 = generateLinearTrajectory(start3, end3, num_frames);
    
    std::vector<Mask2D> masks;
    for (size_t i = 0; i < num_frames; ++i) {
        Mask2D mask = generateCircleMask(traj1[i], 20.0f);
        auto mask2 = generateCircleMask(traj2[i], 20.0f);
        auto mask3 = generateCircleMask(traj3[i], 20.0f);
        mask.insert(mask.end(), mask2.begin(), mask2.end());
        mask.insert(mask.end(), mask3.begin(), mask3.end());
        masks.push_back(mask);
    }
    
    std::vector<Point2D<uint32_t>> start_points = {start1, start2, start3};
    std::vector<Point2D<uint32_t>> end_points = {end1, end2, end3};
    
    CorrelatedMaskPointTracker tracker(500, 15.0f, 0.7f);
    auto tracked_states = tracker.track(start_points, end_points, masks);
    
    REQUIRE(tracked_states.size() == num_frames);
    REQUIRE(tracked_states[0].points.size() == 3u);
    
    // All three points should reach their endpoints
    REQUIRE_THAT(static_cast<float>(tracked_states.back().points[0].x), Catch::Matchers::WithinAbs(end1.x, 10.0f));
    REQUIRE_THAT(static_cast<float>(tracked_states.back().points[1].x), Catch::Matchers::WithinAbs(end2.x, 10.0f));
    REQUIRE_THAT(static_cast<float>(tracked_states.back().points[2].x), Catch::Matchers::WithinAbs(end3.x, 10.0f));
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_CASE("MaskUtilities: Point distance", "[MaskUtilities]") {
    Point2D<uint32_t> p1{0, 0};
    Point2D<uint32_t> p2{3, 4};
    
    float dist = pointDistance(p1, p2);
    REQUIRE_THAT(dist, Catch::Matchers::WithinAbs(5.0f, 0.001f));  // 3-4-5 triangle
}

TEST_CASE("MaskUtilities: Find nearest mask pixel", "[MaskUtilities]") {
    Mask2D mask = {{10, 10}, {20, 20}, {30, 30}};
    Point2D<uint32_t> query{15, 15};
    
    auto nearest = findNearestMaskPixel(query, mask);
    
    // Should find (10, 10) or (20, 20) as nearest
    float dist1 = pointDistance(nearest, Point2D<uint32_t>{10, 10});
    float dist2 = pointDistance(nearest, Point2D<uint32_t>{20, 20});
    
    REQUIRE((dist1 == 0.0f || dist2 == 0.0f));
}

TEST_CASE("MaskUtilities: Find nearest in empty mask", "[MaskUtilities]") {
    Mask2D empty_mask;
    Point2D<uint32_t> query{15, 15};
    
    auto nearest = findNearestMaskPixel(query, empty_mask);
    
    // Should return the query point itself
    REQUIRE(nearest.x == query.x);
    REQUIRE(nearest.y == query.y);
}
