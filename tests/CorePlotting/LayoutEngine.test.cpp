#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/Layout/LayoutEngine.hpp"
#include "CorePlotting/Layout/LayoutTransform.hpp"
#include "CorePlotting/Layout/RowLayoutStrategy.hpp"
#include "CorePlotting/Layout/StackedLayoutStrategy.hpp"

using namespace CorePlotting;

// Helper to get allocated_height from y_transform (for test compatibility)
inline float getAllocatedHeight(SeriesLayout const & layout) {
    return layout.y_transform.gain * 2.0f;
}

// Helper to get allocated_y_center from y_transform (for test compatibility)
inline float getAllocatedYCenter(SeriesLayout const & layout) {
    return layout.y_transform.offset;
}

TEST_CASE("LayoutRequest - countSeriesOfType", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {
            {"analog1", SeriesType::Analog, true},
            {"analog2", SeriesType::Analog, true},
            {"event1", SeriesType::DigitalEvent, true},
            {"interval1", SeriesType::DigitalInterval, false}};

    REQUIRE(request.countSeriesOfType(SeriesType::Analog) == 2);
    REQUIRE(request.countSeriesOfType(SeriesType::DigitalEvent) == 1);
    REQUIRE(request.countSeriesOfType(SeriesType::DigitalInterval) == 1);
}

TEST_CASE("LayoutRequest - countStackableSeries", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {
            {"analog1", SeriesType::Analog, true},
            {"analog2", SeriesType::Analog, true},
            {"event1", SeriesType::DigitalEvent, true},
            {"interval1", SeriesType::DigitalInterval, false},// Not stackable
            {"event2", SeriesType::DigitalEvent, false}       // Not stackable
    };

    REQUIRE(request.countStackableSeries() == 3);
}

TEST_CASE("LayoutResponse - findLayout", "[CorePlotting][Layout]") {
    LayoutResponse response;
    // Create layouts with y_transform: offset=center, gain=half_height
    response.layouts = {
            {"series1", LayoutTransform(0.5f, 0.5f), 0},// center=0.5, height=1.0
            {"series2", LayoutTransform(1.5f, 0.5f), 1},// center=1.5, height=1.0
            {"series3", LayoutTransform(2.5f, 0.5f), 2} // center=2.5, height=1.0
    };

    SECTION("Find existing series") {
        auto const * layout = response.findLayout("series2");
        REQUIRE(layout != nullptr);
        REQUIRE(layout->series_id == "series2");
        REQUIRE_THAT(getAllocatedYCenter(*layout),
                     Catch::Matchers::WithinAbs(1.5f, 0.001f));
    }

    SECTION("Find non-existing series") {
        auto const * layout = response.findLayout("nonexistent");
        REQUIRE(layout == nullptr);
    }
}

TEST_CASE("StackedLayoutStrategy - Single analog series", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {{"analog1", SeriesType::Analog, true}};
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;

    StackedLayoutStrategy strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.size() == 1);

    auto const & layout = response.layouts[0];
    REQUIRE(layout.series_id == "analog1");
    REQUIRE(layout.series_index == 0);

    // Single series should get full viewport height
    REQUIRE_THAT(getAllocatedYCenter(layout),
                 Catch::Matchers::WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(getAllocatedHeight(layout),
                 Catch::Matchers::WithinAbs(2.0f, 0.001f));
}

