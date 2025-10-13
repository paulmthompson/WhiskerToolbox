#include "StateEstimation/Features/CompositeFeatureExtractor.hpp"
#include "StateEstimation/Features/LineCentroidExtractor.hpp"
#include "StateEstimation/Features/LineBasePointExtractor.hpp"
#include "StateEstimation/Features/LineLengthExtractor.hpp"
#include "StateEstimation/Features/FeatureMetadata.hpp"
#include "StateEstimation/Filter/Kalman/KalmanMatrixBuilder.hpp"
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

TEST_CASE("Feature extractors provide correct metadata", "[FeatureMetadata]") {
    auto line = createTestLine();
    
    // Test centroid metadata (KINEMATIC_2D)
    LineCentroidExtractor centroid;
    auto centroid_meta = centroid.getMetadata();
    REQUIRE(centroid_meta.name == "line_centroid");
    REQUIRE(centroid_meta.measurement_size == 2);
    REQUIRE(centroid_meta.state_size == 4);
    REQUIRE(centroid_meta.temporal_type == FeatureTemporalType::KINEMATIC_2D);
    REQUIRE(centroid_meta.hasDerivatives());
    REQUIRE(centroid_meta.getDerivativeOrder() == 1);
    
    // Test base point metadata (KINEMATIC_2D)
    LineBasePointExtractor base;
    auto base_meta = base.getMetadata();
    REQUIRE(base_meta.name == "line_base_point");
    REQUIRE(base_meta.measurement_size == 2);
    REQUIRE(base_meta.state_size == 4);
    REQUIRE(base_meta.temporal_type == FeatureTemporalType::KINEMATIC_2D);
}

TEST_CASE("Composite extractor aggregates metadata correctly", "[CompositeFeatureExtractor]") {
    CompositeFeatureExtractor<Line2D> composite;
    composite.addExtractor(std::make_unique<LineCentroidExtractor>());
    composite.addExtractor(std::make_unique<LineBasePointExtractor>());
    
    auto composite_meta = composite.getMetadata();
    
    // Should combine: 2D + 2D measurements = 4D
    // Should combine: 4D + 4D states = 8D
    REQUIRE(composite_meta.measurement_size == 4);
    REQUIRE(composite_meta.state_size == 8);
    REQUIRE(composite_meta.temporal_type == FeatureTemporalType::CUSTOM);
    
    // Check child metadata
    auto children = composite.getChildMetadata();
    REQUIRE(children.size() == 2);
    REQUIRE(children[0].name == "line_centroid");
    REQUIRE(children[1].name == "line_base_point");
}

TEST_CASE("KalmanMatrixBuilder builds correct matrices from metadata", "[KalmanMatrixBuilder]") {
    // Create metadata for 2 kinematic features
    std::vector<FeatureMetadata> metadata_list = {
        FeatureMetadata::create("feature1", 2, FeatureTemporalType::KINEMATIC_2D),
        FeatureMetadata::create("feature2", 2, FeatureTemporalType::KINEMATIC_2D)
    };
    
    KalmanMatrixBuilder::FeatureConfig config;
    config.dt = 1.0;
    config.process_noise_position = 10.0;
    config.process_noise_velocity = 1.0;
    config.measurement_noise = 5.0;
    
    auto [F, H, Q, R] = KalmanMatrixBuilder::buildAllMatricesFromMetadata(metadata_list, config);
    
    // Check dimensions
    REQUIRE(F.rows() == 8);
    REQUIRE(F.cols() == 8);
    REQUIRE(H.rows() == 4);
    REQUIRE(H.cols() == 8);
    REQUIRE(Q.rows() == 8);
    REQUIRE(Q.cols() == 8);
    REQUIRE(R.rows() == 4);
    REQUIRE(R.cols() == 4);
    
    // Check F is block diagonal
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            REQUIRE_THAT(F(i, j + 4), Catch::Matchers::WithinAbs(0.0, 1e-9));
            REQUIRE_THAT(F(i + 4, j), Catch::Matchers::WithinAbs(0.0, 1e-9));
        }
    }
}

