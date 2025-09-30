#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Features/LineFeatureExtractor.hpp"

#include "CoreGeometry/lines.hpp"

#include <cmath>

using namespace StateEstimation;
using Catch::Matchers::WithinAbs;

TEST_CASE("StateEstimation - LineFeatureExtractor - Config", "[LineFeatureExtractor]") {
    SECTION("Default configuration") {

        Config config;

        // Create horizontal line
        Line2D horizontal_line = {
            Point2D<float>{0.0f, 5.0f},
            Point2D<float>{10.0f, 5.0f},
            Point2D<float>{20.0f, 5.0f}
        };
        
        REQUIRE(config.extract_centroid == true);
        REQUIRE(config.extract_length == true);
        REQUIRE(config.extract_orientation == true);
        REQUIRE(config.extract_bounding_box == false);
        REQUIRE(config.extract_endpoints == false);
        REQUIRE(config.extract_curvature == false);
        REQUIRE(config.position_scale == 1.0);
        REQUIRE(config.length_scale == 1.0);
        REQUIRE(config.normalize_orientation == true);
    }
    
    SECTION("Custom configuration") {
        Config config;
        config.extract_centroid = true;
        config.extract_length = false;
        config.extract_bounding_box = true;
        config.position_scale = 2.0;
        config.normalize_orientation = false;
        
        REQUIRE(config.extract_centroid == true);
        REQUIRE(config.extract_length == false);
        REQUIRE(config.extract_bounding_box == true);
        REQUIRE_THAT(config.position_scale, WithinAbs(2.0, 1e-6));
        REQUIRE(config.normalize_orientation == false);
    }
}

