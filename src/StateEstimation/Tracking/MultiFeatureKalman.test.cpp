#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Tracking/MultiFeatureKalman.hpp"
#include "Features/FeatureVector.hpp"

#include <Eigen/Dense>

using namespace StateEstimation;
using Catch::Matchers::WithinAbs;

TEST_CASE("StateEstimation - MultiFeatureKalman - MultiFeatureKalmanConfig", "[MultiFeatureKalman]") {
    SECTION("Default configuration") {
        MultiFeatureKalmanConfig config;
        
        REQUIRE(config.dt == 1.0);
        REQUIRE(config.default_process_noise == 1.0);
        REQUIRE(config.default_measurement_noise == 1.0);
        REQUIRE(config.default_initial_uncertainty == 10.0);
        
        // Check default derivative settings
        REQUIRE(config.include_derivatives[FeatureType::POSITION] == true);
        REQUIRE(config.include_derivatives[FeatureType::ORIENTATION] == false);
        REQUIRE(config.include_derivatives[FeatureType::SCALE] == false);
    }
    
    SECTION("Custom configuration") {
        MultiFeatureKalmanConfig config;
        config.dt = 0.5;
        config.default_process_noise = 2.0;
        config.feature_process_noise["position"] = 5.0;
        config.feature_measurement_noise["position"] = 1.5;
        
        REQUIRE(config.dt == 0.5);
        REQUIRE(config.default_process_noise == 2.0);
        REQUIRE(config.feature_process_noise["position"] == 5.0);
        REQUIRE(config.feature_measurement_noise["position"] == 1.5);
    }
}

TEST_CASE("StateEstimation - MultiFeatureKalman - FeaturePrediction", "[MultiFeatureKalman]") {
    SECTION("Default construction") {
        FeaturePrediction prediction;
        
        REQUIRE(prediction.confidence == 0.0);
        REQUIRE(prediction.valid == false);
        REQUIRE(prediction.predicted_features.getFeatureCount() == 0);
    }
    
    SECTION("Valid prediction") {
        FeaturePrediction prediction;
        prediction.valid = true;
        prediction.confidence = 0.8;
        
        Eigen::Vector2d position(10.0, 20.0);
        prediction.predicted_features.addFeature("position", FeatureType::POSITION, position);
        
        REQUIRE(prediction.valid == true);
        REQUIRE(prediction.confidence == 0.8);
        REQUIRE(prediction.predicted_features.hasFeature("position"));
    }
}

