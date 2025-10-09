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

TEST_CASE("StateEstimator - multiple outliers with large jumps", "[StateEstimator][Outliers][Comprehensive]") {
    using namespace StateEstimation;

    // 1. --- SETUP ---
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
    R *= 0.5;  // Reduced measurement noise to make outliers more obvious

    auto kalman_filter = std::make_unique<KalmanFilter>(F, H, Q, R);
    auto feature_extractor = std::make_unique<LineCentroidExtractor>();

    StateEstimator<TestLine2D> estimator(std::move(kalman_filter), std::move(feature_extractor));

    // 2. --- CREATE TEST DATA WITH MULTIPLE OUTLIERS ---
    EntityGroupManager group_manager;
    GroupId track1 = group_manager.createGroup("Track1");
    GroupId track2 = group_manager.createGroup("Track2");

    auto makeLine = [](int frame, double x, double y, EntityId id) {
        TestLine2D line;
        line.id = id;
        line.p1 = Eigen::Vector2d(x - 1.0, y);
        line.p2 = Eigen::Vector2d(x + 1.0, y);
        return line;
    };

    std::vector<std::tuple<TestLine2D, EntityId, TimeFrameIndex>> data_source;

    // Track 1: Moving smoothly from (0,0) to (30,30) with THREE large error jumps
    std::vector<EntityId> track1_outlier_ids;
    for (int i = 0; i <= 30; ++i) {
        EntityId eid = 1000 + i;
        double x = i * 1.0;
        double y = i * 1.0;
        
        // Inject VERY large jumps representing calculation errors
        if (i == 8) {
            x += 35.0;  // Very large error in x
            y += 30.0;  // Very large error in y
            track1_outlier_ids.push_back(eid);
        } else if (i == 16) {
            x -= 32.0;  // Very large negative error
            y += 28.0;
            track1_outlier_ids.push_back(eid);
        } else if (i == 24) {
            x += 40.0;  // Another very large error
            y -= 25.0;
            track1_outlier_ids.push_back(eid);
        }
        
        TestLine2D line = makeLine(i, x, y, eid);
        data_source.emplace_back(line, eid, TimeFrameIndex(i));
        group_manager.addEntityToGroup(track1, eid);
    }

    // Track 2: Moving from (50,10) to (50,40) with TWO outliers
    std::vector<EntityId> track2_outlier_ids;
    for (int i = 0; i <= 30; ++i) {
        EntityId eid = 2000 + i;
        double x = 50.0;
        double y = 10.0 + i * 1.0;
        
        // Inject very large errors
        if (i == 10) {
            x += 45.0;  // Huge horizontal error
            track2_outlier_ids.push_back(eid);
        } else if (i == 22) {
            y += 35.0;  // Huge vertical error
            track2_outlier_ids.push_back(eid);
        }
        
        TestLine2D line = makeLine(i, x, y, eid);
        data_source.emplace_back(line, eid, TimeFrameIndex(i));
        group_manager.addEntityToGroup(track2, eid);
    }

    // 3. --- TEST OUTLIER DETECTION WITH 3-SIGMA THRESHOLD ---
    INFO("Testing with 3-sigma threshold (should catch major outliers)");
    auto results_3sigma = estimator.detectOutliers(
        data_source, group_manager,
        TimeFrameIndex(0), TimeFrameIndex(30),
        3.0  // 3-sigma threshold
    );

    // Print statistics for debugging
    INFO("3-sigma results: " << results_3sigma.outliers.size() << " outliers detected");
    if (results_3sigma.mean_innovation.count(track1) > 0) {
        INFO("Track1 mean innovation: " << results_3sigma.mean_innovation[track1]);
        INFO("Track1 std deviation: " << results_3sigma.std_innovation[track1]);
    }
    if (results_3sigma.mean_innovation.count(track2) > 0) {
        INFO("Track2 mean innovation: " << results_3sigma.mean_innovation[track2]);
        INFO("Track2 std deviation: " << results_3sigma.std_innovation[track2]);
    }
    
    // Verify statistics were computed for both tracks
    REQUIRE(results_3sigma.mean_innovation.count(track1) > 0);
    REQUIRE(results_3sigma.std_innovation.count(track1) > 0);
    REQUIRE(results_3sigma.mean_innovation.count(track2) > 0);
    REQUIRE(results_3sigma.std_innovation.count(track2) > 0);

    // Collect detected outlier IDs and print info
    std::set<EntityId> detected_3sigma;
    for (auto const & outlier : results_3sigma.outliers) {
        detected_3sigma.insert(outlier.entity_id);
        
        // Verify each outlier exceeds its threshold
        REQUIRE(outlier.innovation_magnitude > outlier.threshold_used);
        INFO("Outlier EntityID: " << outlier.entity_id 
             << " at frame " << outlier.frame.getValue()
             << " with magnitude " << outlier.innovation_magnitude
             << " (threshold: " << outlier.threshold_used << ")");
    }

    // We injected 5 major outliers, but 3-sigma is conservative
    // so we require at least 3 to be detected (the largest ones)
    REQUIRE(results_3sigma.outliers.size() >= 3);
    
    // Count how many of our injected outliers were found
    int track1_found = 0;
    for (EntityId eid : track1_outlier_ids) {
        if (detected_3sigma.count(eid) > 0) {
            track1_found++;
            INFO("Track1 outlier EntityID " << eid << " was detected");
        } else {
            INFO("Track1 outlier EntityID " << eid << " was NOT detected");
        }
    }
    int track2_found = 0;
    for (EntityId eid : track2_outlier_ids) {
        if (detected_3sigma.count(eid) > 0) {
            track2_found++;
            INFO("Track2 outlier EntityID " << eid << " was detected");
        } else {
            INFO("Track2 outlier EntityID " << eid << " was NOT detected");
        }
    }
    
    // With 3-sigma, we should find at least 2 of our 5 injected outliers
    REQUIRE((track1_found + track2_found) >= 2);

    // 4. --- TEST WITH TIGHTER 2-SIGMA THRESHOLD ---
    INFO("Testing with 2-sigma threshold (should catch more outliers)");
    auto results_2sigma = estimator.detectOutliers(
        data_source, group_manager,
        TimeFrameIndex(0), TimeFrameIndex(30),
        2.0  // Tighter threshold
    );

    INFO("2-sigma results: " << results_2sigma.outliers.size() << " outliers detected");
    
    // Tighter threshold should detect at least as many outliers
    REQUIRE(results_2sigma.outliers.size() >= results_3sigma.outliers.size());

    std::set<EntityId> detected_2sigma;
    for (auto const & outlier : results_2sigma.outliers) {
        detected_2sigma.insert(outlier.entity_id);
        INFO("2-sigma Outlier EntityID: " << outlier.entity_id 
             << " at frame " << outlier.frame.getValue()
             << " magnitude: " << outlier.innovation_magnitude);
    }

    // All 3-sigma outliers should also be in 2-sigma results
    for (EntityId eid : detected_3sigma) {
        INFO("EntityID " << eid << " from 3-sigma should also be in 2-sigma");
        REQUIRE(detected_2sigma.count(eid) > 0);
    }

    // Report how many additional outliers were found
    size_t additional_outliers = results_2sigma.outliers.size() - results_3sigma.outliers.size();
    INFO("2-sigma threshold found " << additional_outliers << " additional outliers beyond 3-sigma");
    INFO("Total outliers with 2-sigma: " << results_2sigma.outliers.size());
    INFO("Total outliers with 3-sigma: " << results_3sigma.outliers.size());

    // Should find at least 4 of our 5 injected outliers with 2-sigma
    int total_2sigma_found = 0;
    for (EntityId eid : track1_outlier_ids) {
        if (detected_2sigma.count(eid) > 0) total_2sigma_found++;
    }
    for (EntityId eid : track2_outlier_ids) {
        if (detected_2sigma.count(eid) > 0) total_2sigma_found++;
    }
    INFO("2-sigma found " << total_2sigma_found << " of 5 injected outliers");
    REQUIRE(total_2sigma_found >= 4);

    // 5. --- TEST WITH EVEN TIGHTER 1.5-SIGMA THRESHOLD ---
    INFO("Testing with 1.5-sigma threshold (should catch even more)");
    auto results_1_5sigma = estimator.detectOutliers(
        data_source, group_manager,
        TimeFrameIndex(0), TimeFrameIndex(30),
        1.5  // Even tighter threshold
    );

    INFO("1.5-sigma results: " << results_1_5sigma.outliers.size() << " outliers detected");
    
    // Should find at least as many as 2-sigma
    REQUIRE(results_1_5sigma.outliers.size() >= results_2sigma.outliers.size());

    std::set<EntityId> detected_1_5sigma;
    for (auto const & outlier : results_1_5sigma.outliers) {
        detected_1_5sigma.insert(outlier.entity_id);
    }
    
    // Should find ALL 5 of our injected outliers with 1.5-sigma
    int total_1_5sigma_found = 0;
    for (EntityId eid : track1_outlier_ids) {
        if (detected_1_5sigma.count(eid) > 0) total_1_5sigma_found++;
    }
    for (EntityId eid : track2_outlier_ids) {
        if (detected_1_5sigma.count(eid) > 0) total_1_5sigma_found++;
    }
    INFO("1.5-sigma found " << total_1_5sigma_found << " of 5 injected outliers");
    REQUIRE(total_1_5sigma_found == 5);
    
    // Verify the hierarchy: 1.5-sigma >= 2-sigma >= 3-sigma
    REQUIRE(results_1_5sigma.outliers.size() >= results_2sigma.outliers.size());
    REQUIRE(results_2sigma.outliers.size() >= results_3sigma.outliers.size());
}