TEST_CASE("Metadata handles mixed feature types correctly", "[FeatureMetadata]") {
    // Test STATIC feature calculation
    auto static_meta = FeatureMetadata::create("length", 1, FeatureTemporalType::STATIC);
    REQUIRE(static_meta.measurement_size == 1);
    REQUIRE(static_meta.state_size == 1);  // No derivatives
    REQUIRE_FALSE(static_meta.hasDerivatives());
    REQUIRE(static_meta.getDerivativeOrder() == 0);
    
    // Test SCALAR_DYNAMIC feature calculation
    auto scalar_meta = FeatureMetadata::create("angle", 1, FeatureTemporalType::SCALAR_DYNAMIC);
    REQUIRE(scalar_meta.measurement_size == 1);
    REQUIRE(scalar_meta.state_size == 2);  // Value + derivative
    REQUIRE(scalar_meta.hasDerivatives());
    REQUIRE(scalar_meta.getDerivativeOrder() == 1);
    
    // Test KINEMATIC_3D feature calculation
    auto kinematic3d_meta = FeatureMetadata::create("position_3d", 3, FeatureTemporalType::KINEMATIC_3D);
    REQUIRE(kinematic3d_meta.measurement_size == 3);
    REQUIRE(kinematic3d_meta.state_size == 6);  // x,y,z,vx,vy,vz
    REQUIRE(kinematic3d_meta.hasDerivatives());
}

TEST_CASE("Mixed feature types work with composite extractor", "[CompositeFeatureExtractor]") {
    auto line = createTestLine();
    
    // Create composite with kinematic + static features
    CompositeFeatureExtractor<Line2D> composite;
    composite.addExtractor(std::make_unique<LineCentroidExtractor>());  // KINEMATIC_2D: 2D → 4D
    composite.addExtractor(std::make_unique<LineLengthExtractor>());    // STATIC: 1D → 1D
    
    auto features = composite.getFilterFeatures(line);
    
    // Should have 3D measurement: [centroid_x, centroid_y, length]
    REQUIRE(features.size() == 3);
    REQUIRE_THAT(features(0), Catch::Matchers::WithinAbs(3.0, 1e-9));  // centroid_x
    REQUIRE_THAT(features(1), Catch::Matchers::WithinAbs(4.0, 1e-9));  // centroid_y
    REQUIRE(features(2) > 0.0);  // length should be positive
    
    auto initial_state = composite.getInitialState(line);
    
    // Should have 5D state: [x, y, vx, vy, length]
    REQUIRE(initial_state.state_mean.size() == 5);
    REQUIRE_THAT(initial_state.state_mean(0), Catch::Matchers::WithinAbs(3.0, 1e-9));  // x
    REQUIRE_THAT(initial_state.state_mean(1), Catch::Matchers::WithinAbs(4.0, 1e-9));  // y
    REQUIRE_THAT(initial_state.state_mean(2), Catch::Matchers::WithinAbs(0.0, 1e-9));  // vx
    REQUIRE_THAT(initial_state.state_mean(3), Catch::Matchers::WithinAbs(0.0, 1e-9));  // vy
    REQUIRE(initial_state.state_mean(4) > 0.0);  // length
}

TEST_CASE("KalmanMatrixBuilder handles mixed feature types", "[KalmanMatrixBuilder]") {
    // Mix of KINEMATIC_2D and STATIC features
    std::vector<FeatureMetadata> metadata_list = {
        FeatureMetadata::create("centroid", 2, FeatureTemporalType::KINEMATIC_2D),  // 2D → 4D
        FeatureMetadata::create("length", 1, FeatureTemporalType::STATIC)            // 1D → 1D
    };
    
    KalmanMatrixBuilder::FeatureConfig config;
    config.dt = 1.0;
    
    auto [F, H, Q, R] = KalmanMatrixBuilder::buildAllMatricesFromMetadata(metadata_list, config);
    
    // Total state: 4D (kinematic) + 1D (static) = 5D
    // Total measurement: 2D + 1D = 3D
    REQUIRE(F.rows() == 5);
    REQUIRE(F.cols() == 5);
    REQUIRE(H.rows() == 3);
    REQUIRE(H.cols() == 5);
    
    // Check F matrix structure
    // First 4×4 block: kinematic model
    REQUIRE_THAT(F(0, 0), Catch::Matchers::WithinAbs(1.0, 1e-9));
    REQUIRE_THAT(F(0, 2), Catch::Matchers::WithinAbs(1.0, 1e-9));  // dt term
    
    // Last 1×1 block: static (identity)
    REQUIRE_THAT(F(4, 4), Catch::Matchers::WithinAbs(1.0, 1e-9));
    
    // Off-diagonal blocks should be zero
    for (int i = 0; i < 4; ++i) {
        REQUIRE_THAT(F(i, 4), Catch::Matchers::WithinAbs(0.0, 1e-9));
        REQUIRE_THAT(F(4, i), Catch::Matchers::WithinAbs(0.0, 1e-9));
    }
    
    // Check H matrix structure
    // Extract position from kinematic feature
    REQUIRE_THAT(H(0, 0), Catch::Matchers::WithinAbs(1.0, 1e-9));  // x
    REQUIRE_THAT(H(1, 1), Catch::Matchers::WithinAbs(1.0, 1e-9));  // y
    // Extract static feature directly
    REQUIRE_THAT(H(2, 4), Catch::Matchers::WithinAbs(1.0, 1e-9));  // length
}