TEST_CASE("StateEstimation - MultiFeatureKalman - MultiFeatureKalmanFilter", "[MultiFeatureKalman]") {
    SECTION("Construction and initialization") {
        MultiFeatureKalmanConfig config;
        config.dt = 1.0;
        config.default_process_noise = 1.0;
        config.default_measurement_noise = 0.5;
        
        MultiFeatureKalmanFilter filter(config);
        
        REQUIRE_FALSE(filter.isInitialized());
        REQUIRE(filter.getConfig().dt == 1.0);
    }
    
    SECTION("Initialization with features") {
        MultiFeatureKalmanConfig config;
        config.dt = 1.0;
        
        MultiFeatureKalmanFilter filter(config);
        
        // Create initial feature vector
        FeatureVector initial_features;
        Eigen::Vector2d position(5.0, 10.0);
        Eigen::VectorXd length(1);
        length[0] = 15.0;
        
        initial_features.addFeature("position", FeatureType::POSITION, position, true);
        initial_features.addFeature("length", FeatureType::SCALE, length, false);
        
        // Initialize filter
        REQUIRE_NOTHROW(filter.initialize(initial_features, 0.0));
        REQUIRE(filter.isInitialized());
        REQUIRE(filter.getCurrentTime() == 0.0);
        
        // Check that we can get current features
        auto current_features = filter.getCurrentFeatures();
        REQUIRE(current_features.hasFeature("position"));
        REQUIRE(current_features.hasFeature("length"));
        
        auto retrieved_position = current_features.getFeature("position");
        REQUIRE_THAT(retrieved_position[0], WithinAbs(5.0, 1e-6));
        REQUIRE_THAT(retrieved_position[1], WithinAbs(10.0, 1e-6));
    }
    
    SECTION("Prediction without initialization should fail gracefully") {
        MultiFeatureKalmanFilter filter;
        
        auto prediction = filter.predict();
        REQUIRE_FALSE(prediction.valid);
    }
    
    SECTION("Update without initialization should throw") {
        MultiFeatureKalmanFilter filter;
        FeatureVector features;
        
        REQUIRE_THROWS_AS(filter.update(features), std::runtime_error);
    }
    
    SECTION("Position-only tracking") {
        MultiFeatureKalmanConfig config;
        config.dt = 1.0;
        config.default_process_noise = 1.0;
        config.default_measurement_noise = 0.1;
        
        MultiFeatureKalmanFilter filter(config);
        
        // Initialize with position
        FeatureVector initial_features;
        Eigen::Vector2d initial_pos(0.0, 0.0);
        initial_features.addFeature("position", FeatureType::POSITION, initial_pos, true);
        
        filter.initialize(initial_features, 0.0);
        
        // Simulate movement: position moves by (1, 1) each time step
        for (int i = 1; i <= 3; ++i) {
            FeatureVector observation;
            Eigen::Vector2d obs_pos(static_cast<double>(i), static_cast<double>(i));
            observation.addFeature("position", FeatureType::POSITION, obs_pos, true);
            
            filter.update(observation, 1.0);
            
            auto current_features = filter.getCurrentFeatures();
            auto current_pos = current_features.getFeature("position");
            
            // Position should be close to the observation
            REQUIRE_THAT(current_pos[0], WithinAbs(static_cast<double>(i), 0.5));
            REQUIRE_THAT(current_pos[1], WithinAbs(static_cast<double>(i), 0.5));
        }
    }
    
    SECTION("Multi-feature tracking") {
        MultiFeatureKalmanConfig config;
        config.dt = 1.0;
        
        MultiFeatureKalmanFilter filter(config);
        
        // Initialize with position and length
        FeatureVector initial_features;
        Eigen::Vector2d position(0.0, 0.0);
        Eigen::VectorXd length(1);
        length[0] = 10.0;
        
        initial_features.addFeature("position", FeatureType::POSITION, position, true);
        initial_features.addFeature("length", FeatureType::SCALE, length, false);
        
        filter.initialize(initial_features, 0.0);
        
        // Update with new observations
        FeatureVector observation;
        Eigen::Vector2d new_position(2.0, 3.0);
        Eigen::VectorXd new_length(1);
        new_length[0] = 12.0;
        
        observation.addFeature("position", FeatureType::POSITION, new_position, true);
        observation.addFeature("length", FeatureType::SCALE, new_length, false);
        
        filter.update(observation, 1.0);
        
        auto current_features = filter.getCurrentFeatures();
        REQUIRE(current_features.hasFeature("position"));
        REQUIRE(current_features.hasFeature("length"));
        
        auto current_pos = current_features.getFeature("position");
        auto current_len = current_features.getFeature("length");
        
        // Values should be influenced by both initial state and observation
        REQUIRE(current_pos[0] > 0.0);
        REQUIRE(current_pos[1] > 0.0);
        REQUIRE(current_len[0] > 10.0);
    }
    
    SECTION("Prediction with time step") {
        MultiFeatureKalmanConfig config;
        config.dt = 1.0;
        config.default_process_noise = 0.1;
        
        MultiFeatureKalmanFilter filter(config);
        
        // Initialize with position
        FeatureVector initial_features;
        Eigen::Vector2d position(0.0, 0.0);
        initial_features.addFeature("position", FeatureType::POSITION, position, true);
        
        filter.initialize(initial_features, 0.0);
        
        // Add some observations to establish velocity
        for (int i = 1; i <= 2; ++i) {
            FeatureVector observation;
            Eigen::Vector2d obs_pos(static_cast<double>(i), 0.0);
            observation.addFeature("position", FeatureType::POSITION, obs_pos, true);
            filter.update(observation, 1.0);
        }
        
        // Make a prediction
        auto prediction = filter.predict(2.0);
        REQUIRE(prediction.valid);
        REQUIRE(prediction.confidence > 0.0);
        REQUIRE(prediction.predicted_features.hasFeature("position"));
        
        // Predicted position should be ahead of current position
        auto predicted_pos = prediction.predicted_features.getFeature("position");
        auto current_pos = filter.getCurrentFeatures().getFeature("position");
        REQUIRE(predicted_pos[0] >= current_pos[0]);
    }
    
    SECTION("Reset functionality") {
        MultiFeatureKalmanConfig config;
        MultiFeatureKalmanFilter filter(config);
        
        // Initialize filter
        FeatureVector initial_features;
        Eigen::Vector2d position(1.0, 2.0);
        initial_features.addFeature("position", FeatureType::POSITION, position, true);
        
        filter.initialize(initial_features, 0.0);
        REQUIRE(filter.isInitialized());
        
        // Reset filter
        filter.reset();
        REQUIRE_FALSE(filter.isInitialized());
        
        // Should be able to reinitialize
        filter.initialize(initial_features, 5.0);
        REQUIRE(filter.isInitialized());
        REQUIRE(filter.getCurrentTime() == 5.0);
    }
}