TEST_CASE("StackedLayoutStrategy - Multiple analog series", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {
            {"analog1", SeriesType::Analog, true},
            {"analog2", SeriesType::Analog, true},
            {"analog3", SeriesType::Analog, true}};
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;

    StackedLayoutStrategy strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.size() == 3);

    // Each series should get equal height (2.0 / 3 = 0.667)
    float const expected_height = 2.0f / 3.0f;

    SECTION("First series at top") {
        auto const & layout = response.layouts[0];
        REQUIRE(layout.series_id == "analog1");
        REQUIRE_THAT(getAllocatedHeight(layout),
                     Catch::Matchers::WithinAbs(expected_height, 0.001f));
        // Center at -1.0 + height/2 = -1.0 + 0.333 = -0.667
        REQUIRE_THAT(getAllocatedYCenter(layout),
                     Catch::Matchers::WithinAbs(-0.667f, 0.01f));
    }

    SECTION("Second series in middle") {
        auto const & layout = response.layouts[1];
        REQUIRE(layout.series_id == "analog2");
        REQUIRE_THAT(getAllocatedHeight(layout),
                     Catch::Matchers::WithinAbs(expected_height, 0.001f));
        // Center at -1.0 + height * 1.5 = -1.0 + 1.0 = 0.0
        REQUIRE_THAT(getAllocatedYCenter(layout),
                     Catch::Matchers::WithinAbs(0.0f, 0.01f));
    }

    SECTION("Third series at bottom") {
        auto const & layout = response.layouts[2];
        REQUIRE(layout.series_id == "analog3");
        REQUIRE_THAT(getAllocatedHeight(layout),
                     Catch::Matchers::WithinAbs(expected_height, 0.001f));
        // Center at -1.0 + height * 2.5 = -1.0 + 1.667 = 0.667
        REQUIRE_THAT(getAllocatedYCenter(layout),
                     Catch::Matchers::WithinAbs(0.667f, 0.01f));
    }
}

