#include "StateEstimation/StateEstimation.hpp"
#include "CoreGeometry/lines.hpp"
#include <iostream>
#include <vector>

/**
 * @brief Demo showing how to migrate from the current line_kalman_grouping to the new framework
 */
void demonstrateNewTrackingFramework() {
    using namespace StateEstimation;
    
    std::cout << "=== Multi-Feature Tracking Framework Demo ===" << std::endl;
    
    // 1. Create feature extractor for Line2D objects
    LineFeatureExtractor::Config extractor_config;
    extractor_config.extract_centroid = true;      // Current approach uses centroid
    extractor_config.extract_length = true;        // Add length as additional feature
    extractor_config.extract_orientation = true;   // Add orientation as additional feature
    
    auto feature_extractor = std::make_unique<LineFeatureExtractor>(extractor_config);
    
    // 2. Configure tracking session
    TrackingSessionConfig session_config;
    
    // Kalman filter configuration
    session_config.kalman_config.dt = 1.0;
    session_config.kalman_config.default_process_noise = 10.0;
    session_config.kalman_config.default_measurement_noise = 5.0;
    
    // Feature-specific noise (could be learned from data like current approach)
    session_config.kalman_config.feature_process_noise["centroid"] = 10.0;
    session_config.kalman_config.feature_measurement_noise["centroid"] = 5.0;
    session_config.kalman_config.feature_process_noise["length"] = 2.0;
    session_config.kalman_config.feature_measurement_noise["length"] = 1.0;
    
    // Assignment configuration  
    session_config.assignment_constraints.max_cost = 100.0;  // Similar to max_assignment_distance
    session_config.assignment_constraints.required_features = {"centroid"}; // Must have centroid
    session_config.assignment_constraints.optional_features = {"length", "orientation"};
    
    // 3. Create tracking session
    auto tracking_session = std::make_unique<TrackingSession>(session_config);
    
    // 4. Set up Mahalanobis distance cost function (similar to current approach)
    Eigen::MatrixXd covariance = Eigen::MatrixXd::Identity(feature_extractor->getFeatureDimension(), 
                                                          feature_extractor->getFeatureDimension());
    covariance *= 25.0; // Corresponds to measurement_noise^2
    
    auto cost_function = CostFunctions::MahalanobisDistance(covariance);
    auto assignment_algorithm = std::make_unique<HungarianAssignment>(
        [cost_function](FeatureVector const& obj, FeatureVector const& target) {
            return cost_function(obj, target);
        }
    );
    tracking_session->setAssignmentAlgorithm(std::move(assignment_algorithm));
    
    // 5. Simulate tracking workflow
    std::cout << "Simulating tracking workflow..." << std::endl;
    
    // Example Line2D objects (normally from LineData)
    std::vector<Line2D> frame1_lines = {
        {{Point2D{10, 20}, Point2D{15, 25}, Point2D{20, 30}}},  // Line 1
        {{Point2D{50, 60}, Point2D{55, 65}, Point2D{60, 70}}}   // Line 2
    };
    
    std::vector<Line2D> frame2_lines = {
        {{Point2D{12, 22}, Point2D{17, 27}, Point2D{22, 32}}},  // Line 1 moved
        {{Point2D{52, 62}, Point2D{57, 67}, Point2D{62, 72}}},  // Line 2 moved
        {{Point2D{80, 90}, Point2D{85, 95}, Point2D{90, 100}}} // New line
    };
    
    // Frame 1: Initialize with ground truth (like anchor frames)
    std::cout << "Frame 1: Initializing groups with ground truth..." << std::endl;
    
    std::vector<FeatureVector> frame1_features;
    for (auto const& line : frame1_lines) {
        frame1_features.push_back(feature_extractor->extractFeatures(line));
    }
    
    // Ground truth assignments for frame 1
    std::unordered_map<std::size_t, GroupId> ground_truth_frame1;
    ground_truth_frame1[0] = 1;  // First line -> Group 1
    ground_truth_frame1[1] = 2;  // Second line -> Group 2
    
    auto result1 = tracking_session->processObservations(frame1_features, 1.0, ground_truth_frame1);
    std::cout << "Frame 1 result: " << result1.updated_groups.size() << " groups updated" << std::endl;
    
    // Frame 2: Predict and assign (like current forward tracking)
    std::cout << "Frame 2: Predicting and assigning..." << std::endl;
    
    std::vector<FeatureVector> frame2_features;
    for (auto const& line : frame2_lines) {
        frame2_features.push_back(feature_extractor->extractFeatures(line));
    }
    
    auto result2 = tracking_session->processObservations(frame2_features, 2.0);
    std::cout << "Frame 2 result: " << result2.updated_groups.size() << " groups updated, " 
              << result2.unassigned_objects.size() << " unassigned" << std::endl;
    
    // 6. Show predictions
    std::cout << "Getting predictions for frame 3..." << std::endl;
    auto predictions = tracking_session->getPredictions(3.0);
    
    for (auto const& [group_id, prediction] : predictions) {
        std::cout << "Group " << group_id << " prediction confidence: " << prediction.confidence << std::endl;
        
        // Extract centroid from prediction
        if (prediction.predicted_features.hasFeature("centroid")) {
            auto centroid = prediction.predicted_features.getFeature("centroid");
            std::cout << "  Predicted centroid: (" << centroid[0] << ", " << centroid[1] << ")" << std::endl;
        }
    }
    
    std::cout << "=== Demo Complete ===" << std::endl;
}

