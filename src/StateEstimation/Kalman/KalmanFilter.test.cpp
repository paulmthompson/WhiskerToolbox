#include <Eigen/Dense>
#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "Assignment/HungarianAssigner.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityTypes.hpp"
#include "Features/IFeatureExtractor.hpp"
#include "IdentityConfidence.hpp"
#include "Kalman/KalmanFilter.hpp"
#include "MinCostFlowTracker.hpp"
#include "StateEstimator.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "Tracker.hpp"

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
    Eigen::VectorXd getFilterFeatures(TestLine2D const & data) const override {
        Eigen::Vector2d c = data.centroid();
        Eigen::VectorXd features(2);
        features << c.x(), c.y();
        return features;
    }

    StateEstimation::FeatureCache getAllFeatures(TestLine2D const & data) const override {
        StateEstimation::FeatureCache cache;
        cache[getFilterFeatureName()] = getFilterFeatures(data);
        return cache;
    }

    std::string getFilterFeatureName() const override {
        return "kalman_features";
    }

    StateEstimation::FilterState getInitialState(TestLine2D const & data) const override {
        Eigen::VectorXd initialState(4);
        Eigen::Vector2d centroid = data.centroid();
        initialState << centroid.x(), centroid.y(), 0, 0;// Position from centroid, velocity is zero

        Eigen::MatrixXd p(4, 4);
        p.setIdentity();
        p *= 100.0;// High initial uncertainty

        return {initialState, p};
    }

    std::unique_ptr<IFeatureExtractor<TestLine2D>> clone() const override {
        return std::make_unique<LineCentroidExtractor>(*this);
    }

    StateEstimation::FeatureMetadata getMetadata() const override {
        return StateEstimation::FeatureMetadata::create(
                "kalman_features",
                2,// 2D measurement
                StateEstimation::FeatureTemporalType::KINEMATIC_2D);
    }
};


TEST_CASE("StateEstimator - tracking and smoothing", "[StateEstimator][Smoothing]") {
    using namespace StateEstimation;

    // 1. --- SETUP ---

    // Define a constant velocity Kalman Filter
    double dt = 1.0;
    Eigen::MatrixXd F(4, 4);
    F << 1, 0, dt, 0,
         0, 1, 0, dt,
         0, 0, 1, 0,
         0, 0, 0, 1;

    Eigen::MatrixXd H(2, 4);
    H << 1, 0, 0, 0,
         0, 1, 0, 0;

    Eigen::MatrixXd Q(4, 4);
    Q.setIdentity();
    Q *= 0.1;

    Eigen::MatrixXd R(2, 2);
    R.setIdentity();
    R *= 5.0;

    auto kalman_filter = std::make_unique<KalmanFilter>(F, H, Q, R);
    auto feature_extractor = std::make_unique<LineCentroidExtractor>();

    // Use StateEstimator for smoothing already-grouped data
    StateEstimator<TestLine2D> estimator(std::move(kalman_filter), std::move(feature_extractor));

    // --- Generate Artificial Data ---
    std::vector<std::tuple<TestLine2D, EntityId, TimeFrameIndex>> data_source;

    EntityGroupManager group_manager;
    GroupId group1 = group_manager.createGroup("Group 1");
    GroupId group2 = group_manager.createGroup("Group 2");

    for (int i = 0; i <= 10; ++i) {
        // Line 1 moves from (10,10) to (60,60)
        TestLine2D line1 = {(EntityId) i, {10.0 + i * 5.0, 10.0 + i * 5.0}, {10.0 + i * 5.0, 10.0 + i * 5.0}};
        // Line 2 moves from (100,50) to (50,50)
        TestLine2D line2 = {(EntityId) (i + 100), {100.0 - i * 5.0, 50.0}, {100.0 - i * 5.0, 50.0}};

        data_source.emplace_back(line1, (EntityId) i, TimeFrameIndex(i));
        data_source.emplace_back(line2, (EntityId) (i + 100), TimeFrameIndex(i));
        
        // Add all entities to their groups
        group_manager.addEntityToGroup(group1, (EntityId) i);
        group_manager.addEntityToGroup(group2, (EntityId) (i + 100));
    }

    // 2. --- EXECUTION ---
    SmoothedGroupResults results = estimator.smoothGroups(data_source, group_manager, 
                                                          TimeFrameIndex(0), TimeFrameIndex(10));

    // 3. --- ASSERTIONS ---
    REQUIRE(results.count(group1) == 1);
    REQUIRE(results.count(group2) == 1);

    // We have 11 frames of data (0-10 inclusive) for the forward pass, which get smoothed.
    REQUIRE(results.at(group1).size() == 11);
    REQUIRE(results.at(group2).size() == 11);

    // Check if the smoothed results are reasonable.
    // The smoothed estimate at the midpoint should be close to the true position.
    auto const & smoothed_g1 = results.at(group1);
    auto const & smoothed_g2 = results.at(group2);

    // True position at frame 5 for line 1 is (10 + 5*5, 10 + 5*5) = (35, 35)
    REQUIRE(std::abs(smoothed_g1[5].state_mean(0) - 35.0) < 5.0);
    REQUIRE(std::abs(smoothed_g1[5].state_mean(1) - 35.0) < 5.0);

    // True position at frame 5 for line 2 is (100 - 5*5, 50) = (75, 50)
    REQUIRE(std::abs(smoothed_g2[5].state_mean(0) - 75.0) < 5.0);
    REQUIRE(std::abs(smoothed_g2[5].state_mean(1) - 50.0) < 5.0);

    // Check velocity estimates. Group 1 should be moving (+5, +5)
    REQUIRE(smoothed_g1[5].state_mean(2) > 4.0);// vx
    REQUIRE(smoothed_g1[5].state_mean(3) > 4.0);// vy

    // Group 2 should be moving (-5, 0)
    REQUIRE(smoothed_g2[5].state_mean(2) < -4.0);         // vx
    REQUIRE(std::abs(smoothed_g2[5].state_mean(3)) < 1.0);// vy
}