TEST_CASE("StateEstimation - GroupTracker", "[MultiFeatureKalman]") {
    SECTION("Construction") {
        MultiFeatureKalmanConfig config;
        GroupTracker tracker(config);
        
        REQUIRE(tracker.getTrackedGroups().empty());
    }
    
    SECTION("Initialize and track groups") {
        MultiFeatureKalmanConfig config;
        config.dt = 1.0;
        
        GroupTracker tracker(config);
        
        // Create feature vectors for two groups
        FeatureVector features1;
        Eigen::Vector2d pos1(10.0, 20.0);
        features1.addFeature("position", FeatureType::POSITION, pos1, true);
        
        FeatureVector features2;
        Eigen::Vector2d pos2(30.0, 40.0);
        features2.addFeature("position", FeatureType::POSITION, pos2, true);
        
        // Initialize groups
        GroupId group1 = 1;
        GroupId group2 = 2;
        
        tracker.initializeGroup(group1, features1, 0.0);
        tracker.initializeGroup(group2, features2, 0.0);
        
        REQUIRE(tracker.isGroupTracked(group1));
        REQUIRE(tracker.isGroupTracked(group2));
        REQUIRE_FALSE(tracker.isGroupTracked(999)); // Non-existent group
        
        auto tracked_groups = tracker.getTrackedGroups();
        REQUIRE(tracked_groups.size() == 2);
        REQUIRE(std::find(tracked_groups.begin(), tracked_groups.end(), group1) != tracked_groups.end());
        REQUIRE(std::find(tracked_groups.begin(), tracked_groups.end(), group2) != tracked_groups.end());
    }
    
    SECTION("Update groups") {
        MultiFeatureKalmanConfig config;
        GroupTracker tracker(config);
        
        // Initialize group
        FeatureVector initial_features;
        Eigen::Vector2d initial_pos(0.0, 0.0);
        initial_features.addFeature("position", FeatureType::POSITION, initial_pos, true);
        
        GroupId group_id = 42;
        tracker.initializeGroup(group_id, initial_features, 0.0);
        
        // Update group
        FeatureVector updated_features;
        Eigen::Vector2d updated_pos(5.0, 7.0);
        updated_features.addFeature("position", FeatureType::POSITION, updated_pos, true);
        
        REQUIRE_NOTHROW(tracker.updateGroup(group_id, updated_features, 1.0));
        
        // Get current features
        auto current_features = tracker.getCurrentFeatures(group_id);
        REQUIRE(current_features.hasFeature("position"));
        
        auto current_pos = current_features.getFeature("position");
        REQUIRE(current_pos[0] > 0.0); // Should be influenced by update
        REQUIRE(current_pos[1] > 0.0);
    }
    
    SECTION("Predictions for groups") {
        MultiFeatureKalmanConfig config;
        config.default_process_noise = 0.1;
        
        GroupTracker tracker(config);
        
        // Initialize group
        FeatureVector features;
        Eigen::Vector2d pos(1.0, 2.0);
        features.addFeature("position", FeatureType::POSITION, pos, true);
        
        GroupId group_id = 100;
        tracker.initializeGroup(group_id, features, 0.0);
        
        // Get prediction
        auto prediction = tracker.predictGroup(group_id, 1.0);
        REQUIRE(prediction.valid);
        REQUIRE(prediction.predicted_features.hasFeature("position"));
        
        // Prediction for non-existent group should be invalid
        auto invalid_prediction = tracker.predictGroup(999, 1.0);
        REQUIRE_FALSE(invalid_prediction.valid);
    }
    
    SECTION("Remove groups") {
        MultiFeatureKalmanConfig config;
        GroupTracker tracker(config);
        
        // Initialize group
        FeatureVector features;
        Eigen::Vector2d pos(1.0, 2.0);
        features.addFeature("position", FeatureType::POSITION, pos, true);
        
        GroupId group_id = 123;
        tracker.initializeGroup(group_id, features, 0.0);
        
        REQUIRE(tracker.isGroupTracked(group_id));
        
        // Remove group
        tracker.removeGroup(group_id);
        REQUIRE_FALSE(tracker.isGroupTracked(group_id));
        REQUIRE(tracker.getTrackedGroups().empty());
    }
    
    SECTION("Reset tracker") {
        MultiFeatureKalmanConfig config;
        GroupTracker tracker(config);
        
        // Initialize multiple groups
        FeatureVector features;
        Eigen::Vector2d pos(1.0, 2.0);
        features.addFeature("position", FeatureType::POSITION, pos, true);
        
        tracker.initializeGroup(1, features, 0.0);
        tracker.initializeGroup(2, features, 0.0);
        
        REQUIRE(tracker.getTrackedGroups().size() == 2);
        
        // Reset
        tracker.reset();
        REQUIRE(tracker.getTrackedGroups().empty());
    }
    
    SECTION("Update non-existent group should throw") {
        GroupTracker tracker;
        
        FeatureVector features;
        Eigen::Vector2d pos(1.0, 2.0);
        features.addFeature("position", FeatureType::POSITION, pos, true);
        
        REQUIRE_THROWS_AS(tracker.updateGroup(999, features, 1.0), std::runtime_error);
    }
}