/**
 * @brief Example of how to extend the framework with custom features
 */
void demonstrateCustomFeatures() {
    using namespace StateEstimation;
    
    std::cout << "\n=== Custom Feature Extension Demo ===" << std::endl;
    
    // Example: Add intensity features to line tracking
    class IntensityLineExtractor : public FeatureExtractor<Line2D> {
    public:
        FeatureVector extractFeatures(Line2D const& line) const override {
            FeatureVector features;
            
            // Basic geometric features
            Eigen::Vector2d centroid = LineFeatureUtils::calculateLineCentroid(line);
            features.addFeature("centroid", FeatureType::POSITION, centroid, true);
            
            // Simulated intensity feature (would come from image data)
            Eigen::VectorXd intensity(1);
            intensity[0] = 128.0 + (std::rand() % 50); // Random intensity
            features.addFeature("intensity", FeatureType::INTENSITY, intensity, false);
            
            // Shape complexity feature
            Eigen::VectorXd complexity(1);
            complexity[0] = static_cast<double>(line.size()); // Simple complexity measure
            features.addFeature("complexity", FeatureType::SHAPE, complexity, false);
            
            return features;
        }
        
        std::vector<std::string> getFeatureNames() const override {
            return {"centroid", "intensity", "complexity"};
        }
        
        std::size_t getFeatureDimension() const override {
            return 4; // 2D centroid + 1D intensity + 1D complexity
        }
    };
    
    // Create tracking session with custom features
    TrackingSessionConfig config;
    config.kalman_config.include_derivatives[FeatureType::POSITION] = true;   // Centroid has velocity
    config.kalman_config.include_derivatives[FeatureType::INTENSITY] = false; // Intensity doesn't
    config.kalman_config.include_derivatives[FeatureType::SHAPE] = false;     // Shape doesn't
    
    // Feature-specific noise parameters
    config.kalman_config.feature_process_noise["centroid"] = 10.0;
    config.kalman_config.feature_process_noise["intensity"] = 5.0;
    config.kalman_config.feature_process_noise["complexity"] = 1.0;
    
    TrackingSession session(config);
    
    // Set up weighted assignment (prioritize position over intensity)
    std::unordered_map<std::string, double> feature_weights;
    feature_weights["centroid"] = 1.0;      // Full weight for position
    feature_weights["intensity"] = 0.3;     // Lower weight for intensity
    feature_weights["complexity"] = 0.1;    // Minimal weight for complexity
    
    auto weighted_cost = CostFunctions::FeatureWeightedDistance(feature_weights);
    auto assignment_alg = std::make_unique<HungarianAssignment>(
        [weighted_cost](FeatureVector const& obj, FeatureVector const& target) {
            return weighted_cost(obj, target);
        }
    );
    session.setAssignmentAlgorithm(std::move(assignment_alg));
    
    std::cout << "Custom feature tracking session configured!" << std::endl;
    std::cout << "Features: centroid (with velocity), intensity, complexity" << std::endl;
    std::cout << "Assignment: weighted by feature importance" << std::endl;
}

int main() {
    ::demonstrateNewTrackingFramework();
    ::demonstrateCustomFeatures();
    return 0;
}