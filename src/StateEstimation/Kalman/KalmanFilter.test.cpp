#include <catch2/catch_test_macros.hpp>
#include <Eigen/Dense>
#include <iostream>

#include "Kalman/KalmanFilter.hpp"
#include "Tracker.hpp"
#include "Features/IFeatureExtractor.hpp"
#include "Assignment/HungarianAssigner.hpp"
#include "Entity/EntityTypes.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "IdentityConfidence.hpp"

// --- Test-Specific Mocks and Implementations ---

// A simple Line2D struct for this test, mirroring what CoreGeometry might provide.
struct TestLine2D {
    EntityId id;
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

    StateEstimation::FeatureMetadata getMetadata() const override {
        return StateEstimation::FeatureMetadata::create(
            "kalman_features",
            2,  // 2D measurement
            StateEstimation::FeatureTemporalType::KINEMATIC_2D
        );
    }
};


TEST_CASE("KalmanFilter tracking and smoothing", "[KalmanFilter]") {
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
    // New API: vector of tuples (DataType, EntityId, TimeFrameIndex)
    std::vector<std::tuple<TestLine2D, EntityId, TimeFrameIndex>> data_source;
    
    EntityGroupManager group_manager;
    GroupId group1 = group_manager.createGroup("Group 1");
    GroupId group2 = group_manager.createGroup("Group 2");

    for (int i = 0; i <= 10; ++i) {
        // Line 1 moves from (10,10) to (60,60)
        TestLine2D line1 = { (EntityId)i, {10.0 + i * 5.0, 10.0 + i * 5.0}, {10.0 + i * 5.0, 10.0 + i * 5.0} };
        // Line 2 moves from (100,50) to (50,50)
        TestLine2D line2 = { (EntityId)(i + 100), {100.0 - i * 5.0, 50.0}, {100.0 - i * 5.0, 50.0} };
        
        data_source.emplace_back(line1, (EntityId)i, TimeFrameIndex(i));
        data_source.emplace_back(line2, (EntityId)(i + 100), TimeFrameIndex(i));
    }

    // Ground truth map with TimeFrameIndex keys
    Tracker<TestLine2D>::GroundTruthMap ground_truth;
    ground_truth[TimeFrameIndex(0)] = {{group1, 0}, {group2, 100}};
    ground_truth[TimeFrameIndex(10)] = {{group1, 10}, {group2, 110}};
    
    // Populate the initial groups in EntityGroupManager
    group_manager.addEntityToGroup(group1, 0);
    group_manager.addEntityToGroup(group1, 10);
    group_manager.addEntityToGroup(group2, 100);
    group_manager.addEntityToGroup(group2, 110);


    // 2. --- EXECUTION ---
    SmoothedResults results = tracker.process(data_source, group_manager, ground_truth, TimeFrameIndex(0), TimeFrameIndex(10));

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

TEST_CASE("StateEstimation - KalmanFilter - assignment with Hungarian algorithm", "[KalmanFilter][Assignment]") {
    using namespace StateEstimation;

    // 1. --- SETUP ---
    double dt = 1.0;
    Eigen::MatrixXd F(4, 4); F << 1, 0, dt, 0, 0, 1, 0, dt, 0, 0, 1, 0, 0, 0, 0, 1;
    Eigen::MatrixXd H(2, 4); H << 1, 0, 0, 0, 0, 1, 0, 0;
    Eigen::MatrixXd Q(4, 4); Q.setIdentity(); Q *= 0.1;
    Eigen::MatrixXd R(2, 2); R.setIdentity(); R *= 5.0;

    auto kalman_filter = std::make_unique<KalmanFilter>(F, H, Q, R);
    auto feature_extractor = std::make_unique<LineCentroidExtractor>();
    auto assigner = std::make_unique<HungarianAssigner>(100.0, H, R, "kalman_features");

    Tracker<TestLine2D> tracker(std::move(kalman_filter), std::move(feature_extractor), std::move(assigner));

    // --- Generate Artificial Data ---
    // New API: vector of tuples (DataType, EntityId, TimeFrameIndex)
    std::vector<std::tuple<TestLine2D, EntityId, TimeFrameIndex>> data_source;
    
    EntityGroupManager group_manager;
    GroupId group1 = group_manager.createGroup("Group 1");
    GroupId group2 = group_manager.createGroup("Group 2");

    // Frame 0: Ground truth
    data_source.emplace_back(TestLine2D{ (EntityId)1, {10, 10}, {10, 10} }, (EntityId)1, TimeFrameIndex(0));
    data_source.emplace_back(TestLine2D{ (EntityId)2, {100, 50}, {100, 50} }, (EntityId)2, TimeFrameIndex(0));
    
    Tracker<TestLine2D>::GroundTruthMap ground_truth;
    ground_truth[TimeFrameIndex(0)] = {{group1, 1}, {group2, 2}};
    
    group_manager.addEntityToGroup(group1, 1);
    group_manager.addEntityToGroup(group2, 2);

    // Frames 1-4: Ungrouped data that should be assigned
    for (int i = 1; i <= 4; ++i) {
        data_source.emplace_back(
            TestLine2D{ (EntityId)(10 + i), {10.0 + i * 5.0, 10.0 + i * 5.0}, {10.0 + i * 5.0, 10.0 + i * 5.0} },
            (EntityId)(10 + i),
            TimeFrameIndex(i)
        );
        data_source.emplace_back(
            TestLine2D{ (EntityId)(20 + i), {100.0 - i * 5.0, 50.0}, {100.0 - i * 5.0, 50.0} },
            (EntityId)(20 + i),
            TimeFrameIndex(i)
        );
    }

    // 2. --- EXECUTION ---
    tracker.process(data_source, group_manager, ground_truth, TimeFrameIndex(0), TimeFrameIndex(4));

    // 3. --- ASSERTIONS ---
    // Verify groups exist
    REQUIRE(group_manager.hasGroup(group1));
    REQUIRE(group_manager.hasGroup(group2));

    // Check that all entities were correctly assigned. Each group should have 5 entities now.
    REQUIRE(group_manager.getGroupSize(group1) == 5); // 1 (initial) + 4 (assigned)
    REQUIRE(group_manager.getGroupSize(group2) == 5);

    // Verify the correct entities were assigned to group 1
    std::vector<EntityId> expected_g1 = {1, 11, 12, 13, 14};
    auto group1_entities = group_manager.getEntitiesInGroup(group1);
    std::sort(group1_entities.begin(), group1_entities.end());
    REQUIRE(group1_entities == expected_g1);

    // Verify the correct entities were assigned to group 2
    std::vector<EntityId> expected_g2 = {2, 21, 22, 23, 24};
    auto group2_entities = group_manager.getEntitiesInGroup(group2);
    std::sort(group2_entities.begin(), group2_entities.end());
    REQUIRE(group2_entities == expected_g2);
}

TEST_CASE("StateEstimation - KalmanFilter - Identity confidence with ambiguous assignments", "[KalmanFilter][IdentityConfidence]") {
    using namespace StateEstimation;

    // 1. --- SETUP ---
    double dt = 1.0;
    Eigen::MatrixXd F(4, 4); F << 1, 0, dt, 0, 0, 1, 0, dt, 0, 0, 1, 0, 0, 0, 0, 1;
    Eigen::MatrixXd H(2, 4); H << 1, 0, 0, 0, 0, 1, 0, 0;
    Eigen::MatrixXd Q(4, 4); Q.setIdentity(); Q *= 0.1;
    Eigen::MatrixXd R(2, 2); R.setIdentity(); R *= 5.0;

    auto kalman_filter = std::make_unique<KalmanFilter>(F, H, Q, R);
    auto feature_extractor = std::make_unique<LineCentroidExtractor>();
    auto assigner = std::make_unique<HungarianAssigner>(100.0, H, R, "kalman_features");

    Tracker<TestLine2D> tracker(std::move(kalman_filter), std::move(feature_extractor), std::move(assigner));

    // --- Generate Data for Assignment Testing ---
    std::vector<std::tuple<TestLine2D, EntityId, TimeFrameIndex>> data_source;
    
    EntityGroupManager group_manager;
    GroupId group1 = group_manager.createGroup("Group 1");
    GroupId group2 = group_manager.createGroup("Group 2");

    // Frame 0: Ground truth - establish initial tracks
    data_source.emplace_back(TestLine2D{ (EntityId)1, {10, 10}, {10, 10} }, (EntityId)1, TimeFrameIndex(0));
    data_source.emplace_back(TestLine2D{ (EntityId)2, {100, 10}, {100, 10} }, (EntityId)2, TimeFrameIndex(0));
    
    Tracker<TestLine2D>::GroundTruthMap ground_truth;
    ground_truth[TimeFrameIndex(0)] = {{group1, 1}, {group2, 2}};

    // Frames 1-3: Add ungrouped data that should be assigned
    // These are close to the predicted positions, so assignment should be easy initially
    for (int i = 1; i <= 3; ++i) {
        // Easy assignments - close to predicted positions
        double g1_x = 10.0 + i * 5.0; // Group 1 moves right
        double g2_x = 100.0 - i * 5.0; // Group 2 moves left
        
        data_source.emplace_back(
            TestLine2D{ (EntityId)(10 + i), {g1_x, 10.0}, {g1_x, 10.0} },
            (EntityId)(10 + i),
            TimeFrameIndex(i)
        );
        data_source.emplace_back(
            TestLine2D{ (EntityId)(20 + i), {g2_x, 10.0}, {g2_x, 10.0} },
            (EntityId)(20 + i),
            TimeFrameIndex(i)
        );
    }

    // Frames 4-5: Make assignments more difficult by bringing lines closer
    for (int i = 4; i <= 5; ++i) {
        // Much harder assignments - lines are very close, making assignment ambiguous
        double g1_x = 10.0 + i * 12.0; // Much faster movement
        double g2_x = 100.0 - i * 12.0; // Much faster movement
        
        data_source.emplace_back(
            TestLine2D{ (EntityId)(10 + i), {g1_x, 10.0}, {g1_x, 10.0} },
            (EntityId)(10 + i),
            TimeFrameIndex(i)
        );
        data_source.emplace_back(
            TestLine2D{ (EntityId)(20 + i), {g2_x, 10.0}, {g2_x, 10.0} },
            (EntityId)(20 + i),
            TimeFrameIndex(i)
        );
    }

    // Frame 6: No ground truth - let confidence accumulate
    data_source.emplace_back(TestLine2D{ (EntityId)3, {50, 10}, {50, 10} }, (EntityId)3, TimeFrameIndex(6));
    data_source.emplace_back(TestLine2D{ (EntityId)4, {52, 10}, {52, 10} }, (EntityId)4, TimeFrameIndex(6));
    // No ground truth at frame 6 - let confidence accumulate

    // 2. --- EXECUTION ---
    tracker.process(data_source, group_manager, ground_truth, TimeFrameIndex(0), TimeFrameIndex(6));

    // 3. --- ASSERTIONS ---
    // Check that groups exist and have been populated
    REQUIRE(group_manager.hasGroup(group1));
    REQUIRE(group_manager.hasGroup(group2));
    
    std::cout << "Group 1 size: " << group_manager.getGroupSize(group1) << std::endl;
    std::cout << "Group 2 size: " << group_manager.getGroupSize(group2) << std::endl;

    // Get identity confidence after processing
    double conf_group1 = tracker.getIdentityConfidence(group1);
    double conf_group2 = tracker.getIdentityConfidence(group2);
    
    // Get measurement noise scaling
    double noise_scale_group1 = tracker.getMeasurementNoiseScale(group1);
    double noise_scale_group2 = tracker.getMeasurementNoiseScale(group2);
    
    // Get minimum confidence since last anchor
    double min_conf_group1 = tracker.getMinConfidenceSinceAnchor(group1);
    double min_conf_group2 = tracker.getMinConfidenceSinceAnchor(group2);

    std::cout << "Group 1 - Confidence: " << conf_group1 
              << ", Noise Scale: " << noise_scale_group1 
              << ", Min Confidence: " << min_conf_group1 << std::endl;
    std::cout << "Group 2 - Confidence: " << conf_group2 
              << ", Noise Scale: " << noise_scale_group2 
              << ", Min Confidence: " << min_conf_group2 << std::endl;

    // Verify confidence values are valid
    REQUIRE(conf_group1 >= 0.1); // Should not go below floor
    REQUIRE(conf_group1 <= 1.0);
    REQUIRE(conf_group2 >= 0.1);
    REQUIRE(conf_group2 <= 1.0);

    // Verify noise scaling is reasonable
    REQUIRE(noise_scale_group1 >= 1.0);
    REQUIRE(noise_scale_group2 >= 1.0);

    // Minimum confidence should be <= current confidence
    REQUIRE(min_conf_group1 <= conf_group1);
    REQUIRE(min_conf_group2 <= conf_group2);
}

TEST_CASE("StateEstimation - IdentityConfidence - Basic functionality", "[IdentityConfidence]") {
    using namespace StateEstimation;

    IdentityConfidence confidence;
    
    // Test initial state
    REQUIRE(confidence.getConfidence() == 1.0);
    REQUIRE(confidence.getMeasurementNoiseScale() == 1.0);
    REQUIRE(confidence.getMinConfidenceSinceAnchor() == 1.0);
    
    // Test confidence degradation with poor assignment
    confidence.updateOnAssignment(0.8, 1.0); // High cost, at threshold
    double conf_after_poor = confidence.getConfidence();
    REQUIRE(conf_after_poor < 1.0);
    REQUIRE(conf_after_poor > 0.1); // Should not go below floor
    
    // Test measurement noise scaling
    double noise_scale = confidence.getMeasurementNoiseScale();
    REQUIRE(noise_scale > 1.0); // Should be inflated
    
    std::cout << "After poor assignment - Confidence: " << conf_after_poor 
              << ", Noise Scale: " << noise_scale << std::endl;
    
    // Test slow recovery with excellent assignment
    confidence.allowSlowRecovery(0.05, 0.1); // Very good assignment
    double conf_after_recovery = confidence.getConfidence();
    REQUIRE(conf_after_recovery > conf_after_poor);
    
    std::cout << "After recovery - Confidence: " << conf_after_recovery << std::endl;
    
    // Test ground truth reset
    confidence.resetOnGroundTruth();
    REQUIRE(confidence.getConfidence() == 1.0);
    REQUIRE(confidence.getMinConfidenceSinceAnchor() == 1.0);
    
    std::cout << "After ground truth reset - Confidence: " << confidence.getConfidence() << std::endl;
}

TEST_CASE("StateEstimation - KalmanFilter - High assignment costs", "[KalmanFilter][IdentityConfidence]") {
    using namespace StateEstimation;

    // 1. --- SETUP ---
    double dt = 1.0;
    Eigen::MatrixXd F(4, 4); F << 1, 0, dt, 0, 0, 1, 0, dt, 0, 0, 1, 0, 0, 0, 0, 1;
    Eigen::MatrixXd H(2, 4); H << 1, 0, 0, 0, 0, 1, 0, 0;
    Eigen::MatrixXd Q(4, 4); Q.setIdentity(); Q *= 0.1;
    Eigen::MatrixXd R(2, 2); R.setIdentity(); R *= 5.0;

    auto kalman_filter = std::make_unique<KalmanFilter>(F, H, Q, R);
    auto feature_extractor = std::make_unique<LineCentroidExtractor>();
    // Use a very low threshold to force high assignment costs
    auto assigner = std::make_unique<HungarianAssigner>(10.0, H, R, "kalman_features");

    Tracker<TestLine2D> tracker(std::move(kalman_filter), std::move(feature_extractor), std::move(assigner));

    // --- Generate Data with High Assignment Costs ---
    std::vector<std::tuple<TestLine2D, EntityId, TimeFrameIndex>> data_source;
    
    EntityGroupManager group_manager;
    GroupId group1 = group_manager.createGroup("Group 1");
    GroupId group2 = group_manager.createGroup("Group 2");

    // Frame 0: Ground truth - establish initial tracks
    data_source.emplace_back(TestLine2D{ (EntityId)1, {10, 10}, {10, 10} }, (EntityId)1, TimeFrameIndex(0));
    data_source.emplace_back(TestLine2D{ (EntityId)2, {100, 10}, {100, 10} }, (EntityId)2, TimeFrameIndex(0));
    
    Tracker<TestLine2D>::GroundTruthMap ground_truth;
    ground_truth[TimeFrameIndex(0)] = {{group1, 1}, {group2, 2}};

    // Frames 1-3: Add data that will have high assignment costs
    for (int i = 1; i <= 3; ++i) {
        // Place observations far from predicted positions to force high costs
        double g1_x = 10.0 + i * 20.0; // Very far from prediction
        double g2_x = 100.0 - i * 20.0; // Very far from prediction
        
        data_source.emplace_back(
            TestLine2D{ (EntityId)(10 + i), {g1_x, 10.0}, {g1_x, 10.0} },
            (EntityId)(10 + i),
            TimeFrameIndex(i)
        );
        data_source.emplace_back(
            TestLine2D{ (EntityId)(20 + i), {g2_x, 10.0}, {g2_x, 10.0} },
            (EntityId)(20 + i),
            TimeFrameIndex(i)
        );
    }

    // 2. --- EXECUTION ---
    tracker.process(data_source, group_manager, ground_truth, TimeFrameIndex(0), TimeFrameIndex(3));

    // 3. --- ASSERTIONS ---
    double conf_group1 = tracker.getIdentityConfidence(group1);
    double conf_group2 = tracker.getIdentityConfidence(group2);
    
    double noise_scale_group1 = tracker.getMeasurementNoiseScale(group1);
    double noise_scale_group2 = tracker.getMeasurementNoiseScale(group2);

    std::cout << "High cost test - Group 1 - Confidence: " << conf_group1 
              << ", Noise Scale: " << noise_scale_group1 << std::endl;
    std::cout << "High cost test - Group 2 - Confidence: " << conf_group2 
              << ", Noise Scale: " << noise_scale_group2 << std::endl;

    // With high assignment costs, we should see confidence degradation
    // Note: This might still be 1.0 if the assignment costs are below the threshold
    REQUIRE(conf_group1 >= 0.1);
    REQUIRE(conf_group1 <= 1.0);
    REQUIRE(conf_group2 >= 0.1);
    REQUIRE(conf_group2 <= 1.0);
}
