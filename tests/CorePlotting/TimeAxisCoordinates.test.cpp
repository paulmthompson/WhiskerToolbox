#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/CoordinateTransform/TimeAxisCoordinates.hpp"

using namespace CorePlotting;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("TimeAxisParams construction", "[CorePlotting][TimeAxisCoordinates]") {
    SECTION("Default constructor") {
        TimeAxisParams params;
        REQUIRE(params.time_start == 0);
        REQUIRE(params.time_end == 0);
        REQUIRE(params.viewport_width_px == 1);
    }
    
    SECTION("Explicit value constructor") {
        TimeAxisParams params(100, 500, 800);
        REQUIRE(params.time_start == 100);
        REQUIRE(params.time_end == 500);
        REQUIRE(params.viewport_width_px == 800);
    }
    
    SECTION("TimeSeriesViewState constructor") {
        TimeSeriesViewState view_state;
        view_state.setTimeRange(100, 500);
        TimeAxisParams params(view_state, 800);
        REQUIRE(params.time_start == 100);
        REQUIRE(params.time_end == 500);
        REQUIRE(params.viewport_width_px == 800);
    }
    
    SECTION("getTimeSpan") {
        TimeAxisParams params(100, 500, 800);
        REQUIRE(params.getTimeSpan() == 400);
    }
}