TEST_CASE("StackedLayoutStrategy - Mixed stackable and full-canvas series", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {
            {"analog1", SeriesType::Analog, true},            // Stackable
            {"interval1", SeriesType::DigitalInterval, false},// Full-canvas
            {"analog2", SeriesType::Analog, true},            // Stackable
            {"event1", SeriesType::DigitalEvent, false}       // Full-canvas
    };
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;

    StackedLayoutStrategy strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.size() == 4);

    SECTION("Stackable series divide viewport equally") {
        // Two stackable series: each gets height = 2.0 / 2 = 1.0
        auto const & analog1 = response.layouts[0];
        REQUIRE(analog1.series_id == "analog1");
        REQUIRE_THAT(getAllocatedHeight(analog1),
                     Catch::Matchers::WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(getAllocatedYCenter(analog1),
                     Catch::Matchers::WithinAbs(-0.5f, 0.001f));

        auto const & analog2 = response.layouts[2];
        REQUIRE(analog2.series_id == "analog2");
        REQUIRE_THAT(getAllocatedHeight(analog2),
                     Catch::Matchers::WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(getAllocatedYCenter(analog2),
                     Catch::Matchers::WithinAbs(0.5f, 0.001f));
    }

    SECTION("Full-canvas series span entire viewport") {
        auto const & interval = response.layouts[1];
        REQUIRE(interval.series_id == "interval1");
        REQUIRE_THAT(getAllocatedHeight(interval),
                     Catch::Matchers::WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(getAllocatedYCenter(interval),
                     Catch::Matchers::WithinAbs(0.0f, 0.001f));

        auto const & event = response.layouts[3];
        REQUIRE(event.series_id == "event1");
        REQUIRE_THAT(getAllocatedHeight(event),
                     Catch::Matchers::WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(getAllocatedYCenter(event),
                     Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("StackedLayoutStrategy - Empty request", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;

    StackedLayoutStrategy strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.empty());
}

TEST_CASE("RowLayoutStrategy - Single series", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {{"trial1", SeriesType::DigitalEvent, true}};
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;

    RowLayoutStrategy strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.size() == 1);

    auto const & layout = response.layouts[0];
    REQUIRE(layout.series_id == "trial1");
    REQUIRE(layout.series_index == 0);

    // Single row should get full viewport height
    REQUIRE_THAT(getAllocatedYCenter(layout),
                 Catch::Matchers::WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(getAllocatedHeight(layout),
                 Catch::Matchers::WithinAbs(2.0f, 0.001f));
}

TEST_CASE("RowLayoutStrategy - Multiple rows", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {
            {"trial1", SeriesType::DigitalEvent, true},
            {"trial2", SeriesType::DigitalEvent, true},
            {"trial3", SeriesType::DigitalEvent, true},
            {"trial4", SeriesType::DigitalEvent, true}};
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;

    RowLayoutStrategy strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.size() == 4);

    // Each row should get equal height (2.0 / 4 = 0.5)
    float const expected_height = 0.5f;

    SECTION("Row spacing is uniform") {
        for (int i = 0; i < 4; ++i) {
            auto const & layout = response.layouts[i];
            REQUIRE_THAT(getAllocatedHeight(layout),
                         Catch::Matchers::WithinAbs(expected_height, 0.001f));

            // Center at -1.0 + height * (i + 0.5)
            float const expected_center = -1.0f + expected_height * (static_cast<float>(i) + 0.5f);
            REQUIRE_THAT(getAllocatedYCenter(layout),
                         Catch::Matchers::WithinAbs(expected_center, 0.001f));
        }
    }

    SECTION("Rows are ordered top to bottom") {
        REQUIRE(response.layouts[0].series_id == "trial1");
        REQUIRE(response.layouts[1].series_id == "trial2");
        REQUIRE(response.layouts[2].series_id == "trial3");
        REQUIRE(response.layouts[3].series_id == "trial4");

        // Y centers should decrease from top to bottom
        REQUIRE(getAllocatedYCenter(response.layouts[0]) <
                getAllocatedYCenter(response.layouts[1]));
        REQUIRE(getAllocatedYCenter(response.layouts[1]) <
                getAllocatedYCenter(response.layouts[2]));
        REQUIRE(getAllocatedYCenter(response.layouts[2]) <
                getAllocatedYCenter(response.layouts[3]));
    }
}

TEST_CASE("RowLayoutStrategy - Ignores is_stackable flag", "[CorePlotting][Layout]") {
    // RowLayoutStrategy treats all series equally regardless of stackable flag
    LayoutRequest request;
    request.series = {
            {"row1", SeriesType::Analog, true},          // Stackable
            {"row2", SeriesType::DigitalInterval, false},// Non-stackable
            {"row3", SeriesType::DigitalEvent, true}     // Stackable
    };
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;

    RowLayoutStrategy strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.size() == 3);

    // All rows should get equal height (2.0 / 3 = 0.667)
    float const expected_height = 2.0f / 3.0f;

    for (auto const & layout: response.layouts) {
        REQUIRE_THAT(getAllocatedHeight(layout),
                     Catch::Matchers::WithinAbs(expected_height, 0.001f));
    }
}

TEST_CASE("RowLayoutStrategy - Empty request", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;

    RowLayoutStrategy strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.empty());
}

TEST_CASE("LayoutEngine - Strategy switching", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {
            {"series1", SeriesType::Analog, true},
            {"series2", SeriesType::Analog, true}};
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;

    LayoutEngine engine(std::make_unique<StackedLayoutStrategy>());

    SECTION("Initial strategy works") {
        LayoutResponse response = engine.compute(request);
        REQUIRE(response.layouts.size() == 2);
    }

    SECTION("Switch to different strategy") {
        engine.setStrategy(std::make_unique<RowLayoutStrategy>());
        LayoutResponse response = engine.compute(request);
        REQUIRE(response.layouts.size() == 2);

        // Results should be identical for this case (both strategies do same thing)
        REQUIRE_THAT(getAllocatedHeight(response.layouts[0]),
                     Catch::Matchers::WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(getAllocatedHeight(response.layouts[1]),
                     Catch::Matchers::WithinAbs(1.0f, 0.001f));
    }
}

TEST_CASE("LayoutEngine - No strategy set", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {{"series1", SeriesType::Analog, true}};

    // Create engine with null strategy
    LayoutEngine engine(nullptr);

    LayoutResponse response = engine.compute(request);
    REQUIRE(response.layouts.empty());
}

TEST_CASE("StackedLayoutStrategy - Custom viewport bounds", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {
            {"analog1", SeriesType::Analog, true},
            {"analog2", SeriesType::Analog, true}};
    // Non-standard viewport
    request.viewport_y_min = 0.0f;
    request.viewport_y_max = 100.0f;

    StackedLayoutStrategy strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.size() == 2);

    // Each series should get height = 100 / 2 = 50
    REQUIRE_THAT(getAllocatedHeight(response.layouts[0]),
                 Catch::Matchers::WithinAbs(50.0f, 0.001f));
    REQUIRE_THAT(getAllocatedHeight(response.layouts[1]),
                 Catch::Matchers::WithinAbs(50.0f, 0.001f));

    // Centers at 25 and 75
    REQUIRE_THAT(getAllocatedYCenter(response.layouts[0]),
                 Catch::Matchers::WithinAbs(25.0f, 0.001f));
    REQUIRE_THAT(getAllocatedYCenter(response.layouts[1]),
                 Catch::Matchers::WithinAbs(75.0f, 0.001f));
}

TEST_CASE("RowLayoutStrategy - Custom viewport bounds", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {
            {"row1", SeriesType::DigitalEvent, true},
            {"row2", SeriesType::DigitalEvent, true}};
    // Non-standard viewport
    request.viewport_y_min = 10.0f;
    request.viewport_y_max = 20.0f;

    RowLayoutStrategy strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.size() == 2);

    // Each row should get height = 10 / 2 = 5
    REQUIRE_THAT(getAllocatedHeight(response.layouts[0]),
                 Catch::Matchers::WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(getAllocatedHeight(response.layouts[1]),
                 Catch::Matchers::WithinAbs(5.0f, 0.001f));

    // Centers at 12.5 and 17.5
    REQUIRE_THAT(getAllocatedYCenter(response.layouts[0]),
                 Catch::Matchers::WithinAbs(12.5f, 0.001f));
    REQUIRE_THAT(getAllocatedYCenter(response.layouts[1]),
                 Catch::Matchers::WithinAbs(17.5f, 0.001f));
}

// ==================== FixedHeight Lane Sizing Tests ====================

TEST_CASE("StackedLayoutStrategy - FixedHeight single series", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {{"analog1", SeriesType::Analog, true}};
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.lane_sizing_policy = LaneSizingPolicy::FixedHeight;
    request.lane_height = 0.5f;
    request.lane_gap = 0.0f;

    StackedLayoutStrategy strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.size() == 1);

    auto const & layout = response.layouts[0];
    REQUIRE(layout.series_id == "analog1");

    // Lane height should be 0.5, gain = 0.25
    REQUIRE_THAT(getAllocatedHeight(layout),
                 Catch::Matchers::WithinAbs(0.5f, 0.001f));
    // Center at viewport_y_min + lane_height/2 = -1.0 + 0.25 = -0.75
    REQUIRE_THAT(getAllocatedYCenter(layout),
                 Catch::Matchers::WithinAbs(-0.75f, 0.001f));
}

TEST_CASE("StackedLayoutStrategy - FixedHeight multiple series", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {
            {"analog1", SeriesType::Analog, true},
            {"analog2", SeriesType::Analog, true},
            {"analog3", SeriesType::Analog, true}};
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.lane_sizing_policy = LaneSizingPolicy::FixedHeight;
    request.lane_height = 0.5f;
    request.lane_gap = 0.0f;

    StackedLayoutStrategy strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.size() == 3);

    // All lanes should have height 0.5
    for (auto const & layout: response.layouts) {
        REQUIRE_THAT(getAllocatedHeight(layout),
                     Catch::Matchers::WithinAbs(0.5f, 0.001f));
    }

    // Centers: -0.75, -0.25, 0.25
    REQUIRE_THAT(getAllocatedYCenter(response.layouts[0]),
                 Catch::Matchers::WithinAbs(-0.75f, 0.001f));
    REQUIRE_THAT(getAllocatedYCenter(response.layouts[1]),
                 Catch::Matchers::WithinAbs(-0.25f, 0.001f));
    REQUIRE_THAT(getAllocatedYCenter(response.layouts[2]),
                 Catch::Matchers::WithinAbs(0.25f, 0.001f));
}

