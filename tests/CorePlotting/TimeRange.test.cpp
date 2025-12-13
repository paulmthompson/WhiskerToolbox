#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/CoordinateTransform/TimeRange.hpp"
#include "TimeFrame/TimeFrame.hpp"

using namespace CorePlotting;

TEST_CASE("TimeRange basic construction", "[CorePlotting][TimeRange]") {
    SECTION("Default constructor creates empty range") {
        TimeRange tr;
        REQUIRE(tr.start == 0);
        REQUIRE(tr.end == 0);
        REQUIRE(tr.min_bound == 0);
        REQUIRE(tr.max_bound == 0);
        REQUIRE(tr.getWidth() == 1);
    }
    
    SECTION("Explicit construction with valid range") {
        TimeRange tr(10, 20, 0, 100);
        REQUIRE(tr.start == 10);
        REQUIRE(tr.end == 20);
        REQUIRE(tr.min_bound == 0);
        REQUIRE(tr.max_bound == 100);
        REQUIRE(tr.getWidth() == 11); // inclusive count
    }
    
    SECTION("Construction auto-clamps invalid range to bounds") {
        TimeRange tr(-10, 150, 0, 100);
        REQUIRE(tr.start == 0);   // clamped to min_bound
        REQUIRE(tr.end == 100);   // clamped to max_bound
    }
    
    SECTION("Construction swaps inverted start/end") {
        TimeRange tr(20, 10, 0, 100);
        REQUIRE(tr.start == 10);
        REQUIRE(tr.end == 20);
    }
}

TEST_CASE("TimeRange::fromTimeFrame", "[CorePlotting][TimeRange]") {
    SECTION("Creates range from TimeFrame bounds") {
        std::vector<int> times = {0, 10, 20, 30, 40, 50};
        TimeFrame tf(times);
        
        TimeRange tr = TimeRange::fromTimeFrame(tf);
        
        REQUIRE(tr.start == 0);
        REQUIRE(tr.end == 5); // 6 frames = indices 0-5
        REQUIRE(tr.min_bound == 0);
        REQUIRE(tr.max_bound == 5);
        REQUIRE(tr.getWidth() == 6);
    }
    
    SECTION("Initial visible range spans entire TimeFrame") {
        std::vector<int> times(1000); // 1000 frames
        TimeFrame tf(times);
        
        TimeRange tr = TimeRange::fromTimeFrame(tf);
        
        REQUIRE(tr.getWidth() == 1000);
        REQUIRE(tr.getTotalBoundedWidth() == 1000);
    }
}

TEST_CASE("TimeRange::setVisibleRange", "[CorePlotting][TimeRange]") {
    TimeRange tr(0, 100, 0, 1000);
    
    SECTION("Sets valid range within bounds") {
        tr.setVisibleRange(50, 150);
        REQUIRE(tr.start == 50);
        REQUIRE(tr.end == 150);
        REQUIRE(tr.getWidth() == 101);
    }
    
    SECTION("Clamps range below min_bound") {
        tr.setVisibleRange(-50, 50);
        REQUIRE(tr.start == 0); // clamped to min_bound
        REQUIRE(tr.end == 50);
    }
    
    SECTION("Clamps range above max_bound") {
        tr.setVisibleRange(900, 1100);
        REQUIRE(tr.start == 900);
        REQUIRE(tr.end == 1000); // clamped to max_bound
    }
    
    SECTION("Clamps both sides if too wide") {
        tr.setVisibleRange(-100, 1100);
        REQUIRE(tr.start == 0);
        REQUIRE(tr.end == 1000);
    }
    
    SECTION("Handles inverted range") {
        tr.setVisibleRange(150, 50);
        REQUIRE(tr.start == 50);
        REQUIRE(tr.end == 150);
    }
}

