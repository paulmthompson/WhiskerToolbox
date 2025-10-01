#include "StateEstimation/Features/CompositeFeatureExtractor.hpp"
#include "StateEstimation/Features/LineCentroidExtractor.hpp"
#include "StateEstimation/Features/LineBasePointExtractor.hpp"
#include "StateEstimation/Kalman/KalmanMatrixBuilder.hpp"
#include "CoreGeometry/lines.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <Eigen/Dense>

using namespace StateEstimation;

// Helper function to create a simple test line
static Line2D createTestLine() {
    return Line2D{
        Point2D<float>{1.0f, 2.0f},   // Base point
        Point2D<float>{3.0f, 4.0f},
        Point2D<float>{5.0f, 6.0f}    // Centroid should be (3, 4)
    };
}

TEST_CASE("CompositeFeatureExtractor extracts individual features", "[CompositeFeatureExtractor]") {
    auto line = createTestLine();
    
    // Test centroid extractor alone
    LineCentroidExtractor centroid_extractor;
    auto centroid_features = centroid_extractor.getFilterFeatures(line);
    REQUIRE(centroid_features.size() == 2);
    REQUIRE_THAT(centroid_features(0), Catch::Matchers::WithinAbs(3.0, 1e-9));  // (1+3+5)/3
    REQUIRE_THAT(centroid_features(1), Catch::Matchers::WithinAbs(4.0, 1e-9));  // (2+4+6)/3
    
    // Test base point extractor alone
    LineBasePointExtractor base_extractor;
    auto base_features = base_extractor.getFilterFeatures(line);
    REQUIRE(base_features.size() == 2);
    REQUIRE_THAT(base_features(0), Catch::Matchers::WithinAbs(1.0, 1e-9));
    REQUIRE_THAT(base_features(1), Catch::Matchers::WithinAbs(2.0, 1e-9));
}

TEST_CASE("CompositeFeatureExtractor concatenates features", "[CompositeFeatureExtractor]") {
    auto line = createTestLine();
    
    // Create composite extractor with both features
    CompositeFeatureExtractor<Line2D> composite;
    composite.addExtractor(std::make_unique<LineCentroidExtractor>());
    composite.addExtractor(std::make_unique<LineBasePointExtractor>());
    
    auto features = composite.getFilterFeatures(line);
    
    // Should have 4 features: [centroid_x, centroid_y, base_x, base_y]
    REQUIRE(features.size() == 4);
    REQUIRE_THAT(features(0), Catch::Matchers::WithinAbs(3.0, 1e-9));  // centroid_x
    REQUIRE_THAT(features(1), Catch::Matchers::WithinAbs(4.0, 1e-9));  // centroid_y
    REQUIRE_THAT(features(2), Catch::Matchers::WithinAbs(1.0, 1e-9));  // base_x
    REQUIRE_THAT(features(3), Catch::Matchers::WithinAbs(2.0, 1e-9));  // base_y
}

TEST_CASE("CompositeFeatureExtractor creates correct initial state", "[CompositeFeatureExtractor]") {
    auto line = createTestLine();
    
    CompositeFeatureExtractor<Line2D> composite;
    composite.addExtractor(std::make_unique<LineCentroidExtractor>());
    composite.addExtractor(std::make_unique<LineBasePointExtractor>());
    
    auto initial_state = composite.getInitialState(line);
    
    // Should have 8D state: [centroid_x, centroid_y, centroid_vx, centroid_vy, 
    //                        base_x, base_y, base_vx, base_vy]
    REQUIRE(initial_state.state_mean.size() == 8);
    REQUIRE_THAT(initial_state.state_mean(0), Catch::Matchers::WithinAbs(3.0, 1e-9));  // centroid_x
    REQUIRE_THAT(initial_state.state_mean(1), Catch::Matchers::WithinAbs(4.0, 1e-9));  // centroid_y
    REQUIRE_THAT(initial_state.state_mean(2), Catch::Matchers::WithinAbs(0.0, 1e-9));  // centroid_vx
    REQUIRE_THAT(initial_state.state_mean(3), Catch::Matchers::WithinAbs(0.0, 1e-9));  // centroid_vy
    REQUIRE_THAT(initial_state.state_mean(4), Catch::Matchers::WithinAbs(1.0, 1e-9));  // base_x
    REQUIRE_THAT(initial_state.state_mean(5), Catch::Matchers::WithinAbs(2.0, 1e-9));  // base_y
    REQUIRE_THAT(initial_state.state_mean(6), Catch::Matchers::WithinAbs(0.0, 1e-9));  // base_vx
    REQUIRE_THAT(initial_state.state_mean(7), Catch::Matchers::WithinAbs(0.0, 1e-9));  // base_vy
    
    // Covariance should be 8x8 block diagonal
    REQUIRE(initial_state.state_covariance.rows() == 8);
    REQUIRE(initial_state.state_covariance.cols() == 8);
}