TEST_CASE("StateEstimation - LineFeatureExtractor", "[LineFeatureExtractor]") {
    SECTION("Construction") {
        Config config;
        LineFeatureExtractor extractor(config);
        
        REQUIRE(extractor.getConfig().extract_centroid == true);
    }
    
    SECTION("Configuration update") {
        LineFeatureExtractor extractor;
        
        Config new_config;
        new_config.extract_centroid = false;
        new_config.extract_length = true;
        new_config.position_scale = 3.0;
        
        extractor.setConfig(new_config);
        
        REQUIRE(extractor.getConfig().extract_centroid == false);
        REQUIRE(extractor.getConfig().extract_length == true);
        REQUIRE_THAT(extractor.getConfig().position_scale, WithinAbs(3.0, 1e-6));
    }
    
    SECTION("Feature names and dimensions") {
        Config config;
        config.extract_centroid = true;
        config.extract_length = true;
        config.extract_orientation = true;
        config.extract_bounding_box = false;
        config.extract_endpoints = false;
        config.extract_curvature = false;
        
        LineFeatureExtractor extractor(config);
        
        auto feature_names = extractor.getFeatureNames();
        REQUIRE(feature_names.size() == 3);
        REQUIRE(std::find(feature_names.begin(), feature_names.end(), "centroid") != feature_names.end());
        REQUIRE(std::find(feature_names.begin(), feature_names.end(), "length") != feature_names.end());
        REQUIRE(std::find(feature_names.begin(), feature_names.end(), "orientation") != feature_names.end());
        
        REQUIRE(extractor.getFeatureDimension() == 4); // 2 + 1 + 1
    }
    
    SECTION("Extract features from empty line") {
        LineFeatureExtractor extractor;
        Line2D empty_line;
        
        auto features = extractor.extractFeatures(empty_line);
        REQUIRE(features.getFeatureCount() == 0);
    }
    
    SECTION("Extract centroid feature") {
        Config config;
        config.extract_centroid = true;
        config.extract_length = false;
        config.extract_orientation = false;
        
        LineFeatureExtractor extractor(config);
        
        // Create simple line
        Line2D line = {
            Point2D<float>{0.0f, 0.0f},
            Point2D<float>{10.0f, 0.0f},
            Point2D<float>{20.0f, 0.0f}
        };
        
        auto features = extractor.extractFeatures(line);
        
        REQUIRE(features.hasFeature("centroid"));
        REQUIRE_FALSE(features.hasFeature("length"));
        
        auto centroid = features.getFeature("centroid");
        REQUIRE_THAT(centroid[0], WithinAbs(10.0, 1e-6)); // (0+10+20)/3
        REQUIRE_THAT(centroid[1], WithinAbs(0.0, 1e-6));  // (0+0+0)/3
    }
    
    SECTION("Extract length feature") {
        Config config;
        config.extract_centroid = false;
        config.extract_length = true;
        config.extract_orientation = false;
        
        LineFeatureExtractor extractor(config);
        
        // Create line with known length
        Line2D line = {
            Point2D<float>{0.0f, 0.0f},
            Point2D<float>{3.0f, 0.0f},
            Point2D<float>{3.0f, 4.0f}  // Forms 3-4-5 right triangle segments
        };
        
        auto features = extractor.extractFeatures(line);
        
        REQUIRE(features.hasFeature("length"));
        auto length = features.getFeature("length");
        REQUIRE_THAT(length[0], WithinAbs(3.0 + 4.0, 1e-6)); // Two segments: 3 + 4 = 7
    }
    
    SECTION("Extract orientation feature") {
        Config config;
        config.extract_centroid = false;
        config.extract_length = false;
        config.extract_orientation = true;
        
        LineFeatureExtractor extractor(config);
        
        // Create horizontal line
        Line2D horizontal_line = {
            Point2D<float>{0.0f, 5.0f},
            Point2D<float>{10.0f, 5.0f},
            Point2D<float>{20.0f, 5.0f}
        };
        
        auto features = extractor.extractFeatures(horizontal_line);
        
        REQUIRE(features.hasFeature("orientation"));
        auto orientation = features.getFeature("orientation");
        REQUIRE_THAT(orientation[0], WithinAbs(0.0, 1e-2)); // Horizontal = 0 radians
        
        // Create vertical line
        Line2D vertical_line = {
            Point2D<float>{5.0f, 0.0f},
            Point2D<float>{5.0f, 10.0f},
            Point2D<float>{5.0f, 20.0f}
        };
        
        features = extractor.extractFeatures(vertical_line);
        orientation = features.getFeature("orientation");
        REQUIRE_THAT(std::abs(orientation[0]), WithinAbs(M_PI/2, 1e-2)); // Vertical = ±π/2
    }
    
    SECTION("Extract bounding box feature") {
        Config config;
        config.extract_centroid = false;
        config.extract_length = false;
        config.extract_orientation = false;
        config.extract_bounding_box = true;
        
        LineFeatureExtractor extractor(config);
        
        Line2D line = {
            Point2D<float>{1.0f, 2.0f},
            Point2D<float>{5.0f, 8.0f},
            Point2D<float>{3.0f, 4.0f}
        };
        
        auto features = extractor.extractFeatures(line);
        
        REQUIRE(features.hasFeature("bounding_box"));
        auto bbox = features.getFeature("bounding_box");
        REQUIRE_THAT(bbox[0], WithinAbs(4.0, 1e-6)); // max_x - min_x = 5 - 1 = 4
        REQUIRE_THAT(bbox[1], WithinAbs(6.0, 1e-6)); // max_y - min_y = 8 - 2 = 6
    }
    
    SECTION("Extract endpoints feature") {
        Config config;
        config.extract_centroid = false;
        config.extract_length = false;
        config.extract_orientation = false;
        config.extract_endpoints = true;
        
        LineFeatureExtractor extractor(config);
        
        Line2D line = {
            Point2D<float>{1.0f, 2.0f},
            Point2D<float>{5.0f, 6.0f},
            Point2D<float>{9.0f, 10.0f}
        };
        
        auto features = extractor.extractFeatures(line);
        
        REQUIRE(features.hasFeature("endpoints"));
        auto endpoints = features.getFeature("endpoints");
        REQUIRE(endpoints.size() == 4);
        
        // Start point
        REQUIRE_THAT(endpoints[0], WithinAbs(1.0, 1e-6));
        REQUIRE_THAT(endpoints[1], WithinAbs(2.0, 1e-6));
        
        // End point
        REQUIRE_THAT(endpoints[2], WithinAbs(9.0, 1e-6));
        REQUIRE_THAT(endpoints[3], WithinAbs(10.0, 1e-6));
    }
    
    SECTION("Extract all features") {
        Config config;
        config.extract_centroid = true;
        config.extract_length = true;
        config.extract_orientation = true;
        config.extract_bounding_box = true;
        config.extract_endpoints = true;
        config.extract_curvature = true;
        
        LineFeatureExtractor extractor(config);
        
        Line2D line = {
            Point2D<float>{0.0f, 0.0f},
            Point2D<float>{5.0f, 0.0f},
            Point2D<float>{10.0f, 0.0f}
        };
        
        auto features = extractor.extractFeatures(line);
        
        REQUIRE(features.hasFeature("centroid"));
        REQUIRE(features.hasFeature("length"));
        REQUIRE(features.hasFeature("orientation"));
        REQUIRE(features.hasFeature("bounding_box"));
        REQUIRE(features.hasFeature("endpoints"));
        REQUIRE(features.hasFeature("curvature"));
        
        REQUIRE(features.getFeatureCount() == 6);
        REQUIRE(extractor.getFeatureDimension() == 12); // 2+1+1+2+4+2
    }
    
    SECTION("Position scaling") {
        Config config;
        config.extract_centroid = true;
        config.extract_endpoints = true;
        config.position_scale = 0.5;
        config.extract_length = false;
        config.extract_orientation = false;
        
        LineFeatureExtractor extractor(config);
        
        Line2D line = {
            Point2D<float>{0.0f, 0.0f},
            Point2D<float>{10.0f, 20.0f}
        };
        
        auto features = extractor.extractFeatures(line);
        
        auto centroid = features.getFeature("centroid");
        REQUIRE_THAT(centroid[0], WithinAbs(2.5, 1e-6)); // (0+10)/2 * 0.5 = 2.5
        REQUIRE_THAT(centroid[1], WithinAbs(5.0, 1e-6)); // (0+20)/2 * 0.5 = 5.0
        
        auto endpoints = features.getFeature("endpoints");
        REQUIRE_THAT(endpoints[0], WithinAbs(0.0, 1e-6)); // 0 * 0.5
        REQUIRE_THAT(endpoints[1], WithinAbs(0.0, 1e-6)); // 0 * 0.5
        REQUIRE_THAT(endpoints[2], WithinAbs(5.0, 1e-6)); // 10 * 0.5
        REQUIRE_THAT(endpoints[3], WithinAbs(10.0, 1e-6)); // 20 * 0.5
    }
    
    SECTION("Length scaling") {
        Config config;
        config.extract_centroid = false;
        config.extract_length = true;
        config.length_scale = 2.0;
        config.extract_orientation = false;
        
        LineFeatureExtractor extractor(config);
        
        Line2D line = {
            Point2D<float>{0.0f, 0.0f},
            Point2D<float>{3.0f, 4.0f} // Length = 5
        };
        
        auto features = extractor.extractFeatures(line);
        
        auto length = features.getFeature("length");
        REQUIRE_THAT(length[0], WithinAbs(10.0, 1e-6)); // 5 * 2.0 = 10
    }
    
    SECTION("Orientation normalization") {
        Config config;
        config.extract_centroid = false;
        config.extract_length = false;
        config.extract_orientation = true;
        config.normalize_orientation = true;
        
        LineFeatureExtractor extractor(config);
        
        // Create a line that would have orientation > π
        // This is tricky to test directly since PCA orientation is typically in [-π/2, π/2]
        // But we test that normalization doesn't break anything
        Line2D line = {
            Point2D<float>{0.0f, 0.0f},
            Point2D<float>{-5.0f, 0.0f}  // Pointing left
        };
        
        auto features = extractor.extractFeatures(line);
        auto orientation = features.getFeature("orientation");
        
        // Should be normalized to [-π, π]
        REQUIRE(orientation[0] >= -M_PI);
        REQUIRE(orientation[0] <= M_PI);
    }
    
    SECTION("Single point line") {
        LineFeatureExtractor extractor;
        
        Line2D single_point = {Point2D<float>{5.0f, 10.0f}};
        
        auto features = extractor.extractFeatures(single_point);
        
        REQUIRE(features.hasFeature("centroid"));
        auto centroid = features.getFeature("centroid");
        REQUIRE_THAT(centroid[0], WithinAbs(5.0, 1e-6));
        REQUIRE_THAT(centroid[1], WithinAbs(10.0, 1e-6));
        
        REQUIRE(features.hasFeature("length"));
        auto length = features.getFeature("length");
        REQUIRE_THAT(length[0], WithinAbs(0.0, 1e-6)); // No length for single point
    }
}

