#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/Layout/SpatialLayoutStrategy.hpp"

#include <glm/vec2.hpp>

using namespace CorePlotting;
using Catch::Matchers::WithinAbs;

// ============================================================================
// SpatialTransform Tests
// ============================================================================

TEST_CASE("SpatialTransform - identity", "[CorePlotting][Layout][SpatialLayout]") {
    auto transform = SpatialTransform::identity();
    
    REQUIRE_THAT(transform.x_scale, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(transform.y_scale, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(transform.x_offset, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(transform.y_offset, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("SpatialTransform - apply point", "[CorePlotting][Layout][SpatialLayout]") {
    SpatialTransform transform;
    transform.x_scale = 2.0f;
    transform.y_scale = 0.5f;
    transform.x_offset = 10.0f;
    transform.y_offset = -5.0f;
    
    glm::vec2 input{5.0f, 20.0f};
    glm::vec2 output = transform.apply(input);
    
    // output.x = 5.0 * 2.0 + 10.0 = 20.0
    // output.y = 20.0 * 0.5 + (-5.0) = 5.0
    REQUIRE_THAT(output.x, WithinAbs(20.0f, 0.001f));
    REQUIRE_THAT(output.y, WithinAbs(5.0f, 0.001f));
}

TEST_CASE("SpatialTransform - apply individual coordinates", "[CorePlotting][Layout][SpatialLayout]") {
    SpatialTransform transform;
    transform.x_scale = 3.0f;
    transform.y_scale = 2.0f;
    transform.x_offset = 1.0f;
    transform.y_offset = -1.0f;
    
    REQUIRE_THAT(transform.applyX(5.0f), WithinAbs(16.0f, 0.001f));  // 5*3 + 1
    REQUIRE_THAT(transform.applyY(5.0f), WithinAbs(9.0f, 0.001f));   // 5*2 - 1
}

// ============================================================================
// SpatialLayoutStrategy - Identity Mode
// ============================================================================

TEST_CASE("SpatialLayoutStrategy - Identity mode", "[CorePlotting][Layout][SpatialLayout]") {
    SpatialLayoutStrategy strategy(SpatialLayoutStrategy::Mode::Identity);
    
    BoundingBox data_bounds(0.0f, 0.0f, 100.0f, 100.0f);
    BoundingBox viewport_bounds(-1.0f, -1.0f, 1.0f, 1.0f);
    
    auto response = strategy.compute(data_bounds, viewport_bounds, 0.0f);
    
    // Identity should not transform
    REQUIRE_THAT(response.layout.transform.x_scale, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(response.layout.transform.y_scale, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(response.layout.transform.x_offset, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(response.layout.transform.y_offset, WithinAbs(0.0f, 0.001f));
}

// ============================================================================
// SpatialLayoutStrategy - Fill Mode
// ============================================================================

TEST_CASE("SpatialLayoutStrategy - Fill mode with square bounds", "[CorePlotting][Layout][SpatialLayout]") {
    SpatialLayoutStrategy strategy(SpatialLayoutStrategy::Mode::Fill);
    
    // Square data bounds (100x100) to square viewport (2x2)
    BoundingBox data_bounds(0.0f, 0.0f, 100.0f, 100.0f);
    BoundingBox viewport_bounds(-1.0f, -1.0f, 1.0f, 1.0f);
    
    auto response = strategy.compute(data_bounds, viewport_bounds, 0.0f);
    
    // Scale: viewport_size / data_size = 2.0 / 100.0 = 0.02
    REQUIRE_THAT(response.layout.transform.x_scale, WithinAbs(0.02f, 0.0001f));
    REQUIRE_THAT(response.layout.transform.y_scale, WithinAbs(0.02f, 0.0001f));
    
    // Verify corners map correctly
    // data (0,0) -> viewport (-1,-1)
    auto output_min = response.layout.transform.apply({0.0f, 0.0f});
    REQUIRE_THAT(output_min.x, WithinAbs(-1.0f, 0.001f));
    REQUIRE_THAT(output_min.y, WithinAbs(-1.0f, 0.001f));
    
    // data (100,100) -> viewport (1,1)
    auto output_max = response.layout.transform.apply({100.0f, 100.0f});
    REQUIRE_THAT(output_max.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(output_max.y, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("SpatialLayoutStrategy - Fill mode with rectangular bounds", "[CorePlotting][Layout][SpatialLayout]") {
    SpatialLayoutStrategy strategy(SpatialLayoutStrategy::Mode::Fill);
    
    // Wide data bounds (200x100) to square viewport (2x2)
    BoundingBox data_bounds(0.0f, 0.0f, 200.0f, 100.0f);
    BoundingBox viewport_bounds(-1.0f, -1.0f, 1.0f, 1.0f);
    
    auto response = strategy.compute(data_bounds, viewport_bounds, 0.0f);
    
    // Non-uniform scale
    // X scale: 2.0 / 200.0 = 0.01
    // Y scale: 2.0 / 100.0 = 0.02
    REQUIRE_THAT(response.layout.transform.x_scale, WithinAbs(0.01f, 0.0001f));
    REQUIRE_THAT(response.layout.transform.y_scale, WithinAbs(0.02f, 0.0001f));
    
    // Corners still map correctly (with distortion)
    auto output_min = response.layout.transform.apply({0.0f, 0.0f});
    REQUIRE_THAT(output_min.x, WithinAbs(-1.0f, 0.001f));
    REQUIRE_THAT(output_min.y, WithinAbs(-1.0f, 0.001f));
    
    auto output_max = response.layout.transform.apply({200.0f, 100.0f});
    REQUIRE_THAT(output_max.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(output_max.y, WithinAbs(1.0f, 0.001f));
}

// ============================================================================
// SpatialLayoutStrategy - Fit Mode (Default)
// ============================================================================

TEST_CASE("SpatialLayoutStrategy - Fit mode with square bounds", "[CorePlotting][Layout][SpatialLayout]") {
    SpatialLayoutStrategy strategy;  // Default is Fit mode
    
    // Square data bounds (100x100) to square viewport (2x2)
    BoundingBox data_bounds(0.0f, 0.0f, 100.0f, 100.0f);
    BoundingBox viewport_bounds(-1.0f, -1.0f, 1.0f, 1.0f);
    
    auto response = strategy.compute(data_bounds, viewport_bounds, 0.0f);
    
    // Uniform scale (same as Fill for square bounds)
    REQUIRE_THAT(response.layout.transform.x_scale, WithinAbs(0.02f, 0.0001f));
    REQUIRE_THAT(response.layout.transform.y_scale, WithinAbs(0.02f, 0.0001f));
    
    // Data should be centered
    auto center = response.layout.transform.apply({50.0f, 50.0f});
    REQUIRE_THAT(center.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(center.y, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("SpatialLayoutStrategy - Fit mode preserves aspect ratio", "[CorePlotting][Layout][SpatialLayout]") {
    SpatialLayoutStrategy strategy;  // Fit mode
    
    // Wide data bounds (200x100) to square viewport (2x2)
    BoundingBox data_bounds(0.0f, 0.0f, 200.0f, 100.0f);
    BoundingBox viewport_bounds(-1.0f, -1.0f, 1.0f, 1.0f);
    
    auto response = strategy.compute(data_bounds, viewport_bounds, 0.0f);
    
    // Uniform scale should be min(2/200, 2/100) = min(0.01, 0.02) = 0.01
    REQUIRE_THAT(response.layout.transform.x_scale, WithinAbs(0.01f, 0.0001f));
    REQUIRE_THAT(response.layout.transform.y_scale, WithinAbs(0.01f, 0.0001f));
    
    // Data should fit within viewport (check corners)
    auto data_min = response.layout.transform.apply({0.0f, 0.0f});
    auto data_max = response.layout.transform.apply({200.0f, 100.0f});
    
    // Data should fit within viewport bounds
    REQUIRE(data_min.x >= -1.0f);
    REQUIRE(data_min.y >= -1.0f);
    REQUIRE(data_max.x <= 1.0f);
    REQUIRE(data_max.y <= 1.0f);
    
    // Data center should map to viewport center
    auto center = response.layout.transform.apply({100.0f, 50.0f});
    REQUIRE_THAT(center.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(center.y, WithinAbs(0.0f, 0.001f));
    
    // Width should touch viewport bounds (200 * 0.01 = 2.0 = viewport width)
    float output_width = data_max.x - data_min.x;
    REQUIRE_THAT(output_width, WithinAbs(2.0f, 0.001f));
    
    // Height should NOT touch viewport bounds (preserving aspect ratio)
    float output_height = data_max.y - data_min.y;
    REQUIRE_THAT(output_height, WithinAbs(1.0f, 0.001f));  // 100 * 0.01 = 1.0
}

TEST_CASE("SpatialLayoutStrategy - Fit mode with tall data", "[CorePlotting][Layout][SpatialLayout]") {
    SpatialLayoutStrategy strategy;  // Fit mode
    
    // Tall data bounds (100x200) to square viewport (2x2)
    BoundingBox data_bounds(0.0f, 0.0f, 100.0f, 200.0f);
    BoundingBox viewport_bounds(-1.0f, -1.0f, 1.0f, 1.0f);
    
    auto response = strategy.compute(data_bounds, viewport_bounds, 0.0f);
    
    // Uniform scale should be min(2/100, 2/200) = min(0.02, 0.01) = 0.01
    REQUIRE_THAT(response.layout.transform.x_scale, WithinAbs(0.01f, 0.0001f));
    REQUIRE_THAT(response.layout.transform.y_scale, WithinAbs(0.01f, 0.0001f));
    
    // Height should touch viewport bounds
    auto data_min = response.layout.transform.apply({0.0f, 0.0f});
    auto data_max = response.layout.transform.apply({100.0f, 200.0f});
    
    float output_height = data_max.y - data_min.y;
    REQUIRE_THAT(output_height, WithinAbs(2.0f, 0.001f));  // 200 * 0.01 = 2.0
    
    // Width should NOT touch viewport bounds
    float output_width = data_max.x - data_min.x;
    REQUIRE_THAT(output_width, WithinAbs(1.0f, 0.001f));  // 100 * 0.01 = 1.0
}

// ============================================================================
// SpatialLayoutStrategy - Padding
// ============================================================================

TEST_CASE("SpatialLayoutStrategy - With padding", "[CorePlotting][Layout][SpatialLayout]") {
    SpatialLayoutStrategy strategy(SpatialLayoutStrategy::Mode::Fill);
    
    BoundingBox data_bounds(0.0f, 0.0f, 100.0f, 100.0f);
    BoundingBox viewport_bounds(-1.0f, -1.0f, 1.0f, 1.0f);
    
    // 10% padding expands data bounds by 10% on each side
    auto response = strategy.compute(data_bounds, viewport_bounds, 0.1f);
    
    // Effective bounds: (-10, -10, 110, 110) = 120x120
    REQUIRE_THAT(response.effective_data_bounds.min_x, WithinAbs(-10.0f, 0.001f));
    REQUIRE_THAT(response.effective_data_bounds.min_y, WithinAbs(-10.0f, 0.001f));
    REQUIRE_THAT(response.effective_data_bounds.max_x, WithinAbs(110.0f, 0.001f));
    REQUIRE_THAT(response.effective_data_bounds.max_y, WithinAbs(110.0f, 0.001f));
    
    // Scale: 2.0 / 120.0 = 0.01667
    REQUIRE_THAT(response.layout.transform.x_scale, WithinAbs(2.0f / 120.0f, 0.0001f));
    REQUIRE_THAT(response.layout.transform.y_scale, WithinAbs(2.0f / 120.0f, 0.0001f));
}

// ============================================================================
// SpatialLayoutStrategy - Edge Cases
// ============================================================================

TEST_CASE("SpatialLayoutStrategy - Degenerate data bounds (zero width)", "[CorePlotting][Layout][SpatialLayout]") {
    SpatialLayoutStrategy strategy;
    
    // Zero-width data bounds
    BoundingBox data_bounds(50.0f, 0.0f, 50.0f, 100.0f);
    BoundingBox viewport_bounds(-1.0f, -1.0f, 1.0f, 1.0f);
    
    auto response = strategy.compute(data_bounds, viewport_bounds, 0.0f);
    
    // Should return identity transform for degenerate case
    REQUIRE_THAT(response.layout.transform.x_scale, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(response.layout.transform.y_scale, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("SpatialLayoutStrategy - Degenerate data bounds (zero height)", "[CorePlotting][Layout][SpatialLayout]") {
    SpatialLayoutStrategy strategy;
    
    // Zero-height data bounds
    BoundingBox data_bounds(0.0f, 50.0f, 100.0f, 50.0f);
    BoundingBox viewport_bounds(-1.0f, -1.0f, 1.0f, 1.0f);
    
    auto response = strategy.compute(data_bounds, viewport_bounds, 0.0f);
    
    // Should return identity transform for degenerate case
    REQUIRE_THAT(response.layout.transform.x_scale, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(response.layout.transform.y_scale, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("SpatialLayoutStrategy - Non-origin data bounds", "[CorePlotting][Layout][SpatialLayout]") {
    SpatialLayoutStrategy strategy(SpatialLayoutStrategy::Mode::Fill);
    
    // Data bounds not starting at origin
    BoundingBox data_bounds(100.0f, 200.0f, 300.0f, 400.0f);  // 200x200 at (100, 200)
    BoundingBox viewport_bounds(0.0f, 0.0f, 400.0f, 400.0f);
    
    auto response = strategy.compute(data_bounds, viewport_bounds, 0.0f);
    
    // Scale: 400 / 200 = 2.0
    REQUIRE_THAT(response.layout.transform.x_scale, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(response.layout.transform.y_scale, WithinAbs(2.0f, 0.001f));
    
    // Verify mapping
    auto output_min = response.layout.transform.apply({100.0f, 200.0f});
    REQUIRE_THAT(output_min.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(output_min.y, WithinAbs(0.0f, 0.001f));
    
    auto output_max = response.layout.transform.apply({300.0f, 400.0f});
    REQUIRE_THAT(output_max.x, WithinAbs(400.0f, 0.001f));
    REQUIRE_THAT(output_max.y, WithinAbs(400.0f, 0.001f));
}

// ============================================================================
// SpatialLayoutStrategy - computeFromRequest
// ============================================================================

TEST_CASE("SpatialLayoutStrategy - computeFromRequest compatibility", "[CorePlotting][Layout][SpatialLayout]") {
    SpatialLayoutStrategy strategy;
    
    LayoutRequest request;
    request.series = {{"spatial_data", SeriesType::Analog, true}};
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    
    auto response = strategy.computeFromRequest(request);
    
    REQUIRE(response.layouts.size() == 1);
    REQUIRE(response.layouts[0].series_id == "spatial_data");
    
    // Should fill viewport height
    REQUIRE_THAT(response.layouts[0].result.allocated_y_center, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(response.layouts[0].result.allocated_height, WithinAbs(2.0f, 0.001f));
}