TEST_CASE("TimeRange::setCenterAndZoom", "[CorePlotting][TimeRange]") {
    TimeRange tr(0, 100, 0, 1000);
    
    SECTION("Centers range on specified point") {
        int64_t actual_width = tr.setCenterAndZoom(500, 200);
        
        REQUIRE(actual_width == 200);
        // With even width (200), center is at 499 due to integer division
        // Range is [400, 599], center = 400 + (599-400)/2 = 400 + 99 = 499
        REQUIRE(tr.getCenter() == 499);
        REQUIRE(tr.start == 400); // 500 - 200/2
        REQUIRE(tr.end == 599);   // 400 + 200 - 1
    }
    
    SECTION("Clamps to min_bound when centering too low") {
        int64_t actual_width = tr.setCenterAndZoom(50, 200);
        
        REQUIRE(actual_width == 200);
        REQUIRE(tr.start == 0); // shifted to stay within bounds
        REQUIRE(tr.end == 199);
    }
    
    SECTION("Clamps to max_bound when centering too high") {
        int64_t actual_width = tr.setCenterAndZoom(950, 200);
        
        REQUIRE(actual_width == 200);
        REQUIRE(tr.start == 801); // shifted to stay within bounds
        REQUIRE(tr.end == 1000);
    }
    
    SECTION("Clamps width if larger than total bounds") {
        int64_t actual_width = tr.setCenterAndZoom(500, 2000);
        
        REQUIRE(actual_width == 1001); // full bounds width
        REQUIRE(tr.start == 0);
        REQUIRE(tr.end == 1000);
    }
    
    SECTION("Enforces minimum width of 1") {
        int64_t actual_width = tr.setCenterAndZoom(500, 0);
        
        REQUIRE(actual_width == 1);
        REQUIRE(tr.getWidth() == 1);
    }
    
    SECTION("Handles negative width request") {
        int64_t actual_width = tr.setCenterAndZoom(500, -100);
        
        REQUIRE(actual_width == 1);
        REQUIRE(tr.getWidth() == 1);
    }
}

TEST_CASE("TimeRange query methods", "[CorePlotting][TimeRange]") {
    TimeRange tr(100, 200, 0, 1000);
    
    SECTION("getWidth returns inclusive count") {
        REQUIRE(tr.getWidth() == 101); // 200 - 100 + 1
    }
    
    SECTION("getCenter returns midpoint") {
        REQUIRE(tr.getCenter() == 150);
    }
    
    SECTION("getCenter rounds down for odd widths") {
        tr.setVisibleRange(100, 199); // width = 100 (even)
        REQUIRE(tr.getCenter() == 149); // (100 + 199) / 2 = 149
        
        tr.setVisibleRange(100, 200); // width = 101 (odd)
        REQUIRE(tr.getCenter() == 150); // (100 + 200) / 2 = 150
    }
    
    SECTION("contains checks if time is in visible range") {
        REQUIRE(tr.contains(100) == true);  // at start
        REQUIRE(tr.contains(150) == true);  // in middle
        REQUIRE(tr.contains(200) == true);  // at end
        REQUIRE(tr.contains(99) == false);   // before start
        REQUIRE(tr.contains(201) == false);  // after end
    }
    
    SECTION("isAtMinBound detects lower bound limit") {
        REQUIRE(tr.isAtMinBound() == false);
        
        tr.setVisibleRange(0, 100);
        REQUIRE(tr.isAtMinBound() == true);
        
        tr.setVisibleRange(-10, 100); // should clamp to 0
        REQUIRE(tr.isAtMinBound() == true);
    }
    
    SECTION("isAtMaxBound detects upper bound limit") {
        REQUIRE(tr.isAtMaxBound() == false);
        
        tr.setVisibleRange(900, 1000);
        REQUIRE(tr.isAtMaxBound() == true);
        
        tr.setVisibleRange(900, 1100); // should clamp to 1000
        REQUIRE(tr.isAtMaxBound() == true);
    }
    
    SECTION("getTotalBoundedWidth returns full data range") {
        REQUIRE(tr.getTotalBoundedWidth() == 1001); // 1000 - 0 + 1
    }
}

TEST_CASE("TimeRange zoom scenarios", "[CorePlotting][TimeRange]") {
    TimeRange tr(0, 999, 0, 999); // 1000 frames
    
    SECTION("Zoom in by half") {
        int64_t center = tr.getCenter(); // 499
        int64_t new_width = tr.getWidth() / 2; // 500
        
        tr.setCenterAndZoom(center, new_width);
        
        REQUIRE(tr.getWidth() == 500);
        // With even width (500), centering at 499 gives range [249, 748]
        // center = 249 + (748-249)/2 = 249 + 249 = 498 (one less due to rounding)
        REQUIRE(tr.getCenter() == 498);
    }
    
    SECTION("Zoom out by double") {
        tr.setVisibleRange(400, 599); // width = 200, center = 499
        
        int64_t center = tr.getCenter();
        int64_t new_width = tr.getWidth() * 2; // 400
        
        tr.setCenterAndZoom(center, new_width);
        
        REQUIRE(tr.getWidth() == 400);
        // Centering at 499 with width 400 gives [299, 698]
        // center = 299 + (698-299)/2 = 299 + 199 = 498 (one less due to rounding)
        REQUIRE(tr.getCenter() == 498);
    }
    
    SECTION("Zoom out beyond bounds shows full range") {
        tr.setVisibleRange(400, 599);
        
        int64_t center = tr.getCenter();
        int64_t new_width = 2000; // larger than bounds
        
        int64_t actual_width = tr.setCenterAndZoom(center, new_width);
        
        REQUIRE(actual_width == 1000); // clamped to full bounds
        REQUIRE(tr.start == 0);
        REQUIRE(tr.end == 999);
    }
}