TEST_CASE("StackedLayoutStrategy - FixedHeight with lane gap", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {
            {"analog1", SeriesType::Analog, true},
            {"analog2", SeriesType::Analog, true}};
    request.viewport_y_min = 0.0f;
    request.viewport_y_max = 10.0f;
    request.lane_sizing_policy = LaneSizingPolicy::FixedHeight;
    request.lane_height = 2.0f;
    request.lane_gap = 1.0f;

    StackedLayoutStrategy strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.size() == 2);

    // Both lanes should have height 2.0
    REQUIRE_THAT(getAllocatedHeight(response.layouts[0]),
                 Catch::Matchers::WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(getAllocatedHeight(response.layouts[1]),
                 Catch::Matchers::WithinAbs(2.0f, 0.001f));

    // Stride = lane_height + lane_gap = 3.0
    // Lane 0 center: viewport_y_min + 0 * stride + lane_height/2 = 0.0 + 0.0 + 1.0 = 1.0
    // Lane 1 center: viewport_y_min + 1 * stride + lane_height/2 = 0.0 + 3.0 + 1.0 = 4.0
    REQUIRE_THAT(getAllocatedYCenter(response.layouts[0]),
                 Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(getAllocatedYCenter(response.layouts[1]),
                 Catch::Matchers::WithinAbs(4.0f, 0.001f));
}

TEST_CASE("StackedLayoutStrategy - FixedHeight exceeds viewport", "[CorePlotting][Layout]") {
    // 10 series with height 0.5 each = total 5.0, viewport is only 2.0
    LayoutRequest request;
    for (int i = 0; i < 10; ++i) {
        request.series.emplace_back("ch" + std::to_string(i), SeriesType::Analog, true);
    }
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.lane_sizing_policy = LaneSizingPolicy::FixedHeight;
    request.lane_height = 0.5f;
    request.lane_gap = 0.0f;

    StackedLayoutStrategy strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.size() == 10);

    // Last lane center should exceed viewport_y_max
    float const last_center = getAllocatedYCenter(response.layouts[9]);
    // Center of lane 9: -1.0 + 0.5 * 9 + 0.25 = -1.0 + 4.5 + 0.25 = 3.75
    REQUIRE_THAT(last_center, Catch::Matchers::WithinAbs(3.75f, 0.001f));
    REQUIRE(last_center > request.viewport_y_max);
}

