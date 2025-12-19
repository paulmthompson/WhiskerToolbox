/**
 * @file MVPComposition.test.cpp
 * @brief Tests for MVP matrix composition ensuring correct NDC coverage
 * 
 * These tests verify that Model-View-Projection matrix composition produces
 * correct results, particularly that:
 * 1. Full-canvas elements (intervals) span [-1, 1] in NDC regardless of other series
 * 2. Stacked elements are correctly positioned within their allocated space
 * 3. Adding/removing series doesn't affect full-canvas element coverage
 * 
 * Bug being tested: When analog series are added alongside digital intervals,
 * the intervals become compressed to ~30% of the canvas instead of spanning
 * the full height.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/Layout/LayoutEngine.hpp"
#include "CorePlotting/Layout/LayoutTransform.hpp"
#include "CorePlotting/Layout/StackedLayoutStrategy.hpp"
#include "CorePlotting/Layout/NormalizationHelpers.hpp"
#include "CorePlotting/CoordinateTransform/SeriesMatrices.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace CorePlotting;
using Catch::Matchers::WithinAbs;

namespace {

/**
 * @brief Apply MVP transformation to a point and return NDC coordinates
 */
glm::vec4 transformToNDC(glm::vec4 const & point, 
                          glm::mat4 const & model,
                          glm::mat4 const & view,
                          glm::mat4 const & projection) {
    return projection * view * model * point;
}

/**
 * @brief Compose interval Y transform matching OpenGLWidget::composeIntervalYTransform
 */
LayoutTransform composeIntervalYTransform(
        SeriesLayout const & layout,
        float margin_factor,
        float global_zoom,
        float global_vertical_scale) {

    float const half_height = layout.y_transform.gain * margin_factor *
                              global_zoom * global_vertical_scale;
    float const center = layout.y_transform.offset;

    return LayoutTransform{center, half_height};
}

/**
 * @brief Compose analog Y transform matching OpenGLWidget::composeAnalogYTransform
 */
LayoutTransform composeAnalogYTransform(
        SeriesLayout const & layout,
        float data_mean,
        float std_dev,
        float intrinsic_scale,
        float user_scale_factor,
        float user_vertical_offset,
        float global_zoom,
        float global_vertical_scale) {

    auto data_norm = NormalizationHelpers::forStdDevRange(data_mean, std_dev, 3.0f);
    auto user_adj = NormalizationHelpers::manual(intrinsic_scale * user_scale_factor, user_vertical_offset);
    
    constexpr float margin_factor = 0.8f;
    LayoutTransform layout_with_margin{
            layout.y_transform.offset,
            layout.y_transform.gain * margin_factor};

    auto global_adj = NormalizationHelpers::manual(global_zoom * global_vertical_scale, 0.0f);
    return global_adj.compose(layout_with_margin.compose(user_adj.compose(data_norm)));
}

} // anonymous namespace


// ============================================================================
// LayoutTransform Basic Tests
// ============================================================================

