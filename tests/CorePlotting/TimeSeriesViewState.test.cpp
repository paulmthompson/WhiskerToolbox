#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/CoordinateTransform/TimeRange.hpp"

using namespace CorePlotting;

TEST_CASE("TimeSeriesViewState default construction", "[CorePlotting][TimeSeriesViewState]") {
    SECTION("Default constructor creates standard view state") {
        TimeSeriesViewState state;
        
        REQUIRE(state.time_start == 0);
        REQUIRE(state.time_end == 1000);
        REQUIRE(state.getTimeWidth() == 1001);
        REQUIRE(state.y_min == -1.0f);
        REQUIRE(state.y_max == 1.0f);
        REQUIRE(state.vertical_pan_offset == 0.0f);
        REQUIRE(state.global_zoom == 1.0f);
        REQUIRE(state.global_vertical_scale == 1.0f);
    }
    
    SECTION("Construct with explicit time window") {
        TimeSeriesViewState state(100, 500);
        
        REQUIRE(state.time_start == 100);
        REQUIRE(state.time_end == 500);
        REQUIRE(state.getTimeWidth() == 401);
    }
}

TEST_CASE("TimeSeriesViewState time window methods", "[CorePlotting][TimeSeriesViewState]") {
    TimeSeriesViewState state;
    
    SECTION("getTimeWidth returns inclusive count") {
        state.time_start = 100;
        state.time_end = 200;
        REQUIRE(state.getTimeWidth() == 101); // 200 - 100 + 1
    }
    
    SECTION("getTimeCenter returns midpoint") {
        state.time_start = 100;
        state.time_end = 200;
        REQUIRE(state.getTimeCenter() == 150);
    }
    
    SECTION("getTimeCenter rounds down for odd widths") {
        state.time_start = 100;
        state.time_end = 199; // width = 100 (even)
        REQUIRE(state.getTimeCenter() == 149); // 100 + (199-100)/2 = 149
        
        state.time_end = 200; // width = 101 (odd)
        REQUIRE(state.getTimeCenter() == 150); // 100 + (200-100)/2 = 150
    }
    
    SECTION("setTimeWindow centers on point with specified width") {
        state.setTimeWindow(500, 200);
        
        // Center is 500, width is 200
        // half_width = 100, start = 500 - 100 = 400, end = 400 + 200 - 1 = 599
        REQUIRE(state.time_start == 400);
        REQUIRE(state.time_end == 599);
        REQUIRE(state.getTimeWidth() == 200);
    }
    
    SECTION("setTimeWindow enforces minimum width of 1") {
        state.setTimeWindow(500, 0);
        REQUIRE(state.getTimeWidth() == 1);
        
        state.setTimeWindow(500, -100);
        REQUIRE(state.getTimeWidth() == 1);
    }
    
    SECTION("setTimeWindow allows negative time values") {
        // No bounds enforcement - negative times are allowed
        state.setTimeWindow(-100, 200);
        REQUIRE(state.time_start == -200);
        REQUIRE(state.time_end == -1);
        REQUIRE(state.getTimeWidth() == 200);
    }
    
    SECTION("setTimeWindow allows times beyond any data range") {
        // No bounds enforcement - can set any range
        state.setTimeWindow(1000000, 500);
        REQUIRE(state.time_start == 999750);
        REQUIRE(state.time_end == 1000249);
        REQUIRE(state.getTimeWidth() == 500);
    }
    
    SECTION("setTimeRange sets explicit start and end") {
        state.setTimeRange(50, 150);
        REQUIRE(state.time_start == 50);
        REQUIRE(state.time_end == 150);
    }
    
    SECTION("setTimeRange handles inverted range") {
        state.setTimeRange(150, 50);
        REQUIRE(state.time_start == 50);
        REQUIRE(state.time_end == 150);
    }
}