TEST_CASE("StackedLayoutStrategy - FixedHeight mixed with full-canvas", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {
            {"analog1", SeriesType::Analog, true},
            {"interval1", SeriesType::DigitalInterval, false},
            {"analog2", SeriesType::Analog, true}};
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.lane_sizing_policy = LaneSizingPolicy::FixedHeight;
    request.lane_height = 0.5f;
    request.lane_gap = 0.0f;

    StackedLayoutStrategy strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.size() == 3);

    SECTION("Stackable series use fixed height") {
        REQUIRE_THAT(getAllocatedHeight(response.layouts[0]),
                     Catch::Matchers::WithinAbs(0.5f, 0.001f));
        REQUIRE_THAT(getAllocatedHeight(response.layouts[2]),
                     Catch::Matchers::WithinAbs(0.5f, 0.001f));
    }

    SECTION("Full-canvas series still span entire viewport") {
        REQUIRE_THAT(getAllocatedHeight(response.layouts[1]),
                     Catch::Matchers::WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(getAllocatedYCenter(response.layouts[1]),
                     Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("StackedLayoutStrategy - FitToViewport ignores lane_height and lane_gap", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {
            {"analog1", SeriesType::Analog, true},
            {"analog2", SeriesType::Analog, true}};
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.lane_sizing_policy = LaneSizingPolicy::FitToViewport;
    request.lane_height = 99.0f;// Should be ignored
    request.lane_gap = 42.0f;   // Should be ignored

    StackedLayoutStrategy strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.size() == 2);

    // Each series should get viewport_height / 2 = 1.0
    REQUIRE_THAT(getAllocatedHeight(response.layouts[0]),
                 Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(getAllocatedHeight(response.layouts[1]),
                 Catch::Matchers::WithinAbs(1.0f, 0.001f));
}