TEST_CASE("canvasXToTime conversion", "[CorePlotting][TimeAxisCoordinates]") {
    SECTION("Simple range [0, 1000] with 800px canvas") {
        TimeAxisParams params(0, 1000, 800);
        
        // Left edge -> time 0
        REQUIRE_THAT(canvasXToTime(0.0f, params), WithinAbs(0.0f, 0.001f));
        
        // Right edge -> time 1000
        REQUIRE_THAT(canvasXToTime(800.0f, params), WithinAbs(1000.0f, 0.001f));
        
        // Middle -> time 500
        REQUIRE_THAT(canvasXToTime(400.0f, params), WithinAbs(500.0f, 0.001f));
        
        // Quarter -> time 250
        REQUIRE_THAT(canvasXToTime(200.0f, params), WithinAbs(250.0f, 0.001f));
    }
    
    SECTION("Offset range [100, 200]") {
        TimeAxisParams params(100, 200, 500);
        
        // Left edge -> time 100
        REQUIRE_THAT(canvasXToTime(0.0f, params), WithinAbs(100.0f, 0.001f));
        
        // Right edge -> time 200
        REQUIRE_THAT(canvasXToTime(500.0f, params), WithinAbs(200.0f, 0.001f));
        
        // Middle -> time 150
        REQUIRE_THAT(canvasXToTime(250.0f, params), WithinAbs(150.0f, 0.001f));
    }
    
    SECTION("Zoomed in range") {
        // Zoomed into [500, 600] of a larger dataset
        TimeAxisParams params(500, 600, 1000);
        
        REQUIRE_THAT(canvasXToTime(0.0f, params), WithinAbs(500.0f, 0.001f));
        REQUIRE_THAT(canvasXToTime(1000.0f, params), WithinAbs(600.0f, 0.001f));
        REQUIRE_THAT(canvasXToTime(500.0f, params), WithinAbs(550.0f, 0.001f));
    }
    
    SECTION("Negative values - canvas position before left edge") {
        TimeAxisParams params(0, 1000, 800);
        // -100 pixels should give negative time
        REQUIRE_THAT(canvasXToTime(-100.0f, params), WithinAbs(-125.0f, 0.001f));
    }
    
    SECTION("Edge case: zero width viewport") {
        TimeAxisParams params(0, 1000, 0);
        // Should return time_start to avoid division by zero
        REQUIRE_THAT(canvasXToTime(100.0f, params), WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("timeToCanvasX conversion", "[CorePlotting][TimeAxisCoordinates]") {
    SECTION("Simple range [0, 1000] with 800px canvas") {
        TimeAxisParams params(0, 1000, 800);
        
        // time 0 -> left edge
        REQUIRE_THAT(timeToCanvasX(0.0f, params), WithinAbs(0.0f, 0.001f));
        
        // time 1000 -> right edge
        REQUIRE_THAT(timeToCanvasX(1000.0f, params), WithinAbs(800.0f, 0.001f));
        
        // time 500 -> middle
        REQUIRE_THAT(timeToCanvasX(500.0f, params), WithinAbs(400.0f, 0.001f));
    }
    
    SECTION("Offset range [100, 200]") {
        TimeAxisParams params(100, 200, 500);
        
        REQUIRE_THAT(timeToCanvasX(100.0f, params), WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(timeToCanvasX(200.0f, params), WithinAbs(500.0f, 0.001f));
        REQUIRE_THAT(timeToCanvasX(150.0f, params), WithinAbs(250.0f, 0.001f));
    }
    
    SECTION("Time outside visible range") {
        TimeAxisParams params(100, 200, 500);
        
        // Before visible range -> negative X
        REQUIRE_THAT(timeToCanvasX(50.0f, params), WithinAbs(-250.0f, 0.001f));
        
        // After visible range -> X > width
        REQUIRE_THAT(timeToCanvasX(250.0f, params), WithinAbs(750.0f, 0.001f));
    }
    
    SECTION("Edge case: zero time span") {
        TimeAxisParams params(100, 100, 500);
        // Should return 0 to avoid division by zero
        REQUIRE_THAT(timeToCanvasX(100.0f, params), WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("Round-trip conversions canvasX <-> time", "[CorePlotting][TimeAxisCoordinates]") {
    TimeAxisParams params(0, 1000, 800);
    
    SECTION("canvas -> time -> canvas") {
        for (float x : {0.0f, 100.0f, 400.0f, 800.0f}) {
            float time = canvasXToTime(x, params);
            float back = timeToCanvasX(time, params);
            REQUIRE_THAT(back, WithinAbs(x, 0.001f));
        }
    }
    
    SECTION("time -> canvas -> time") {
        for (float t : {0.0f, 250.0f, 500.0f, 1000.0f}) {
            float canvas = timeToCanvasX(t, params);
            float back = canvasXToTime(canvas, params);
            REQUIRE_THAT(back, WithinAbs(t, 0.001f));
        }
    }
}

TEST_CASE("timeToNDC conversion", "[CorePlotting][TimeAxisCoordinates]") {
    SECTION("Basic range mapping") {
        TimeAxisParams params(100, 200, 800); // viewport width ignored for NDC
        
        // Start of range -> -1
        REQUIRE_THAT(timeToNDC(100.0f, params), WithinAbs(-1.0f, 0.001f));
        
        // End of range -> +1
        REQUIRE_THAT(timeToNDC(200.0f, params), WithinAbs(1.0f, 0.001f));
        
        // Middle -> 0
        REQUIRE_THAT(timeToNDC(150.0f, params), WithinAbs(0.0f, 0.001f));
        
        // Quarter -> -0.5
        REQUIRE_THAT(timeToNDC(125.0f, params), WithinAbs(-0.5f, 0.001f));
        
        // Three quarters -> 0.5
        REQUIRE_THAT(timeToNDC(175.0f, params), WithinAbs(0.5f, 0.001f));
    }
    
    SECTION("Time outside visible range") {
        TimeAxisParams params(100, 200, 800);
        
        // Before range
        REQUIRE_THAT(timeToNDC(50.0f, params), WithinAbs(-2.0f, 0.001f));
        
        // After range
        REQUIRE_THAT(timeToNDC(250.0f, params), WithinAbs(2.0f, 0.001f));
    }
    
    SECTION("Edge case: zero time span") {
        TimeAxisParams params(100, 100, 800);
        REQUIRE_THAT(timeToNDC(100.0f, params), WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("ndcToTime conversion", "[CorePlotting][TimeAxisCoordinates]") {
    SECTION("Basic range mapping") {
        TimeAxisParams params(100, 200, 800);
        
        // -1 -> start of range
        REQUIRE_THAT(ndcToTime(-1.0f, params), WithinAbs(100.0f, 0.001f));
        
        // +1 -> end of range
        REQUIRE_THAT(ndcToTime(1.0f, params), WithinAbs(200.0f, 0.001f));
        
        // 0 -> middle
        REQUIRE_THAT(ndcToTime(0.0f, params), WithinAbs(150.0f, 0.001f));
    }
}

TEST_CASE("Round-trip NDC conversions", "[CorePlotting][TimeAxisCoordinates]") {
    TimeAxisParams params(100, 200, 800);
    
    SECTION("time -> NDC -> time") {
        for (float t : {100.0f, 125.0f, 150.0f, 175.0f, 200.0f}) {
            float ndc = timeToNDC(t, params);
            float back = ndcToTime(ndc, params);
            REQUIRE_THAT(back, WithinAbs(t, 0.001f));
        }
    }
    
    SECTION("NDC -> time -> NDC") {
        for (float ndc : {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f}) {
            float time = ndcToTime(ndc, params);
            float back = timeToNDC(time, params);
            REQUIRE_THAT(back, WithinAbs(ndc, 0.001f));
        }
    }
}

TEST_CASE("pixelsPerTimeUnit and timeUnitsPerPixel", "[CorePlotting][TimeAxisCoordinates]") {
    SECTION("Basic calculation") {
        TimeAxisParams params(0, 100, 500); // 100 time units over 500 pixels
        
        // 500 pixels / 100 time units = 5 pixels per time unit
        REQUIRE_THAT(pixelsPerTimeUnit(params), WithinAbs(5.0f, 0.001f));
        
        // 100 time units / 500 pixels = 0.2 time units per pixel
        REQUIRE_THAT(timeUnitsPerPixel(params), WithinAbs(0.2f, 0.001f));
    }
    
    SECTION("Zoomed in (fewer time units over same pixels)") {
        TimeAxisParams params(500, 510, 500); // 10 time units over 500 pixels
        
        // 500 pixels / 10 time units = 50 pixels per time unit
        REQUIRE_THAT(pixelsPerTimeUnit(params), WithinAbs(50.0f, 0.001f));
        
        // 10 time units / 500 pixels = 0.02 time units per pixel
        REQUIRE_THAT(timeUnitsPerPixel(params), WithinAbs(0.02f, 0.001f));
    }
    
    SECTION("Zoomed out (more time units over same pixels)") {
        TimeAxisParams params(0, 10000, 500); // 10000 time units over 500 pixels
        
        // 500 pixels / 10000 time units = 0.05 pixels per time unit
        REQUIRE_THAT(pixelsPerTimeUnit(params), WithinAbs(0.05f, 0.001f));
        
        // 10000 time units / 500 pixels = 20 time units per pixel
        REQUIRE_THAT(timeUnitsPerPixel(params), WithinAbs(20.0f, 0.001f));
    }
    
    SECTION("Inverse relationship") {
        TimeAxisParams params(0, 1000, 800);
        
        float ppt = pixelsPerTimeUnit(params);
        float tpp = timeUnitsPerPixel(params);
        
        // Should be inverses of each other
        REQUIRE_THAT(ppt * tpp, WithinAbs(1.0f, 0.001f));
    }
    
    SECTION("Edge cases") {
        // Zero time span
        TimeAxisParams zero_span(100, 100, 500);
        REQUIRE_THAT(pixelsPerTimeUnit(zero_span), WithinAbs(0.0f, 0.001f));
        
        // Zero viewport width
        TimeAxisParams zero_width(0, 100, 0);
        REQUIRE_THAT(timeUnitsPerPixel(zero_width), WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("makeTimeAxisParams helper", "[CorePlotting][TimeAxisCoordinates]") {
    TimeSeriesViewState view_state;
    view_state.setTimeRange(100, 500);
    auto params = makeTimeAxisParams(view_state, 800);
    
    REQUIRE(params.time_start == 100);
    REQUIRE(params.time_end == 500);
    REQUIRE(params.viewport_width_px == 800);
}

TEST_CASE("Practical scenario: mouse hover conversion", "[CorePlotting][TimeAxisCoordinates]") {
    // Simulate a DataViewer scenario:
    // - Viewing time range [1000, 2000] (zoomed in to 1000 frames)
    // - Canvas width is 1200 pixels
    // - Mouse is at pixel 600 (center of canvas)
    
    TimeAxisParams params(1000, 2000, 1200);
    
    float mouse_x = 600.0f;
    float hover_time = canvasXToTime(mouse_x, params);
    
    // At center of canvas, should be center of time range (1500)
    REQUIRE_THAT(hover_time, WithinAbs(1500.0f, 0.001f));
    
    // The inverse should give us back our mouse position
    float back_to_pixel = timeToCanvasX(hover_time, params);
    REQUIRE_THAT(back_to_pixel, WithinAbs(600.0f, 0.001f));
}

TEST_CASE("Practical scenario: hit tolerance calculation", "[CorePlotting][TimeAxisCoordinates]") {
    // Scenario: User clicks within 5 pixels of an event
    // We need to convert "5 pixel tolerance" to time tolerance
    
    TimeAxisParams params(0, 1000, 800);
    
    float pixel_tolerance = 5.0f;
    float time_tolerance = pixel_tolerance * timeUnitsPerPixel(params);
    
    // 1000 time units / 800 pixels = 1.25 time units per pixel
    // 5 pixels * 1.25 = 6.25 time units
    REQUIRE_THAT(time_tolerance, WithinAbs(6.25f, 0.001f));
}

// ============================================================================
// Y-Axis Coordinate Tests
// ============================================================================

TEST_CASE("YAxisParams construction", "[CorePlotting][TimeAxisCoordinates]") {
    SECTION("Default constructor") {
        YAxisParams params;
        REQUIRE_THAT(params.world_y_min, WithinAbs(-1.0f, 0.001f));
        REQUIRE_THAT(params.world_y_max, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(params.pan_offset, WithinAbs(0.0f, 0.001f));
        REQUIRE(params.viewport_height_px == 1);
    }
    
    SECTION("Explicit value constructor") {
        YAxisParams params(-2.0f, 2.0f, 600, 0.5f);
        REQUIRE_THAT(params.world_y_min, WithinAbs(-2.0f, 0.001f));
        REQUIRE_THAT(params.world_y_max, WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(params.pan_offset, WithinAbs(0.5f, 0.001f));
        REQUIRE(params.viewport_height_px == 600);
    }
    
    SECTION("getEffectiveRange with pan offset") {
        YAxisParams params(-1.0f, 1.0f, 600, 0.5f);
        auto [y_min, y_max] = params.getEffectiveRange();
        REQUIRE_THAT(y_min, WithinAbs(-0.5f, 0.001f));
        REQUIRE_THAT(y_max, WithinAbs(1.5f, 0.001f));
    }
}

TEST_CASE("canvasYToWorldY conversion", "[CorePlotting][TimeAxisCoordinates]") {
    SECTION("Simple range [-1, 1] with 600px canvas") {
        YAxisParams params(-1.0f, 1.0f, 600);
        
        // Top of canvas (y=0) -> world_y_max (1.0)
        REQUIRE_THAT(canvasYToWorldY(0.0f, params), WithinAbs(1.0f, 0.001f));
        
        // Bottom of canvas (y=600) -> world_y_min (-1.0)
        REQUIRE_THAT(canvasYToWorldY(600.0f, params), WithinAbs(-1.0f, 0.001f));
        
        // Middle (y=300) -> 0.0
        REQUIRE_THAT(canvasYToWorldY(300.0f, params), WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Asymmetric range [0, 10]") {
        YAxisParams params(0.0f, 10.0f, 500);
        
        // Top -> 10.0
        REQUIRE_THAT(canvasYToWorldY(0.0f, params), WithinAbs(10.0f, 0.001f));
        
        // Bottom -> 0.0
        REQUIRE_THAT(canvasYToWorldY(500.0f, params), WithinAbs(0.0f, 0.001f));
        
        // Middle -> 5.0
        REQUIRE_THAT(canvasYToWorldY(250.0f, params), WithinAbs(5.0f, 0.001f));
    }
    
    SECTION("With pan offset") {
        YAxisParams params(-1.0f, 1.0f, 600, 0.5f);  // Pan up by 0.5
        
        // Effective range is [-0.5, 1.5]
        // Top of canvas -> 1.5
        REQUIRE_THAT(canvasYToWorldY(0.0f, params), WithinAbs(1.5f, 0.001f));
        
        // Bottom of canvas -> -0.5
        REQUIRE_THAT(canvasYToWorldY(600.0f, params), WithinAbs(-0.5f, 0.001f));
        
        // Middle -> 0.5
        REQUIRE_THAT(canvasYToWorldY(300.0f, params), WithinAbs(0.5f, 0.001f));
    }
    
    SECTION("Edge case: zero height viewport") {
        YAxisParams params(-1.0f, 1.0f, 0);
        // Should return world_y_min to avoid division by zero
        REQUIRE_THAT(canvasYToWorldY(100.0f, params), WithinAbs(-1.0f, 0.001f));
    }
}

TEST_CASE("worldYToCanvasY conversion", "[CorePlotting][TimeAxisCoordinates]") {
    SECTION("Simple range [-1, 1] with 600px canvas") {
        YAxisParams params(-1.0f, 1.0f, 600);
        
        // world_y_max (1.0) -> top of canvas (0)
        REQUIRE_THAT(worldYToCanvasY(1.0f, params), WithinAbs(0.0f, 0.001f));
        
        // world_y_min (-1.0) -> bottom of canvas (600)
        REQUIRE_THAT(worldYToCanvasY(-1.0f, params), WithinAbs(600.0f, 0.001f));
        
        // 0.0 -> middle (300)
        REQUIRE_THAT(worldYToCanvasY(0.0f, params), WithinAbs(300.0f, 0.001f));
    }
}

TEST_CASE("Y-axis round-trip conversion", "[CorePlotting][TimeAxisCoordinates]") {
    YAxisParams params(-1.0f, 1.0f, 600, 0.25f);
    
    // Various canvas positions
    for (float canvas_y : {0.0f, 100.0f, 300.0f, 500.0f, 600.0f}) {
        float world_y = canvasYToWorldY(canvas_y, params);
        float back_to_canvas = worldYToCanvasY(world_y, params);
        REQUIRE_THAT(back_to_canvas, WithinAbs(canvas_y, 0.01f));
    }
    
    // Various world positions
    for (float world_y : {-0.75f, -0.25f, 0.0f, 0.5f, 1.25f}) {
        float canvas_y = worldYToCanvasY(world_y, params);
        float back_to_world = canvasYToWorldY(canvas_y, params);
        REQUIRE_THAT(back_to_world, WithinAbs(world_y, 0.001f));
    }
}

TEST_CASE("worldYToNDC conversion", "[CorePlotting][TimeAxisCoordinates]") {
    SECTION("Simple range") {
        YAxisParams params(-1.0f, 1.0f, 600);
        
        // world_y_min -> -1 NDC
        REQUIRE_THAT(worldYToNDC(-1.0f, params), WithinAbs(-1.0f, 0.001f));
        
        // world_y_max -> +1 NDC
        REQUIRE_THAT(worldYToNDC(1.0f, params), WithinAbs(1.0f, 0.001f));
        
        // Center -> 0 NDC
        REQUIRE_THAT(worldYToNDC(0.0f, params), WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("With pan offset") {
        YAxisParams params(-1.0f, 1.0f, 600, 0.5f);
        // Effective range is [-0.5, 1.5]
        
        // Bottom of effective range -> -1 NDC
        REQUIRE_THAT(worldYToNDC(-0.5f, params), WithinAbs(-1.0f, 0.001f));
        
        // Top of effective range -> +1 NDC
        REQUIRE_THAT(worldYToNDC(1.5f, params), WithinAbs(1.0f, 0.001f));
        
        // Center of effective range (0.5) -> 0 NDC
        REQUIRE_THAT(worldYToNDC(0.5f, params), WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("ndcToWorldY conversion", "[CorePlotting][TimeAxisCoordinates]") {
    SECTION("Simple range") {
        YAxisParams params(-1.0f, 1.0f, 600);
        
        REQUIRE_THAT(ndcToWorldY(-1.0f, params), WithinAbs(-1.0f, 0.001f));
        REQUIRE_THAT(ndcToWorldY(1.0f, params), WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(ndcToWorldY(0.0f, params), WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("Practical scenario: mouse hover Y conversion", "[CorePlotting][TimeAxisCoordinates]") {
    // Simulate a DataViewer scenario:
    // - World Y range [-1, 1] 
    // - Canvas height 600 pixels
    // - User panned up by 0.3
    // - Mouse is at pixel 150 (25% from top)
    
    YAxisParams params(-1.0f, 1.0f, 600, 0.3f);
    
    float mouse_y = 150.0f;
    float world_y = canvasYToWorldY(mouse_y, params);
    
    // Effective range is [-0.7, 1.3] (shifted up by 0.3)
    // At 25% from top: 1.3 - 0.25 * 2.0 = 1.3 - 0.5 = 0.8
    REQUIRE_THAT(world_y, WithinAbs(0.8f, 0.001f));
    
    // The inverse should give us back our mouse position
    float back_to_pixel = worldYToCanvasY(world_y, params);
    REQUIRE_THAT(back_to_pixel, WithinAbs(150.0f, 0.01f));
}