TEST_CASE("StateEstimator smoothing and outlier detection", "[StateEstimator]") {
    using namespace StateEstimation;

    // 1. --- SETUP ---

    // Define a constant velocity Kalman Filter
    double dt = 1.0;
    Eigen::MatrixXd F(4, 4);
    F << 1, 0, dt, 0,
         0, 1, 0, dt,
         0, 0, 1, 0,
         0, 0, 0, 1;

    Eigen::MatrixXd H(2, 4);
    H << 1, 0, 0, 0,
         0, 1, 0, 0;

    Eigen::MatrixXd Q(4, 4);
    Q.setIdentity();
    Q *= 0.1;

    Eigen::MatrixXd R(2, 2);
    R.setIdentity();
    R *= 5.0;

    auto kalman_filter = std::make_unique<KalmanFilter>(F, H, Q, R);
    auto feature_extractor = std::make_unique<LineCentroidExtractor>();

    // Create StateEstimator (separate from MinCostFlowTracker)
    StateEstimator<TestLine2D> estimator(std::move(kalman_filter), std::move(feature_extractor));

    // 2. --- CREATE TEST DATA ---
    EntityGroupManager group_manager;
    GroupId group1 = group_manager.createGroup("Group1");
    GroupId group2 = group_manager.createGroup("Group2");

    auto makeA = [](int frame, double x, double y) {
        TestLine2D line;
        line.p1 = Eigen::Vector2d(x - 1.0, y);
        line.p2 = Eigen::Vector2d(x + 1.0, y);
        return line;
    };

    auto makeB = [](int frame, double x, double y) {
        TestLine2D line;
        line.p1 = Eigen::Vector2d(x - 1.0, y);
        line.p2 = Eigen::Vector2d(x + 1.0, y);
        return line;
    };

    std::vector<std::tuple<TestLine2D, EntityId, TimeFrameIndex>> data_source;

    // Create two tracks moving in opposite directions
    // Group 1: Moving right (x increases) - but skip frame 5 initially
    for (int i = 0; i <= 10; ++i) {
        if (i == 5) continue;  // Skip frame 5, we'll add an outlier there
        EntityId entity_id = 1000 + i;
        data_source.emplace_back(makeA(i, 10.0 + i * 2.0, 10.0), entity_id, TimeFrameIndex(i));
        group_manager.addEntityToGroup(group1, entity_id);
    }

    // Group 2: Moving left (x decreases)
    for (int i = 0; i <= 10; ++i) {
        EntityId entity_id = 2000 + i;
        data_source.emplace_back(makeB(i, 50.0 - i * 2.0, 10.0), entity_id, TimeFrameIndex(i));
        group_manager.addEntityToGroup(group2, entity_id);
    }

    // Add an outlier at frame 5 for group 1 (large deviation from expected trajectory)
    EntityId outlier_id = 1005;
    data_source.emplace_back(makeA(5, 100.0, 10.0), outlier_id, TimeFrameIndex(5));  // x=100 instead of ~20
    group_manager.addEntityToGroup(group1, outlier_id);

    // 3. --- TEST SMOOTHING ---
    auto smoothed_results = estimator.smoothGroups(data_source, group_manager, 
                                                   TimeFrameIndex(0), TimeFrameIndex(10));

    REQUIRE(smoothed_results.size() == 2);  // Two groups
    REQUIRE(smoothed_results.count(group1) > 0);
    REQUIRE(smoothed_results.count(group2) > 0);

    // Check that we got smoothed states for both groups
    auto const & group1_states = smoothed_results.at(group1);
    auto const & group2_states = smoothed_results.at(group2);
    
    REQUIRE(group1_states.size() > 0);
    REQUIRE(group2_states.size() > 0);

    // 4. --- TEST OUTLIER DETECTION ---
    auto outlier_results = estimator.detectOutliers(data_source, group_manager,
                                                    TimeFrameIndex(0), TimeFrameIndex(10),
                                                    2.0);  // 2-sigma threshold

    // Should detect the outlier we added
    REQUIRE(outlier_results.outliers.size() > 0);
    
    // Check that statistics were computed
    REQUIRE(outlier_results.mean_innovation.count(group1) > 0);
    REQUIRE(outlier_results.std_innovation.count(group1) > 0);
    REQUIRE(outlier_results.innovation_magnitudes.count(group1) > 0);

    // The outlier should be in the results
    bool found_outlier = false;
    for (auto const & outlier : outlier_results.outliers) {
        if (outlier.entity_id == outlier_id) {
            found_outlier = true;
            REQUIRE(outlier.group_id == group1);
            REQUIRE(outlier.frame == TimeFrameIndex(5));
            REQUIRE(outlier.innovation_magnitude > outlier.threshold_used);
        }
    }
    REQUIRE(found_outlier);
}