TEST_CASE("LayoutTransform - compose preserves identity", "[CorePlotting][LayoutTransform]") {
    LayoutTransform identity;  // offset=0, gain=1
    LayoutTransform other{5.0f, 2.0f};  // offset=5, gain=2
    
    auto result = identity.compose(other);
    
    // identity.compose(other) = other
    // Because: (x * 1 + 0) * 2 + 5 = x * 2 + 5
    // Should give same result as just applying other
    REQUIRE_THAT(result.offset, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(result.gain, WithinAbs(2.0f, 0.001f));
}

TEST_CASE("LayoutTransform - compose applies in correct order", "[CorePlotting][LayoutTransform]") {
    // a.compose(b) means: apply b first, then a
    // Result: (x * b.gain + b.offset) * a.gain + a.offset
    //       = x * (b.gain * a.gain) + (b.offset * a.gain + a.offset)
    
    LayoutTransform a{10.0f, 3.0f};  // scale by 3, then add 10
    LayoutTransform b{2.0f, 2.0f};   // scale by 2, then add 2
    
    auto result = a.compose(b);
    
    // Expected: x * (2*3) + (2*3 + 10) = x * 6 + 16
    REQUIRE_THAT(result.gain, WithinAbs(6.0f, 0.001f));
    REQUIRE_THAT(result.offset, WithinAbs(16.0f, 0.001f));
    
    // Verify by applying
    float test_value = 1.0f;
    float direct = a.apply(b.apply(test_value));  // b first, then a
    float composed = result.apply(test_value);
    REQUIRE_THAT(direct, WithinAbs(composed, 0.001f));
}

TEST_CASE("LayoutTransform - toModelMatrixY produces correct scaling", "[CorePlotting][LayoutTransform]") {
    LayoutTransform transform{0.5f, 0.25f};  // center=0.5, half_height=0.25
    glm::mat4 model = transform.toModelMatrixY();
    
    // Y scale should be gain
    REQUIRE_THAT(model[1][1], WithinAbs(0.25f, 0.001f));
    // Y translation should be offset
    REQUIRE_THAT(model[3][1], WithinAbs(0.5f, 0.001f));
    // X should be identity
    REQUIRE_THAT(model[0][0], WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(model[3][0], WithinAbs(0.0f, 0.001f));
}


// ============================================================================
// Full-Canvas Interval Coverage Tests
// ============================================================================

TEST_CASE("Intervals span full NDC when alone", "[CorePlotting][MVPComposition]") {
    // Setup: Single interval series, full canvas
    LayoutRequest request;
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.series = {
        {"intervals", SeriesType::DigitalInterval, false}
    };
    
    StackedLayoutStrategy strategy;
    LayoutResponse layout = strategy.compute(request);
    
    auto const* interval_layout = layout.findLayout("intervals");
    REQUIRE(interval_layout != nullptr);
    
    // Compose transform with typical parameters
    LayoutTransform y_transform = composeIntervalYTransform(
        *interval_layout, 
        0.95f,  // margin_factor
        1.0f,   // global_zoom
        1.0f    // global_vertical_scale
    );
    
    // Create model matrix
    glm::mat4 model = y_transform.toModelMatrixY();
    glm::mat4 view(1.0f);  // Identity view
    glm::mat4 projection = glm::ortho(0.0f, 1000.0f, -1.0f, 1.0f);  // Time [0,1000], Y [-1,1]
    
    // Interval in local space spans [-1, 1] in Y
    glm::vec4 bottom_point{500.0f, -1.0f, 0.0f, 1.0f};
    glm::vec4 top_point{500.0f, 1.0f, 0.0f, 1.0f};
    
    auto ndc_bottom = transformToNDC(bottom_point, model, view, projection);
    auto ndc_top = transformToNDC(top_point, model, view, projection);
    
    // After transformation, should span approximately [-0.95, 0.95] (with margin)
    REQUIRE_THAT(ndc_bottom.y, WithinAbs(-0.95f, 0.01f));
    REQUIRE_THAT(ndc_top.y, WithinAbs(0.95f, 0.01f));
}

TEST_CASE("Intervals span full NDC when analog series are present", "[CorePlotting][MVPComposition]") {
    // This is the key test for the bug:
    // When analog series are added alongside intervals, intervals should still span full canvas
    
    LayoutRequest request;
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.series = {
        {"analog1", SeriesType::Analog, true},       // Stackable
        {"analog2", SeriesType::Analog, true},       // Stackable
        {"intervals", SeriesType::DigitalInterval, false}  // Full canvas
    };
    
    StackedLayoutStrategy strategy;
    LayoutResponse layout = strategy.compute(request);
    
    SECTION("Analog series divide viewport equally") {
        auto const* analog1_layout = layout.findLayout("analog1");
        auto const* analog2_layout = layout.findLayout("analog2");
        REQUIRE(analog1_layout != nullptr);
        REQUIRE(analog2_layout != nullptr);
        
        // Each analog should get height = 2.0 / 2 = 1.0
        float analog1_height = analog1_layout->y_transform.gain * 2.0f;
        float analog2_height = analog2_layout->y_transform.gain * 2.0f;
        REQUIRE_THAT(analog1_height, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(analog2_height, WithinAbs(1.0f, 0.001f));
    }
    
    SECTION("Interval series spans entire viewport") {
        auto const* interval_layout = layout.findLayout("intervals");
        REQUIRE(interval_layout != nullptr);
        
        // Interval should get full height = 2.0
        float interval_height = interval_layout->y_transform.gain * 2.0f;
        REQUIRE_THAT(interval_height, WithinAbs(2.0f, 0.001f));
        
        // Interval should be centered at 0
        REQUIRE_THAT(interval_layout->y_transform.offset, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Interval MVP produces full NDC coverage") {
        auto const* interval_layout = layout.findLayout("intervals");
        REQUIRE(interval_layout != nullptr);
        
        // Compose transform exactly like OpenGLWidget does
        LayoutTransform y_transform = composeIntervalYTransform(
            *interval_layout, 
            0.95f,  // margin_factor from DigitalIntervalSeriesDisplayOptions
            1.0f,   // global_zoom default
            1.0f    // global_vertical_scale default
        );
        
        // Create MVP matrices
        glm::mat4 model = y_transform.toModelMatrixY();
        glm::mat4 view(1.0f);
        glm::mat4 projection = glm::ortho(0.0f, 1000.0f, -1.0f, 1.0f);
        
        // Test points: intervals in local space span [-1, 1]
        glm::vec4 bottom_point{500.0f, -1.0f, 0.0f, 1.0f};
        glm::vec4 top_point{500.0f, 1.0f, 0.0f, 1.0f};
        
        auto ndc_bottom = transformToNDC(bottom_point, model, view, projection);
        auto ndc_top = transformToNDC(top_point, model, view, projection);
        
        // With margin_factor=0.95, should span [-0.95, 0.95]
        REQUIRE_THAT(ndc_bottom.y, WithinAbs(-0.95f, 0.01f));
        REQUIRE_THAT(ndc_top.y, WithinAbs(0.95f, 0.01f));
    }
    
    SECTION("Analog MVP produces correct stacked coverage") {
        auto const* analog1_layout = layout.findLayout("analog1");
        REQUIRE(analog1_layout != nullptr);
        
        // Compose transform exactly like OpenGLWidget does
        LayoutTransform y_transform = composeAnalogYTransform(
            *analog1_layout,
            0.0f,   // data_mean
            1.0f,   // std_dev
            1.0f,   // intrinsic_scale
            1.0f,   // user_scale_factor
            0.0f,   // user_vertical_offset
            1.0f,   // global_zoom
            1.0f    // global_vertical_scale
        );
        
        // Create MVP matrices
        glm::mat4 model = y_transform.toModelMatrixY();
        glm::mat4 view(1.0f);
        glm::mat4 projection = glm::ortho(0.0f, 1000.0f, -1.0f, 1.0f);
        
        // Analog data at ±3 std_dev should map to top/bottom of allocated space
        // For analog1: allocated center=-0.5, height=1.0 (covers [-1, 0])
        glm::vec4 low_point{500.0f, -3.0f, 0.0f, 1.0f};  // Data at -3 std_dev
        glm::vec4 high_point{500.0f, 3.0f, 0.0f, 1.0f};  // Data at +3 std_dev
        
        auto ndc_low = transformToNDC(low_point, model, view, projection);
        auto ndc_high = transformToNDC(high_point, model, view, projection);
        
        // analog1 should be in lower half of canvas: [-1, 0]
        // With 0.8 margin factor: should span approx [-0.9, -0.1]
        REQUIRE(ndc_low.y < ndc_high.y);
        REQUIRE(ndc_low.y > -1.0f);
        REQUIRE(ndc_high.y < 0.5f);  // Should stay in lower half
    }
}

TEST_CASE("Multiple intervals stay full canvas when analog series added", "[CorePlotting][MVPComposition]") {
    // Test with multiple interval series
    LayoutRequest request;
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.series = {
        {"analog1", SeriesType::Analog, true},
        {"intervals1", SeriesType::DigitalInterval, false},
        {"analog2", SeriesType::Analog, true},
        {"intervals2", SeriesType::DigitalInterval, false}
    };
    
    StackedLayoutStrategy strategy;
    LayoutResponse layout = strategy.compute(request);
    
    // Both interval series should have full height
    auto const* int1 = layout.findLayout("intervals1");
    auto const* int2 = layout.findLayout("intervals2");
    REQUIRE(int1 != nullptr);
    REQUIRE(int2 != nullptr);
    
    REQUIRE_THAT(int1->y_transform.gain * 2.0f, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(int2->y_transform.gain * 2.0f, WithinAbs(2.0f, 0.001f));
    
    // Both should be centered
    REQUIRE_THAT(int1->y_transform.offset, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(int2->y_transform.offset, WithinAbs(0.0f, 0.001f));
}


// ============================================================================
// Global Zoom/Scale Tests
// ============================================================================

TEST_CASE("Global zoom affects interval coverage correctly", "[CorePlotting][MVPComposition]") {
    LayoutRequest request;
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.series = {
        {"intervals", SeriesType::DigitalInterval, false}
    };
    
    StackedLayoutStrategy strategy;
    LayoutResponse layout = strategy.compute(request);
    auto const* interval_layout = layout.findLayout("intervals");
    REQUIRE(interval_layout != nullptr);
    
    SECTION("Global zoom 1.0 maintains full coverage") {
        LayoutTransform y_transform = composeIntervalYTransform(
            *interval_layout, 0.95f, 1.0f, 1.0f);
        
        glm::mat4 model = y_transform.toModelMatrixY();
        glm::mat4 view(1.0f);
        glm::mat4 projection = glm::ortho(0.0f, 1000.0f, -1.0f, 1.0f);
        
        glm::vec4 top{500.0f, 1.0f, 0.0f, 1.0f};
        auto ndc_top = transformToNDC(top, model, view, projection);
        
        REQUIRE_THAT(ndc_top.y, WithinAbs(0.95f, 0.01f));
    }
    
    SECTION("Global zoom 0.5 halves coverage") {
        LayoutTransform y_transform = composeIntervalYTransform(
            *interval_layout, 0.95f, 0.5f, 1.0f);
        
        glm::mat4 model = y_transform.toModelMatrixY();
        glm::mat4 view(1.0f);
        glm::mat4 projection = glm::ortho(0.0f, 1000.0f, -1.0f, 1.0f);
        
        glm::vec4 top{500.0f, 1.0f, 0.0f, 1.0f};
        auto ndc_top = transformToNDC(top, model, view, projection);
        
        // With zoom 0.5: 0.95 * 0.5 = 0.475
        REQUIRE_THAT(ndc_top.y, WithinAbs(0.475f, 0.01f));
    }
}


// ============================================================================
// Viewport Pan Tests  
// ============================================================================

TEST_CASE("Vertical pan offset shifts content correctly", "[CorePlotting][MVPComposition]") {
    LayoutRequest request;
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.series = {
        {"intervals", SeriesType::DigitalInterval, false}
    };
    
    StackedLayoutStrategy strategy;
    LayoutResponse layout = strategy.compute(request);
    auto const* interval_layout = layout.findLayout("intervals");
    REQUIRE(interval_layout != nullptr);
    
    LayoutTransform y_transform = composeIntervalYTransform(
        *interval_layout, 1.0f, 1.0f, 1.0f);  // No margin for simpler math
    
    glm::mat4 model = y_transform.toModelMatrixY();
    glm::mat4 projection = glm::ortho(0.0f, 1000.0f, -1.0f, 1.0f);
    
    SECTION("No pan - center at origin") {
        glm::mat4 view(1.0f);
        glm::vec4 center{500.0f, 0.0f, 0.0f, 1.0f};
        auto ndc = transformToNDC(center, model, view, projection);
        REQUIRE_THAT(ndc.y, WithinAbs(0.0f, 0.01f));
    }
    
    SECTION("Pan up by 0.5 - content shifts down in NDC") {
        glm::mat4 view(1.0f);
        view[3][1] = 0.5f;  // Pan up
        
        glm::vec4 center{500.0f, 0.0f, 0.0f, 1.0f};
        auto ndc = transformToNDC(center, model, view, projection);
        
        // Note: intervals are viewport-pinned in some implementations
        // This test verifies the view matrix effect
        REQUIRE_THAT(ndc.y, WithinAbs(0.5f, 0.01f));
    }
}


// ============================================================================
// Regression Tests
// ============================================================================

TEST_CASE("REGRESSION: Adding analog series should not compress intervals", 
          "[CorePlotting][MVPComposition][Regression]") {
    // This test explicitly captures the reported bug:
    // Intervals that previously spanned full canvas become compressed to ~30%
    // when analog series are added
    
    // Step 1: Layout with only intervals
    LayoutRequest request_intervals_only;
    request_intervals_only.viewport_y_min = -1.0f;
    request_intervals_only.viewport_y_max = 1.0f;
    request_intervals_only.series = {
        {"intervals", SeriesType::DigitalInterval, false}
    };
    
    StackedLayoutStrategy strategy;
    LayoutResponse layout_alone = strategy.compute(request_intervals_only);
    
    // Step 2: Layout with intervals + analog
    LayoutRequest request_with_analog;
    request_with_analog.viewport_y_min = -1.0f;
    request_with_analog.viewport_y_max = 1.0f;
    request_with_analog.series = {
        {"analog1", SeriesType::Analog, true},
        {"intervals", SeriesType::DigitalInterval, false}
    };
    
    LayoutResponse layout_with_analog = strategy.compute(request_with_analog);
    
    // Get interval layouts from both
    auto const* interval_alone = layout_alone.findLayout("intervals");
    auto const* interval_with_analog = layout_with_analog.findLayout("intervals");
    REQUIRE(interval_alone != nullptr);
    REQUIRE(interval_with_analog != nullptr);
    
    // CRITICAL ASSERTION: Interval layout should be IDENTICAL regardless of analog presence
    REQUIRE_THAT(interval_alone->y_transform.gain, 
                 WithinAbs(interval_with_analog->y_transform.gain, 0.001f));
    REQUIRE_THAT(interval_alone->y_transform.offset,
                 WithinAbs(interval_with_analog->y_transform.offset, 0.001f));
    
    // Both should have full height (gain = 1.0 = half-height)
    REQUIRE_THAT(interval_alone->y_transform.gain, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(interval_with_analog->y_transform.gain, WithinAbs(1.0f, 0.001f));
    
    // Now test the full MVP chain
    float const margin_factor = 0.95f;
    float const global_zoom = 1.0f;
    float const global_vertical_scale = 1.0f;
    
    LayoutTransform transform_alone = composeIntervalYTransform(
        *interval_alone, margin_factor, global_zoom, global_vertical_scale);
    LayoutTransform transform_with_analog = composeIntervalYTransform(
        *interval_with_analog, margin_factor, global_zoom, global_vertical_scale);
    
    // Composed transforms should also be identical
    REQUIRE_THAT(transform_alone.gain, WithinAbs(transform_with_analog.gain, 0.001f));
    REQUIRE_THAT(transform_alone.offset, WithinAbs(transform_with_analog.offset, 0.001f));
    
    // Final NDC coverage check
    glm::mat4 model_alone = transform_alone.toModelMatrixY();
    glm::mat4 model_with_analog = transform_with_analog.toModelMatrixY();
    glm::mat4 view(1.0f);
    glm::mat4 projection = glm::ortho(0.0f, 1000.0f, -1.0f, 1.0f);
    
    glm::vec4 top{500.0f, 1.0f, 0.0f, 1.0f};
    glm::vec4 bottom{500.0f, -1.0f, 0.0f, 1.0f};
    
    auto ndc_top_alone = transformToNDC(top, model_alone, view, projection);
    auto ndc_bottom_alone = transformToNDC(bottom, model_alone, view, projection);
    auto ndc_top_with_analog = transformToNDC(top, model_with_analog, view, projection);
    auto ndc_bottom_with_analog = transformToNDC(bottom, model_with_analog, view, projection);
    
    // Both should have ~95% coverage (with margin factor)
    float coverage_alone = ndc_top_alone.y - ndc_bottom_alone.y;
    float coverage_with_analog = ndc_top_with_analog.y - ndc_bottom_with_analog.y;
    
    REQUIRE_THAT(coverage_alone, WithinAbs(1.9f, 0.02f));  // ~95% of 2.0 = 1.9
    REQUIRE_THAT(coverage_with_analog, WithinAbs(1.9f, 0.02f));
    
    // If the bug exists, coverage_with_analog would be ~0.6 (30% of 2.0)
    // This assertion would catch it:
    REQUIRE_THAT(coverage_with_analog, WithinAbs(coverage_alone, 0.01f));
}