TEST_CASE("CompositeFeatureExtractor clones correctly", "[CompositeFeatureExtractor]") {
    CompositeFeatureExtractor<Line2D> composite;
    composite.addExtractor(std::make_unique<LineCentroidExtractor>());
    composite.addExtractor(std::make_unique<LineBasePointExtractor>());
    
    auto cloned = composite.clone();
    auto line = createTestLine();
    
    auto original_features = composite.getFilterFeatures(line);
    auto cloned_features = cloned->getFilterFeatures(line);
    
    REQUIRE(original_features.size() == cloned_features.size());
    for (int i = 0; i < original_features.size(); ++i) {
        REQUIRE_THAT(original_features(i), Catch::Matchers::WithinAbs(cloned_features(i), 1e-9));
    }
}

TEST_CASE("KalmanMatrixBuilder creates correct dimensions", "[KalmanMatrixBuilder]") {
    KalmanMatrixBuilder::FeatureConfig config;
    config.dt = 1.0;
    config.process_noise_position = 10.0;
    config.process_noise_velocity = 1.0;
    config.measurement_noise = 5.0;
    
    auto [F, H, Q, R] = KalmanMatrixBuilder::buildAllMatrices(2, config);
    
    // For 2 features:
    // F should be 8×8 (4 state dimensions per feature)
    // H should be 4×8 (2 measurement dimensions per feature)
    // Q should be 8×8
    // R should be 4×4
    
    REQUIRE(F.rows() == 8);
    REQUIRE(F.cols() == 8);
    REQUIRE(H.rows() == 4);
    REQUIRE(H.cols() == 8);
    REQUIRE(Q.rows() == 8);
    REQUIRE(Q.cols() == 8);
    REQUIRE(R.rows() == 4);
    REQUIRE(R.cols() == 4);
}

TEST_CASE("KalmanMatrixBuilder creates block diagonal matrices", "[KalmanMatrixBuilder]") {
    KalmanMatrixBuilder::FeatureConfig config;
    config.dt = 1.0;
    
    auto F = KalmanMatrixBuilder::buildF({config, config});
    
    // F should be block diagonal with 4×4 blocks
    // Check that off-diagonal blocks are zero
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            REQUIRE_THAT(F(i, j + 4), Catch::Matchers::WithinAbs(0.0, 1e-9));  // Top-right block
            REQUIRE_THAT(F(i + 4, j), Catch::Matchers::WithinAbs(0.0, 1e-9));  // Bottom-left block
        }
    }
    
    // Check diagonal blocks have expected structure
    REQUIRE_THAT(F(0, 0), Catch::Matchers::WithinAbs(1.0, 1e-9));
    REQUIRE_THAT(F(0, 2), Catch::Matchers::WithinAbs(1.0, 1e-9));  // dt term
    REQUIRE_THAT(F(4, 4), Catch::Matchers::WithinAbs(1.0, 1e-9));
    REQUIRE_THAT(F(4, 6), Catch::Matchers::WithinAbs(1.0, 1e-9));  // dt term
}

TEST_CASE("Empty composite returns empty features", "[CompositeFeatureExtractor]") {
    CompositeFeatureExtractor<Line2D> composite;
    auto line = createTestLine();
    
    auto features = composite.getFilterFeatures(line);
    REQUIRE(features.size() == 0);
    
    auto initial_state = composite.getInitialState(line);
    REQUIRE(initial_state.state_mean.size() == 0);
}

TEST_CASE("Feature cache contains all features", "[CompositeFeatureExtractor]") {
    auto line = createTestLine();
    
    CompositeFeatureExtractor<Line2D> composite;
    composite.addExtractor(std::make_unique<LineCentroidExtractor>());
    composite.addExtractor(std::make_unique<LineBasePointExtractor>());
    
    auto cache = composite.getAllFeatures(line);
    
    // Should contain:
    // - "composite_features" (the concatenated features)
    // - "line_centroid" (from LineCentroidExtractor)
    // - "line_base_point" (from LineBasePointExtractor)
    
    REQUIRE(cache.find("composite_features") != cache.end());
    REQUIRE(cache.find("line_centroid") != cache.end());
    REQUIRE(cache.find("line_base_point") != cache.end());
}