TEST_CASE("TimeRange scroll scenarios", "[CorePlotting][TimeRange]") {
    TimeRange tr(100, 199, 0, 1000); // width = 100
    
    SECTION("Scroll right within bounds") {
        int64_t width = tr.getWidth();
        tr.setVisibleRange(tr.start + 50, tr.end + 50);
        
        REQUIRE(tr.start == 150);
        REQUIRE(tr.end == 249);
        REQUIRE(tr.getWidth() == width); // width preserved
    }
    
    SECTION("Scroll left within bounds") {
        int64_t width = tr.getWidth();
        tr.setVisibleRange(tr.start - 50, tr.end - 50);
        
        REQUIRE(tr.start == 50);
        REQUIRE(tr.end == 149);
        REQUIRE(tr.getWidth() == width);
    }
    
    SECTION("Scroll right hits max_bound") {
        tr.setVisibleRange(900, 999); // at right edge
        tr.setVisibleRange(tr.start + 50, tr.end + 50); // try to scroll further
        
        REQUIRE(tr.end == 1000); // clamped to max_bound
        REQUIRE(tr.isAtMaxBound() == true);
    }
    
    SECTION("Scroll left hits min_bound") {
        tr.setVisibleRange(0, 99); // at left edge
        tr.setVisibleRange(tr.start - 50, tr.end - 50); // try to scroll further
        
        REQUIRE(tr.start == 0); // clamped to min_bound
        REQUIRE(tr.isAtMinBound() == true);
    }
}

TEST_CASE("TimeSeriesViewState construction", "[CorePlotting][TimeRange]") {
    SECTION("Default constructor") {
        TimeSeriesViewState state;
        
        REQUIRE(state.view_state.zoom_level_x == 1.0f);
        REQUIRE(state.view_state.zoom_level_y == 1.0f);
        REQUIRE(state.time_range.start == 0);
        REQUIRE(state.time_range.end == 0);
    }
    
    SECTION("Construct from TimeFrame") {
        std::vector<int> times(500);
        TimeFrame tf(times);
        
        TimeSeriesViewState state(tf);
        
        REQUIRE(state.time_range.start == 0);
        REQUIRE(state.time_range.end == 499);
        REQUIRE(state.time_range.getWidth() == 500);
        REQUIRE(state.time_range.getTotalBoundedWidth() == 500);
    }
}

TEST_CASE("TimeRange edge cases", "[CorePlotting][TimeRange]") {
    SECTION("Single-frame TimeFrame") {
        std::vector<int> times = {0};
        TimeFrame tf(times);
        
        TimeRange tr = TimeRange::fromTimeFrame(tf);
        
        REQUIRE(tr.start == 0);
        REQUIRE(tr.end == 0);
        REQUIRE(tr.getWidth() == 1);
        
        // Can't zoom in further
        tr.setCenterAndZoom(0, 1);
        REQUIRE(tr.getWidth() == 1);
        
        // Can't scroll
        tr.setVisibleRange(1, 1);
        REQUIRE(tr.start == 0);
        REQUIRE(tr.end == 0);
    }
    
    SECTION("Tight bounds (min_bound == max_bound)") {
        TimeRange tr(5, 5, 5, 5);
        
        REQUIRE(tr.start == 5);
        REQUIRE(tr.end == 5);
        REQUIRE(tr.getWidth() == 1);
        REQUIRE(tr.isAtMinBound() == true);
        REQUIRE(tr.isAtMaxBound() == true);
    }
    
    SECTION("Large time values (64-bit range)") {
        int64_t large = 1000000000000LL; // 1 trillion
        TimeRange tr(large, large + 1000, large - 100, large + 2000);
        
        REQUIRE(tr.start == large);
        REQUIRE(tr.end == large + 1000);
        REQUIRE(tr.getWidth() == 1001);
        
        tr.setCenterAndZoom(large + 500, 2000);
        REQUIRE(tr.getWidth() == 2000);
    }
}