TEST_CASE("TimeSeriesViewState Y-axis methods", "[CorePlotting][TimeSeriesViewState]") {
    TimeSeriesViewState state;
    
    SECTION("applyVerticalPanDelta adjusts offset") {
        REQUIRE(state.vertical_pan_offset == 0.0f);
        
        state.applyVerticalPanDelta(0.5f);
        REQUIRE(state.vertical_pan_offset == 0.5f);
        
        state.applyVerticalPanDelta(-0.3f);
        REQUIRE_THAT(state.vertical_pan_offset, Catch::Matchers::WithinAbs(0.2f, 0.0001f));
    }
    
    SECTION("resetVerticalPan resets to zero") {
        state.vertical_pan_offset = 0.5f;
        state.resetVerticalPan();
        REQUIRE(state.vertical_pan_offset == 0.0f);
    }
    
    SECTION("getEffectiveYBounds accounts for pan offset") {
        state.y_min = -1.0f;
        state.y_max = 1.0f;
        state.vertical_pan_offset = 0.0f;
        
        auto [y_min, y_max] = state.getEffectiveYBounds();
        REQUIRE(y_min == -1.0f);
        REQUIRE(y_max == 1.0f);
        
        state.vertical_pan_offset = 0.5f;
        auto [y_min2, y_max2] = state.getEffectiveYBounds();
        REQUIRE(y_min2 == -1.5f);
        REQUIRE(y_max2 == 0.5f);
    }
}

TEST_CASE("TimeSeriesViewState zoom scenarios", "[CorePlotting][TimeSeriesViewState]") {
    TimeSeriesViewState state;
    state.setTimeRange(0, 999); // 1000 frames visible
    
    SECTION("Zoom in by half") {
        int64_t center = state.getTimeCenter(); // 499
        int64_t new_width = state.getTimeWidth() / 2; // 500
        
        state.setTimeWindow(center, new_width);
        
        REQUIRE(state.getTimeWidth() == 500);
    }
    
    SECTION("Zoom out by double") {
        state.setTimeRange(400, 599); // width = 200
        int64_t center = state.getTimeCenter();
        int64_t new_width = state.getTimeWidth() * 2; // 400
        
        state.setTimeWindow(center, new_width);
        
        REQUIRE(state.getTimeWidth() == 400);
    }
    
    SECTION("Zoom out beyond original data - no clamping occurs") {
        // Unlike the old TimeRange, no bounds enforcement
        state.setTimeRange(400, 599);
        int64_t center = state.getTimeCenter();
        int64_t new_width = 2000;
        
        state.setTimeWindow(center, new_width);
        
        // No clamping - full requested width achieved
        REQUIRE(state.getTimeWidth() == 2000);
    }
}

TEST_CASE("TimeSeriesViewState architectural distinction", "[CorePlotting][TimeSeriesViewState]") {
    // These tests document the key architectural difference from ViewState:
    // TimeSeriesViewState is for real-time/streaming data where:
    // - X zoom requires buffer rebuild (changing visible data)
    // - Y zoom/pan is MVP-only (no buffer changes)
    // - No bounds enforcement (blank areas allowed)
    
    TimeSeriesViewState state;
    
    SECTION("No bounds enforcement allows viewing empty regions") {
        // Can set time window to any range, including negative times
        // or times far beyond expected data
        state.setTimeRange(-1000, -500);
        REQUIRE(state.time_start == -1000);
        REQUIRE(state.time_end == -500);
        
        // This allows scrolling to "blank" regions before data starts
        // The widget will simply not render data for those times
    }
    
    SECTION("Time window defines buffer scope") {
        // In real-time plotting, changing the time window
        // determines what data is loaded into GPU buffers
        // (not just what portion of pre-loaded data is visible)
        
        state.setTimeWindow(50000, 1000);
        
        // These values would be used to query data for the buffer
        REQUIRE(state.time_start == 49500);
        REQUIRE(state.time_end == 50499);
    }
    
    SECTION("Y-axis is MVP-only") {
        // Changing Y parameters doesn't change what data is loaded
        // Only affects the projection/view matrices
        
        state.y_min = -2.0f;
        state.y_max = 2.0f;
        state.vertical_pan_offset = 1.0f;
        state.global_vertical_scale = 0.5f;
        
        // These affect rendering, not data loading
        REQUIRE(state.y_min == -2.0f);
        REQUIRE(state.global_vertical_scale == 0.5f);
    }
}

TEST_CASE("TimeSeriesViewState large time values", "[CorePlotting][TimeSeriesViewState]") {
    SECTION("Handles 64-bit time values") {
        TimeSeriesViewState state;
        
        int64_t large = 1000000000000LL; // 1 trillion
        state.setTimeRange(large, large + 1000);
        
        REQUIRE(state.time_start == large);
        REQUIRE(state.time_end == large + 1000);
        REQUIRE(state.getTimeWidth() == 1001);
        REQUIRE(state.getTimeCenter() == large + 500);
    }
}