TEST_CASE("StateEstimation - MultiFeatureKalman Integration", "[MultiFeatureKalman]") {
    SECTION("Complete tracking scenario") {
        // Simulate tracking a moving object with position and size features
        MultiFeatureKalmanConfig config;
        config.dt = 1.0;
        config.default_process_noise = 0.5;
        config.default_measurement_noise = 0.1;
        
        // Position should have derivatives (velocity), size should not
        config.include_derivatives[FeatureType::POSITION] = true;
        config.include_derivatives[FeatureType::SCALE] = false;
        
        GroupTracker tracker(config);
        
        // Ground truth trajectory: object moves diagonally, size varies
        std::vector<std::pair<Eigen::Vector2d, double>> ground_truth = {
            {{0.0, 0.0}, 10.0},
            {{1.0, 1.0}, 11.0},
            {{2.0, 2.0}, 12.0},
            {{3.0, 3.0}, 11.5},
            {{4.0, 4.0}, 10.5}
        };
        
        GroupId group_id = 1;
        
        // Initialize with first observation
        FeatureVector initial_features;
        initial_features.addFeature("position", FeatureType::POSITION, ground_truth[0].first, true);
        Eigen::VectorXd initial_size(1);
        initial_size[0] = ground_truth[0].second;
        initial_features.addFeature("size", FeatureType::SCALE, initial_size, false);
        
        tracker.initializeGroup(group_id, initial_features, 0.0);
        
        // Process subsequent observations
        for (std::size_t i = 1; i < ground_truth.size(); ++i) {
            // Create observation
            FeatureVector observation;
            observation.addFeature("position", FeatureType::POSITION, ground_truth[i].first, true);
            Eigen::VectorXd size(1);
            size[0] = ground_truth[i].second;
            observation.addFeature("size", FeatureType::SCALE, size, false);
            
            // Get prediction before update
            auto prediction = tracker.predictGroup(group_id, 1.0);
            REQUIRE(prediction.valid);
            REQUIRE(prediction.confidence > 0.0);
            
            // Update with observation
            tracker.updateGroup(group_id, observation, 1.0);
            
            // Check that current features are reasonable
            auto current_features = tracker.getCurrentFeatures(group_id);
            auto current_pos = current_features.getFeature("position");
            auto current_size = current_features.getFeature("size");
            
            // Position should be close to ground truth
            REQUIRE_THAT(current_pos[0], WithinAbs(ground_truth[i].first[0], 1.0));
            REQUIRE_THAT(current_pos[1], WithinAbs(ground_truth[i].first[1], 1.0));
            
            // Size should be close to ground truth
            REQUIRE_THAT(current_size[0], WithinAbs(ground_truth[i].second, 2.0));
        }
        
        // Make a final prediction
        auto final_prediction = tracker.predictGroup(group_id, 2.0);
        REQUIRE(final_prediction.valid);
        
        auto predicted_pos = final_prediction.predicted_features.getFeature("position");
        // Should predict continued movement in the same direction
        REQUIRE(predicted_pos[0] > 4.0);
        REQUIRE(predicted_pos[1] > 4.0);
    }
}

TEST_CASE("StateEstimation - MultiFeatureKalman - Feature-specific noise configuration", "[MultiFeatureKalman]") {
    SECTION("Different noise for different features") {
        MultiFeatureKalmanConfig config;
        config.default_process_noise = 1.0;
        config.default_measurement_noise = 0.5;
        
        // Set specific noise for position feature
        config.feature_process_noise["position"] = 2.0;
        config.feature_measurement_noise["position"] = 0.1;
        config.feature_initial_uncertainty["position"] = 5.0;
        
        MultiFeatureKalmanFilter filter(config);
        
        // Create multi-feature observation
        FeatureVector features;
        Eigen::Vector2d position(1.0, 2.0);
        Eigen::VectorXd length(1);
        length[0] = 10.0;
        
        features.addFeature("position", FeatureType::POSITION, position, true);
        features.addFeature("length", FeatureType::SCALE, length, false);
        
        // Should initialize without throwing
        REQUIRE_NOTHROW(filter.initialize(features, 0.0));
        REQUIRE(filter.isInitialized());
        
        // Configuration should be accessible
        REQUIRE(filter.getConfig().feature_process_noise.at("position") == 2.0);
        REQUIRE(filter.getConfig().feature_measurement_noise.at("position") == 0.1);
    }
}
