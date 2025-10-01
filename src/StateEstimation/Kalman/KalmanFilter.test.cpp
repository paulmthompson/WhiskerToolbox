#include <catch2/catch_test_macros.hpp>
#include <Eigen/Dense>
#include <iostream>

#include "Kalman/KalmanFilter.hpp"
#include "Tracker.hpp"
#include "Features/IFeatureExtractor.hpp"

// --- Test-Specific Mocks and Implementations ---

// A simple Line2D struct for this test, mirroring what CoreGeometry might provide.
struct TestLine2D {
    StateEstimation::EntityID id;
    Eigen::Vector2d p1;
    Eigen::Vector2d p2;

    Eigen::Vector2d centroid() const {
        return (p1 + p2) / 2.0;
    }
};

// A concrete feature extractor for TestLine2D centroids.
class LineCentroidExtractor : public StateEstimation::IFeatureExtractor<TestLine2D> {
public:
    Eigen::VectorXd getFilterFeatures(const TestLine2D& data) const override {
        Eigen::Vector2d c = data.centroid();
        Eigen::VectorXd features(2);
        features << c.x(), c.y();
        return features;
    }

    StateEstimation::FeatureCache getAllFeatures(const TestLine2D& data) const override {
        StateEstimation::FeatureCache cache;
        cache[getFilterFeatureName()] = getFilterFeatures(data);
        return cache;
    }

    std::string getFilterFeatureName() const override {
        return "kalman_features";
    }

    StateEstimation::FilterState getInitialState(const TestLine2D& data) const override {
        Eigen::VectorXd initialState(4);
        Eigen::Vector2d centroid = data.centroid();
        initialState << centroid.x(), centroid.y(), 0, 0; // Position from centroid, velocity is zero

        Eigen::MatrixXd p(4, 4);
        p.setIdentity();
        p *= 100.0; // High initial uncertainty

        return {initialState, p};
    }

    std::unique_ptr<IFeatureExtractor<TestLine2D>> clone() const override {
        return std::make_unique<LineCentroidExtractor>(*this);
    }
};


TEST_CASE("StateEstimation - KalmanFilter - tracking and smoothing", "[KalmanFilter]") {
    using namespace StateEstimation;

    // 1. --- SETUP ---

    // Define a constant velocity Kalman Filter
    double dt = 1.0;
    Eigen::MatrixXd F(4, 4); // State Transition Matrix
    F << 1, 0, dt, 0,
         0, 1, 0, dt,
         0, 0, 1, 0,
         0, 0, 0, 1;

    Eigen::MatrixXd H(2, 4); // Measurement Matrix
    H << 1, 0, 0, 0,
         0, 1, 0, 0;

    Eigen::MatrixXd Q(4, 4); // Process Noise Covariance
    Q.setIdentity();
    Q *= 0.1;

    Eigen::MatrixXd R(2, 2); // Measurement Noise Covariance
    R.setIdentity();
    R *= 5.0;

    auto kalman_filter = std::make_unique<KalmanFilter>(F, H, Q, R);
    auto feature_extractor = std::make_unique<LineCentroidExtractor>();

    // We run in smoothing-only mode, so no assigner is needed.
    Tracker<TestLine2D> tracker(std::move(kalman_filter), std::move(feature_extractor), nullptr);

    // --- Generate Artificial Data ---
    Tracker<TestLine2D>::DataSource data_source;
    Tracker<TestLine2D>::GroupMap groups;
    Tracker<TestLine2D>::GroundTruthMap ground_truth;
    
    GroupId group1 = 1;
    GroupId group2 = 2;

    for (int i = 0; i <= 10; ++i) {
        // Line 1 moves from (10,10) to (60,60)
        TestLine2D line1 = { (EntityID)i, {10.0 + i * 5.0, 10.0 + i * 5.0}, {10.0 + i * 5.0, 10.0 + i * 5.0} };
        // Line 2 moves from (100,50) to (50,50)
        TestLine2D line2 = { (EntityID)(i + 100), {100.0 - i * 5.0, 50.0}, {100.0 - i * 5.0, 50.0} };
        
        data_source[i] = {line1, line2};
    }

    // Set ground truth at start and end frames
    ground_truth[0] = {{group1, 0}, {group2, 100}};
    ground_truth[10] = {{group1, 10}, {group2, 110}};
    
    // Populate the initial groups map
    groups[group1] = {0, 10};
    groups[group2] = {100, 110};


    // 2. --- EXECUTION ---
    SmoothedResults results = tracker.process(data_source, groups, ground_truth, 0, 10);

    // 3. --- ASSERTIONS ---
    REQUIRE(results.count(group1) == 1);
    REQUIRE(results.count(group2) == 1);

    // We have 11 frames of data (0-10 inclusive) for the forward pass, which get smoothed.
    REQUIRE(results.at(group1).size() == 11);
    REQUIRE(results.at(group2).size() == 11);

    // Check if the smoothed results are reasonable.
    // The smoothed estimate at the midpoint should be close to the true position.
    const auto& smoothed_g1 = results.at(group1);
    const auto& smoothed_g2 = results.at(group2);
    
    // True position at frame 5 for line 1 is (10 + 5*5, 10 + 5*5) = (35, 35)
    REQUIRE(std::abs(smoothed_g1[5].state_mean(0) - 35.0) < 5.0);
    REQUIRE(std::abs(smoothed_g1[5].state_mean(1) - 35.0) < 5.0);
    
    // True position at frame 5 for line 2 is (100 - 5*5, 50) = (75, 50)
    REQUIRE(std::abs(smoothed_g2[5].state_mean(0) - 75.0) < 5.0);
    REQUIRE(std::abs(smoothed_g2[5].state_mean(1) - 50.0) < 5.0);

    // Check velocity estimates. Group 1 should be moving (+5, +5)
    REQUIRE(smoothed_g1[5].state_mean(2) > 4.0); // vx
    REQUIRE(smoothed_g1[5].state_mean(3) > 4.0); // vy

    // Group 2 should be moving (-5, 0)
    REQUIRE(smoothed_g2[5].state_mean(2) < -4.0); // vx
    REQUIRE(std::abs(smoothed_g2[5].state_mean(3)) < 1.0); // vy
}