TEST_CASE("StateEstimation - LineFeatureUtils", "[LineFeatureExtractor]") {
    SECTION("Calculate line centroid") {
        Line2D line = {
            Point2D<float>{0.0f, 0.0f},
            Point2D<float>{6.0f, 0.0f},
            Point2D<float>{3.0f, 9.0f}
        };
        
        auto centroid = LineFeatureUtils::calculateLineCentroid(line);
        REQUIRE_THAT(centroid[0], WithinAbs(3.0, 1e-6)); // (0+6+3)/3
        REQUIRE_THAT(centroid[1], WithinAbs(3.0, 1e-6)); // (0+0+9)/3
    }
    
    SECTION("Calculate line PCA") {
        // Create a horizontal line
        Line2D horizontal_line = {
            Point2D<float>{0.0f, 5.0f},
            Point2D<float>{10.0f, 5.0f},
            Point2D<float>{20.0f, 5.0f}
        };
        
        auto pca = LineFeatureUtils::calculateLinePCA(horizontal_line);
        
        REQUIRE_THAT(pca.mean[0], WithinAbs(10.0, 1e-6));
        REQUIRE_THAT(pca.mean[1], WithinAbs(5.0, 1e-6));
        
        // Primary direction should be roughly horizontal [1, 0]
        REQUIRE_THAT(std::abs(pca.primary_direction[0]), WithinAbs(1.0, 1e-2));
        REQUIRE_THAT(std::abs(pca.primary_direction[1]), WithinAbs(0.0, 1e-2));
        
        // Primary variance should be much larger than secondary
        REQUIRE(pca.primary_variance > pca.secondary_variance);
    }
    
    SECTION("Calculate curvature") {
        // Create a straight line (should have zero curvature)
        Line2D straight_line = {
            Point2D<float>{0.0f, 0.0f},
            Point2D<float>{1.0f, 0.0f},
            Point2D<float>{2.0f, 0.0f},
            Point2D<float>{3.0f, 0.0f}
        };
        
        auto curvatures = LineFeatureUtils::calculateCurvature(straight_line);
        
        // All curvatures should be approximately zero
        for (double curv : curvatures) {
            REQUIRE_THAT(curv, WithinAbs(0.0, 1e-6));
        }
        
        // Create a line with some curvature
        Line2D curved_line = {
            Point2D<float>{0.0f, 0.0f},
            Point2D<float>{1.0f, 1.0f},
            Point2D<float>{2.0f, 0.0f}  // Forms a peak
        };
        
        curvatures = LineFeatureUtils::calculateCurvature(curved_line);
        
        // Should have some non-zero curvature
        REQUIRE(curvatures.size() > 0);
        REQUIRE(curvatures[0] > 0.0);
    }
    
    SECTION("Fit line to points") {
        // Create points that lie on a line y = 2x + 1
        Line2D line_points = {
            Point2D<float>{0.0f, 1.0f},   // y = 2*0 + 1 = 1
            Point2D<float>{1.0f, 3.0f},   // y = 2*1 + 1 = 3
            Point2D<float>{2.0f, 5.0f},   // y = 2*2 + 1 = 5
            Point2D<float>{3.0f, 7.0f}    // y = 2*3 + 1 = 7
        };
        
        auto fit_result = LineFeatureUtils::fitLineToPoints(line_points);
        
        // Point on line should be close to the centroid
        REQUIRE_THAT(fit_result.point_on_line[0], WithinAbs(1.5, 1e-6)); // (0+1+2+3)/4
        REQUIRE_THAT(fit_result.point_on_line[1], WithinAbs(4.0, 1e-6)); // (1+3+5+7)/4
        
        // Direction should be roughly [1, 2] normalized
        double expected_norm = std::sqrt(1.0 + 4.0); // sqrt(1^2 + 2^2)
        REQUIRE_THAT(std::abs(fit_result.direction[0]), WithinAbs(1.0/expected_norm, 1e-2));
        REQUIRE_THAT(std::abs(fit_result.direction[1]), WithinAbs(2.0/expected_norm, 1e-2));
        
        // Total residual should be very small (points lie on line)
        REQUIRE_THAT(fit_result.total_residual, WithinAbs(0.0, 1e-6));
        
        // All point residuals should be very small
        for (double residual : fit_result.point_residuals) {
            REQUIRE_THAT(residual, WithinAbs(0.0, 1e-6));
        }
    }
    
    SECTION("PCA with insufficient points") {
        Line2D single_point = {Point2D<float>{5.0f, 10.0f}};
        
        auto pca = LineFeatureUtils::calculateLinePCA(single_point);
        
        REQUIRE_THAT(pca.mean[0], WithinAbs(5.0, 1e-6));
        REQUIRE_THAT(pca.mean[1], WithinAbs(10.0, 1e-6));
        
        // Should have default directions
        REQUIRE_THAT(pca.primary_direction[0], WithinAbs(1.0, 1e-6));
        REQUIRE_THAT(pca.primary_direction[1], WithinAbs(0.0, 1e-6));
        
        REQUIRE_THAT(pca.primary_variance, WithinAbs(0.0, 1e-6));
        REQUIRE_THAT(pca.secondary_variance, WithinAbs(0.0, 1e-6));
    }
    
    SECTION("Curvature with insufficient points") {
        Line2D insufficient_points = {Point2D<float>{0.0f, 0.0f}, Point2D<float>{1.0f, 1.0f}};
        
        auto curvatures = LineFeatureUtils::calculateCurvature(insufficient_points);
        REQUIRE(curvatures.empty());
    }
}