TEST_CASE("StateEstimator - outlier detection with varying error magnitudes", "[StateEstimator][Outliers][Thresholds]") {
    using namespace StateEstimation;

    // Setup with lower measurement noise for better sensitivity
    double dt = 1.0;
    Eigen::MatrixXd F(4, 4);
    F << 1, 0, dt, 0, 0, 1, 0, dt, 0, 0, 1, 0, 0, 0, 0, 1;
    Eigen::MatrixXd H(2, 4);
    H << 1, 0, 0, 0, 0, 1, 0, 0;
    Eigen::MatrixXd Q = Eigen::MatrixXd::Identity(4, 4) * 0.05;
    Eigen::MatrixXd R = Eigen::MatrixXd::Identity(2, 2) * 0.3;  // Lower noise for better sensitivity

    StateEstimator<TestLine2D> estimator(
        std::make_unique<KalmanFilter>(F, H, Q, R),
        std::make_unique<LineCentroidExtractor>()
    );

    // Create track with small, medium, and large errors
    std::vector<std::tuple<TestLine2D, EntityId, TimeFrameIndex>> data_source;
    EntityGroupManager group_manager;
    GroupId group = group_manager.createGroup("VaryingErrors");

    std::map<std::string, std::vector<EntityId>> error_categories = {
        {"small", {}},
        {"medium", {}},
        {"large", {}}
    };

    // Create a smooth trajectory with injected errors of varying magnitude
    for (int i = 0; i <= 40; ++i) {
        EntityId eid = 3000 + i;
        double x = i * 1.0;
        double y = i * 0.5;
        
        // Inject errors of different magnitudes - make them more distinct
        if (i == 10) {
            x += 8.0;  // Small-ish error
            error_categories["small"].push_back(eid);
        } else if (i == 20) {
            x += 20.0;  // Medium error
            error_categories["medium"].push_back(eid);
        } else if (i == 30) {
            x += 40.0;  // Large error
            error_categories["large"].push_back(eid);
        }
        
        TestLine2D line;
        line.id = eid;
        line.p1 = Eigen::Vector2d(x - 0.5, y);
        line.p2 = Eigen::Vector2d(x + 0.5, y);
        
        data_source.emplace_back(line, eid, TimeFrameIndex(i));
        group_manager.addEntityToGroup(group, eid);
    }

    // Test multiple thresholds and verify detection hierarchy
    auto results_3sigma = estimator.detectOutliers(data_source, group_manager, 
                                                   TimeFrameIndex(0), TimeFrameIndex(40), 3.0);
    auto results_2sigma = estimator.detectOutliers(data_source, group_manager, 
                                                   TimeFrameIndex(0), TimeFrameIndex(40), 2.0);
    auto results_1sigma = estimator.detectOutliers(data_source, group_manager, 
                                                   TimeFrameIndex(0), TimeFrameIndex(40), 1.0);

    INFO("3-sigma found " << results_3sigma.outliers.size() << " outliers");
    INFO("2-sigma found " << results_2sigma.outliers.size() << " outliers");
    INFO("1-sigma found " << results_1sigma.outliers.size() << " outliers");

    // Get detected IDs for each threshold
    std::set<EntityId> ids_3sigma, ids_2sigma, ids_1sigma;
    for (auto const & o : results_3sigma.outliers) {
        ids_3sigma.insert(o.entity_id);
        INFO("3-sigma detected EntityID " << o.entity_id << " at frame " << o.frame.getValue());
    }
    for (auto const & o : results_2sigma.outliers) {
        ids_2sigma.insert(o.entity_id);
        INFO("2-sigma detected EntityID " << o.entity_id << " at frame " << o.frame.getValue());
    }
    for (auto const & o : results_1sigma.outliers) {
        ids_1sigma.insert(o.entity_id);
    }

    // 3-sigma should catch at least the large error
    REQUIRE(results_3sigma.outliers.size() >= 1);
    
    // 2-sigma should catch more (large + medium)
    REQUIRE(results_2sigma.outliers.size() >= results_3sigma.outliers.size());
    REQUIRE(results_2sigma.outliers.size() >= 2);
    
    // 1-sigma should catch all errors
    REQUIRE(results_1sigma.outliers.size() >= results_2sigma.outliers.size());
    REQUIRE(results_1sigma.outliers.size() >= 3);
    
    // Large error should be in all results
    for (EntityId eid : error_categories["large"]) {
        INFO("Checking large error EntityID " << eid);
        REQUIRE(ids_3sigma.count(eid) > 0);
        REQUIRE(ids_2sigma.count(eid) > 0);
        REQUIRE(ids_1sigma.count(eid) > 0);
    }
    
    // Medium error should be in 2-sigma and 1-sigma
    for (EntityId eid : error_categories["medium"]) {
        INFO("Checking medium error EntityID " << eid);
        REQUIRE(ids_2sigma.count(eid) > 0);
        REQUIRE(ids_1sigma.count(eid) > 0);
    }
    
    // Small error should be in 1-sigma
    for (EntityId eid : error_categories["small"]) {
        INFO("Checking small error EntityID " << eid);
        REQUIRE(ids_1sigma.count(eid) > 0);
    }
    
    // Verify proper subset relationship: 3σ ⊆ 2σ ⊆ 1σ
    for (EntityId eid : ids_3sigma) {
        REQUIRE(ids_2sigma.count(eid) > 0);
        REQUIRE(ids_1sigma.count(eid) > 0);
    }
    for (EntityId eid : ids_2sigma) {
        REQUIRE(ids_1sigma.count(eid) > 0);
    }
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