TEST_CASE("StateEstimation - MinCostFlowTracker - blackout crossing", "[MinCostFlowTracker][FlipBlackout]") {
    using namespace StateEstimation;

    // 1. --- SETUP ---
    // This setup is identical to the failing test for the old tracker.
    double dt = 1.0;
    Eigen::MatrixXd F(4, 4);
    F << 1, 0, dt, 0, 0, 1, 0, dt, 0, 0, 1, 0, 0, 0, 0, 1;
    Eigen::MatrixXd H(2, 4);
    H << 1, 0, 0, 0, 0, 1, 0, 0;
    Eigen::MatrixXd Q(4, 4);
    Q.setIdentity();
    Q *= 0.1;
    Eigen::MatrixXd R(2, 2);
    R.setIdentity();
    R *= 5.0;

    auto kalman_filter = std::make_unique<KalmanFilter>(F, H, Q, R);
    auto feature_extractor = std::make_unique<LineCentroidExtractor>();

    // Instantiate the new MinCostFlowTracker
    // No max_gap_frames needed - filter uncertainty naturally handles long gaps
    MinCostFlowTracker<TestLine2D> tracker(std::move(kalman_filter), std::move(feature_extractor), H, R);
    tracker.enableDebugLogging("mcf_tracker_blackout_crossing.log");

    // --- Generate Data with blackout and crossing ---
    std::vector<std::tuple<TestLine2D, EntityId, TimeFrameIndex>> data_source;
    EntityGroupManager group_manager;
    GroupId group1 = group_manager.createGroup("Group 1");
    GroupId group2 = group_manager.createGroup("Group 2");

    auto makeA = [](int frame, double x, double y) {
        return TestLine2D{(EntityId)(1000 + frame), {x, y}, {x, y}};
    };
    auto makeB = [](int frame, double x, double y) {
        return TestLine2D{(EntityId)(2000 + frame), {x, y}, {x, y}};
    };

    // Frame 0: Ground truth anchors
    data_source.emplace_back(makeA(0, 10.0, 10.0), (EntityId)1000, TimeFrameIndex(0));
    data_source.emplace_back(makeB(0, 90.0, 10.0), (EntityId)2000, TimeFrameIndex(0));

    MinCostFlowTracker<TestLine2D>::GroundTruthMap ground_truth;
    ground_truth[TimeFrameIndex(0)] = {{group1, (EntityId)1000}, {group2, (EntityId)2000}};

    // Frames 1-2: Lines move toward each other
    data_source.emplace_back(makeA(1, 15.0, 10.0), (EntityId)1001, TimeFrameIndex(1));
    data_source.emplace_back(makeB(1, 85.0, 10.0), (EntityId)2001, TimeFrameIndex(1));
    data_source.emplace_back(makeA(2, 20.0, 10.0), (EntityId)1002, TimeFrameIndex(2));
    data_source.emplace_back(makeB(2, 80.0, 10.0), (EntityId)2002, TimeFrameIndex(2));

    // Frames 3-7: BLACKOUT

    // Frame 8: Post-blackout, ambiguous observations
    data_source.emplace_back(makeA(8, 52.0, 10.0), (EntityId)1008, TimeFrameIndex(8));
    data_source.emplace_back(makeB(8, 48.0, 10.0), (EntityId)2008, TimeFrameIndex(8));

    // Frames 9-10: Lines continue moving
    data_source.emplace_back(makeA(9, 54.0, 10.0), (EntityId)1009, TimeFrameIndex(9));
    data_source.emplace_back(makeB(9, 49.0, 10.0), (EntityId)2009, TimeFrameIndex(9));
    data_source.emplace_back(makeA(10, 56.0, 10.0), (EntityId)1010, TimeFrameIndex(10));
    data_source.emplace_back(makeB(10, 50.0, 10.0), (EntityId)2010, TimeFrameIndex(10));

    // Frame 11: Final ground truth anchor
    data_source.emplace_back(makeA(11, 58.0, 10.0), (EntityId)1011, TimeFrameIndex(11));
    data_source.emplace_back(makeB(11, 51.0, 10.0), (EntityId)2011, TimeFrameIndex(11));
    ground_truth[TimeFrameIndex(11)] = {{group1, (EntityId)1011}, {group2, (EntityId)2011}};

    // 2. --- EXECUTION ---
    tracker.process(data_source, group_manager, ground_truth, TimeFrameIndex(0), TimeFrameIndex(11));

    // 3. --- ASSERTIONS ---
    REQUIRE(group_manager.hasGroup(group1));
    REQUIRE(group_manager.hasGroup(group2));

    // The MinCostFlowTracker should correctly assign all entities despite the blackout ambiguity.
    std::vector<EntityId> expected_g1 = {1000, 1001, 1002, 1008, 1009, 1010, 1011};
    std::vector<EntityId> expected_g2 = {2000, 2001, 2002, 2008, 2009, 2010, 2011};

    auto group1_entities = group_manager.getEntitiesInGroup(group1);
    auto group2_entities = group_manager.getEntitiesInGroup(group2);
    std::sort(group1_entities.begin(), group1_entities.end());
    std::sort(group2_entities.begin(), group2_entities.end());

    REQUIRE(group1_entities == expected_g1);
    REQUIRE(group2_entities == expected_g2);
}
