#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/CoordinateTransform/SeriesCoordinateQuery.hpp"

using namespace CorePlotting;
using Catch::Matchers::WithinAbs;

namespace {

// Helper to create a simple layout response for testing
LayoutResponse makeTestLayout() {
    LayoutResponse response;
    
    // Three stacked series in [-1, +1] range:
    // Series 0: center at +0.5, height 0.6  (top)
    // Series 1: center at  0.0, height 0.6  (middle)
    // Series 2: center at -0.5, height 0.6  (bottom)
    
    response.layouts.push_back(SeriesLayout(
        SeriesLayoutResult(0.5f, 0.6f), "series_top", 0));
    response.layouts.push_back(SeriesLayout(
        SeriesLayoutResult(0.0f, 0.6f), "series_mid", 1));
    response.layouts.push_back(SeriesLayout(
        SeriesLayoutResult(-0.5f, 0.6f), "series_bot", 2));
    
    return response;
}

} // anonymous namespace

TEST_CASE("findSeriesAtWorldY basic queries", "[CorePlotting][SeriesCoordinateQuery]") {
    auto layout = makeTestLayout();
    
    SECTION("Query at center of top series") {
        auto result = findSeriesAtWorldY(0.5f, layout);
        REQUIRE(result.has_value());
        REQUIRE(result->series_key == "series_top");
        REQUIRE(result->series_index == 0);
        REQUIRE(result->is_within_bounds);
        REQUIRE_THAT(result->series_local_y, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(result->normalized_y, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Query at center of middle series") {
        auto result = findSeriesAtWorldY(0.0f, layout);
        REQUIRE(result.has_value());
        REQUIRE(result->series_key == "series_mid");
        REQUIRE(result->series_index == 1);
        REQUIRE(result->is_within_bounds);
        REQUIRE_THAT(result->series_local_y, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Query at center of bottom series") {
        auto result = findSeriesAtWorldY(-0.5f, layout);
        REQUIRE(result.has_value());
        REQUIRE(result->series_key == "series_bot");
        REQUIRE(result->series_index == 2);
        REQUIRE(result->is_within_bounds);
    }
    
    SECTION("Query at top edge of middle series (overlap region)") {
        // Middle series has center at 0, height 0.6
        // Top edge is at 0 + 0.3 = 0.3
        // But series_top extends from 0.2 to 0.8, so 0.29 is in overlap
        // The implementation returns the first matching series (series_top)
        auto result = findSeriesAtWorldY(0.29f, layout);
        REQUIRE(result.has_value());
        // In overlap region, first series in layout order is returned
        REQUIRE(result->series_key == "series_top");
        REQUIRE(result->is_within_bounds);
    }
    
    SECTION("Query at bottom edge of middle series") {
        // Bottom edge is at 0 - 0.3 = -0.3
        auto result = findSeriesAtWorldY(-0.29f, layout);
        REQUIRE(result.has_value());
        REQUIRE(result->series_key == "series_mid");
        REQUIRE(result->is_within_bounds);
        // normalized_y should be close to -1
        REQUIRE_THAT(result->normalized_y, WithinAbs(-0.967f, 0.01f));
    }
}

TEST_CASE("findSeriesAtWorldY gap between series", "[CorePlotting][SeriesCoordinateQuery]") {
    auto layout = makeTestLayout();
    
    // Gap between series_top and series_mid:
    // series_top bottom edge: 0.5 - 0.3 = 0.2
    // series_mid top edge: 0.0 + 0.3 = 0.3
    // They actually overlap slightly in this test setup
    
    SECTION("Query in gap (outside all series)") {
        // Create layout with actual gaps
        LayoutResponse gapped;
        gapped.layouts.push_back(SeriesLayout(
            SeriesLayoutResult(0.75f, 0.4f), "top", 0));  // [0.55, 0.95]
        gapped.layouts.push_back(SeriesLayout(
            SeriesLayoutResult(0.0f, 0.4f), "mid", 1));   // [-0.2, 0.2]
        gapped.layouts.push_back(SeriesLayout(
            SeriesLayoutResult(-0.75f, 0.4f), "bot", 2)); // [-0.95, -0.55]
        
        // Query at 0.4 (in gap between top and mid)
        auto result = findSeriesAtWorldY(0.4f, gapped);
        REQUIRE_FALSE(result.has_value());
    }
    
    SECTION("Query in gap with tolerance") {
        LayoutResponse gapped;
        gapped.layouts.push_back(SeriesLayout(
            SeriesLayoutResult(0.75f, 0.4f), "top", 0));  // [0.55, 0.95]
        gapped.layouts.push_back(SeriesLayout(
            SeriesLayoutResult(0.0f, 0.4f), "mid", 1));   // [-0.2, 0.2]
        
        // Query at 0.35 with tolerance 0.2 should hit mid (top edge at 0.2 + 0.2 = 0.4)
        auto result = findSeriesAtWorldY(0.35f, gapped, 0.2f);
        REQUIRE(result.has_value());
        REQUIRE(result->series_key == "mid");
        // But is_within_bounds should be false
        REQUIRE_FALSE(result->is_within_bounds);
    }
}

TEST_CASE("findSeriesAtWorldY outside all series", "[CorePlotting][SeriesCoordinateQuery]") {
    auto layout = makeTestLayout();
    
    SECTION("Query above all series") {
        auto result = findSeriesAtWorldY(1.5f, layout);
        REQUIRE_FALSE(result.has_value());
    }
    
    SECTION("Query below all series") {
        auto result = findSeriesAtWorldY(-1.5f, layout);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("findSeriesAtWorldY empty layout", "[CorePlotting][SeriesCoordinateQuery]") {
    LayoutResponse empty;
    auto result = findSeriesAtWorldY(0.0f, empty);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("findClosestSeriesAtWorldY", "[CorePlotting][SeriesCoordinateQuery]") {
    auto layout = makeTestLayout();
    
    SECTION("Query at center of middle series") {
        auto result = findClosestSeriesAtWorldY(0.0f, layout);
        REQUIRE(result.has_value());
        REQUIRE(result->series_key == "series_mid");
        REQUIRE(result->is_within_bounds);
    }
    
    SECTION("Query in gap - returns closest") {
        LayoutResponse gapped;
        gapped.layouts.push_back(SeriesLayout(
            SeriesLayoutResult(0.75f, 0.4f), "top", 0));
        gapped.layouts.push_back(SeriesLayout(
            SeriesLayoutResult(0.0f, 0.4f), "mid", 1));
        
        // Query at 0.4 is closer to top (center 0.75, distance 0.35) 
        // than mid (center 0.0, distance 0.4)
        auto result = findClosestSeriesAtWorldY(0.4f, gapped);
        REQUIRE(result.has_value());
        REQUIRE(result->series_key == "top");
        REQUIRE_FALSE(result->is_within_bounds); // 0.4 is outside [0.55, 0.95]
    }
    
    SECTION("Query above all series") {
        auto result = findClosestSeriesAtWorldY(1.5f, layout);
        REQUIRE(result.has_value());
        REQUIRE(result->series_key == "series_top");
        REQUIRE_FALSE(result->is_within_bounds);
    }
    
    SECTION("Empty layout") {
        LayoutResponse empty;
        auto result = findClosestSeriesAtWorldY(0.0f, empty);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("worldYToSeriesLocalY", "[CorePlotting][SeriesCoordinateQuery]") {
    SeriesLayout series(SeriesLayoutResult(0.5f, 0.6f), "test", 0);
    
    SECTION("At center") {
        float local = worldYToSeriesLocalY(0.5f, series);
        REQUIRE_THAT(local, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Above center") {
        float local = worldYToSeriesLocalY(0.7f, series);
        REQUIRE_THAT(local, WithinAbs(0.2f, 0.001f));
    }
    
    SECTION("Below center") {
        float local = worldYToSeriesLocalY(0.3f, series);
        REQUIRE_THAT(local, WithinAbs(-0.2f, 0.001f));
    }
}

TEST_CASE("seriesLocalYToWorldY", "[CorePlotting][SeriesCoordinateQuery]") {
    SeriesLayout series(SeriesLayoutResult(0.5f, 0.6f), "test", 0);
    
    SECTION("At center") {
        float world = seriesLocalYToWorldY(0.0f, series);
        REQUIRE_THAT(world, WithinAbs(0.5f, 0.001f));
    }
    
    SECTION("Above center") {
        float world = seriesLocalYToWorldY(0.2f, series);
        REQUIRE_THAT(world, WithinAbs(0.7f, 0.001f));
    }
}

TEST_CASE("Round-trip local <-> world Y", "[CorePlotting][SeriesCoordinateQuery]") {
    SeriesLayout series(SeriesLayoutResult(-0.3f, 0.8f), "test", 0);
    
    for (float world_y : {-0.7f, -0.3f, 0.0f, 0.1f}) {
        float local = worldYToSeriesLocalY(world_y, series);
        float back = seriesLocalYToWorldY(local, series);
        REQUIRE_THAT(back, WithinAbs(world_y, 0.001f));
    }
}

TEST_CASE("getSeriesWorldBounds", "[CorePlotting][SeriesCoordinateQuery]") {
    SeriesLayout series(SeriesLayoutResult(0.5f, 0.6f), "test", 0);
    
    auto [y_min, y_max] = getSeriesWorldBounds(series);
    
    // center 0.5, half_height 0.3 → bounds [0.2, 0.8]
    REQUIRE_THAT(y_min, WithinAbs(0.2f, 0.001f));
    REQUIRE_THAT(y_max, WithinAbs(0.8f, 0.001f));
}

TEST_CASE("isWithinSeriesBounds", "[CorePlotting][SeriesCoordinateQuery]") {
    SeriesLayout series(SeriesLayoutResult(0.5f, 0.6f), "test", 0);
    // Bounds: [0.2, 0.8]
    
    SECTION("Inside bounds") {
        REQUIRE(isWithinSeriesBounds(0.5f, series));
        REQUIRE(isWithinSeriesBounds(0.2f, series));
        REQUIRE(isWithinSeriesBounds(0.8f, series));
    }
    
    SECTION("Outside bounds") {
        REQUIRE_FALSE(isWithinSeriesBounds(0.1f, series));
        REQUIRE_FALSE(isWithinSeriesBounds(0.9f, series));
    }
    
    SECTION("With tolerance") {
        REQUIRE(isWithinSeriesBounds(0.1f, series, 0.15f));  // 0.1 + 0.15 > 0.2
        REQUIRE(isWithinSeriesBounds(0.9f, series, 0.15f));  // 0.9 - 0.15 < 0.8
    }
}

TEST_CASE("normalizedSeriesYToWorldY", "[CorePlotting][SeriesCoordinateQuery]") {
    SeriesLayout series(SeriesLayoutResult(0.5f, 0.6f), "test", 0);
    // half_height = 0.3
    
    SECTION("Center (normalized 0)") {
        float world = normalizedSeriesYToWorldY(0.0f, series);
        REQUIRE_THAT(world, WithinAbs(0.5f, 0.001f));
    }
    
    SECTION("Top edge (normalized +1)") {
        float world = normalizedSeriesYToWorldY(1.0f, series);
        REQUIRE_THAT(world, WithinAbs(0.8f, 0.001f));  // 0.5 + 0.3
    }
    
    SECTION("Bottom edge (normalized -1)") {
        float world = normalizedSeriesYToWorldY(-1.0f, series);
        REQUIRE_THAT(world, WithinAbs(0.2f, 0.001f));  // 0.5 - 0.3
    }
    
    SECTION("Half way up (normalized 0.5)") {
        float world = normalizedSeriesYToWorldY(0.5f, series);
        REQUIRE_THAT(world, WithinAbs(0.65f, 0.001f));  // 0.5 + 0.15
    }
}

TEST_CASE("worldYToNormalizedSeriesY", "[CorePlotting][SeriesCoordinateQuery]") {
    SeriesLayout series(SeriesLayoutResult(0.5f, 0.6f), "test", 0);
    
    SECTION("Center") {
        float norm = worldYToNormalizedSeriesY(0.5f, series);
        REQUIRE_THAT(norm, WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Top edge") {
        float norm = worldYToNormalizedSeriesY(0.8f, series);
        REQUIRE_THAT(norm, WithinAbs(1.0f, 0.001f));
    }
    
    SECTION("Bottom edge") {
        float norm = worldYToNormalizedSeriesY(0.2f, series);
        REQUIRE_THAT(norm, WithinAbs(-1.0f, 0.001f));
    }
    
    SECTION("Zero height series") {
        SeriesLayout zero_height(SeriesLayoutResult(0.5f, 0.0f), "zero", 0);
        float norm = worldYToNormalizedSeriesY(0.5f, zero_height);
        REQUIRE_THAT(norm, WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("Round-trip normalized <-> world Y", "[CorePlotting][SeriesCoordinateQuery]") {
    SeriesLayout series(SeriesLayoutResult(-0.2f, 0.8f), "test", 0);
    
    for (float norm : {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f}) {
        float world = normalizedSeriesYToWorldY(norm, series);
        float back = worldYToNormalizedSeriesY(world, series);
        REQUIRE_THAT(back, WithinAbs(norm, 0.001f));
    }
}

TEST_CASE("Practical scenario: DataViewer hover", "[CorePlotting][SeriesCoordinateQuery]") {
    // Simulate a DataViewer with 3 analog series
    // Viewport is [-1, +1] in Y
    LayoutResponse layout;
    layout.layouts.push_back(SeriesLayout(
        SeriesLayoutResult(0.6f, 0.5f), "EMG_signal", 0));
    layout.layouts.push_back(SeriesLayout(
        SeriesLayoutResult(0.0f, 0.5f), "LFP_channel1", 1));
    layout.layouts.push_back(SeriesLayout(
        SeriesLayoutResult(-0.6f, 0.5f), "Breathing", 2));
    
    SECTION("User hovers over LFP channel") {
        // World Y = 0.1 (slightly above center of middle series)
        auto result = findSeriesAtWorldY(0.1f, layout);
        REQUIRE(result.has_value());
        REQUIRE(result->series_key == "LFP_channel1");
        REQUIRE(result->is_within_bounds);
        
        // The series_local_y tells us position relative to series center
        // This would be passed to the data object for value interpretation
        REQUIRE_THAT(result->series_local_y, WithinAbs(0.1f, 0.001f));
    }
    
    SECTION("User hovers in gap between series") {
        // World Y = 0.36 (just inside EMG bottom edge, which is at 0.35)
        // EMG: center=0.6, height=0.5, bounds [0.35, 0.85]
        // LFP: center=0.0, height=0.5, bounds [-0.25, 0.25]
        // Gap is from 0.25 to 0.35
        auto result = findSeriesAtWorldY(0.36f, layout);
        REQUIRE(result.has_value());
        REQUIRE(result->series_key == "EMG_signal");
    }
}
