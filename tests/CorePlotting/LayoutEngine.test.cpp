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

    StackedLayoutStrategy const strategy;
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

    StackedLayoutStrategy const strategy;
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

    StackedLayoutStrategy const strategy;
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

TEST_CASE("StackedLayoutStrategy - Shared lane groups reuse one transform", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {
            {"analog1", SeriesType::Analog, true, "lane_shared", 1.0f},
            {"event1", SeriesType::DigitalEvent, true, "lane_shared", 1.0f},
            {"analog2", SeriesType::Analog, true, "lane_other", 1.0f}};
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;

    StackedLayoutStrategy const strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.size() == 3);

    auto const & analog1 = response.layouts[0];
    auto const & event1 = response.layouts[1];
    auto const & analog2 = response.layouts[2];

    // Two lanes total: each gets half of viewport height.
    REQUIRE_THAT(getAllocatedHeight(analog1),
                 Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(getAllocatedHeight(analog2),
                 Catch::Matchers::WithinAbs(1.0f, 0.001f));

    // Shared lane members must reuse the same transform.
    REQUIRE_THAT(getAllocatedHeight(analog1),
                 Catch::Matchers::WithinAbs(getAllocatedHeight(event1), 0.001f));
    REQUIRE_THAT(getAllocatedYCenter(analog1),
                 Catch::Matchers::WithinAbs(getAllocatedYCenter(event1), 0.001f));

    // Different lane gets a different center.
    REQUIRE_FALSE(getAllocatedYCenter(analog1) == getAllocatedYCenter(analog2));
}

TEST_CASE("StackedLayoutStrategy - Weighted lanes allocate proportional heights", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {
            {"analog1", SeriesType::Analog, true, "lane_heavy", 2.0f},
            {"event1", SeriesType::DigitalEvent, true, "lane_light", 1.0f}};
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;

    StackedLayoutStrategy const strategy;
    LayoutResponse response = strategy.compute(request);

    REQUIRE(response.layouts.size() == 2);

    float const total_height = request.viewport_y_max - request.viewport_y_min;
    float const heavy_expected = total_height * (2.0f / 3.0f);
    float const light_expected = total_height * (1.0f / 3.0f);

    REQUIRE_THAT(getAllocatedHeight(response.layouts[0]),
                 Catch::Matchers::WithinAbs(heavy_expected, 0.001f));
    REQUIRE_THAT(getAllocatedHeight(response.layouts[1]),
                 Catch::Matchers::WithinAbs(light_expected, 0.001f));
}

TEST_CASE("StackedLayoutStrategy - Shared lane visibility stays consistent", "[CorePlotting][Layout][Culling]") {
    LayoutRequest request;
    request.series = {
            {"analog1", SeriesType::Analog, true, "lane_shared", 1.0f},
            {"event1", SeriesType::DigitalEvent, true, "lane_shared", 1.0f},
            {"analog2", SeriesType::Analog, true, "lane_other", 1.0f}};
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;

    StackedLayoutStrategy const strategy;
    LayoutResponse response = strategy.compute(request);

    auto const shared_center = getAllocatedYCenter(response.layouts[0]);
    auto const half_height = getAllocatedHeight(response.layouts[0]) * 0.5f;
    float const y_min = shared_center - half_height + 0.01f;
    float const y_max = shared_center + half_height - 0.01f;

    REQUIRE(response.isSeriesVisible("analog1", y_min, y_max));
    REQUIRE(response.isSeriesVisible("event1", y_min, y_max));
}

TEST_CASE("StackedLayoutStrategy - Empty request", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;

    StackedLayoutStrategy const strategy;
    LayoutResponse const response = strategy.compute(request);

    REQUIRE(response.layouts.empty());
}

TEST_CASE("RowLayoutStrategy - Single series", "[CorePlotting][Layout]") {
    LayoutRequest request;
    request.series = {{"trial1", SeriesType::DigitalEvent, true}};
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;

    RowLayoutStrategy const strategy;
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

    RowLayoutStrategy const strategy;
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

    RowLayoutStrategy const strategy;
    LayoutResponse const response = strategy.compute(request);

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

    RowLayoutStrategy const strategy;
    LayoutResponse const response = strategy.compute(request);

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
        LayoutResponse const response = engine.compute(request);
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
    LayoutEngine const engine(nullptr);

    LayoutResponse const response = engine.compute(request);
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

    StackedLayoutStrategy const strategy;
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

    RowLayoutStrategy const strategy;
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

// ============================================================================
// LayoutResponse::isSeriesVisible / visibleSeriesIds (viewport culling)
// ============================================================================

TEST_CASE("LayoutResponse::isSeriesVisible - basic overlap", "[CorePlotting][Layout][Culling]") {
    // Three lanes stacked at y = 0.5, 1.5, 2.5 (height 1.0 each)
    LayoutResponse response;
    response.layouts = {
            {"s1", LayoutTransform(0.5f, 0.5f), 0},
            {"s2", LayoutTransform(1.5f, 0.5f), 1},
            {"s3", LayoutTransform(2.5f, 0.5f), 2}};

    SECTION("Viewport covers all lanes") {
        REQUIRE(response.isSeriesVisible("s1", 0.0f, 3.0f));
        REQUIRE(response.isSeriesVisible("s2", 0.0f, 3.0f));
        REQUIRE(response.isSeriesVisible("s3", 0.0f, 3.0f));
    }

    SECTION("Viewport covers only first lane") {
        REQUIRE(response.isSeriesVisible("s1", 0.0f, 1.0f));
        REQUIRE_FALSE(response.isSeriesVisible("s2", 0.0f, 0.9f));
        REQUIRE_FALSE(response.isSeriesVisible("s3", 0.0f, 0.9f));
    }

    SECTION("Viewport covers only last lane") {
        REQUIRE_FALSE(response.isSeriesVisible("s1", 2.1f, 3.0f));
        REQUIRE_FALSE(response.isSeriesVisible("s2", 2.1f, 3.0f));
        REQUIRE(response.isSeriesVisible("s3", 2.1f, 3.0f));
    }

    SECTION("Viewport partially overlaps a lane boundary") {
        // Viewport [0.9, 1.1] overlaps both s1 (top=1.0) and s2 (bottom=1.0)
        REQUIRE(response.isSeriesVisible("s1", 0.9f, 1.1f));
        REQUIRE(response.isSeriesVisible("s2", 0.9f, 1.1f));
        REQUIRE_FALSE(response.isSeriesVisible("s3", 0.9f, 1.1f));
    }

    SECTION("Unknown series is conservatively visible") {
        REQUIRE(response.isSeriesVisible("unknown_key", 0.0f, 1.0f));
    }
}

TEST_CASE("LayoutResponse::isSeriesVisible - exact boundary touch", "[CorePlotting][Layout][Culling]") {
    LayoutResponse response;
    response.layouts = {
            {"s1", LayoutTransform(0.5f, 0.5f), 0}};
    // Lane s1: [0.0, 1.0]

    SECTION("Viewport top exactly touches lane bottom") {
        // Viewport [0.0, 0.0] — degenerate but lane_top(1.0) >= y_min(0.0) and lane_bottom(0.0) <= y_max(0.0)
        REQUIRE(response.isSeriesVisible("s1", 0.0f, 0.0f));
    }

    SECTION("Viewport entirely below lane") {
        REQUIRE_FALSE(response.isSeriesVisible("s1", -2.0f, -0.1f));
    }

    SECTION("Viewport entirely above lane") {
        REQUIRE_FALSE(response.isSeriesVisible("s1", 1.1f, 2.0f));
    }
}

TEST_CASE("LayoutResponse::visibleSeriesIds", "[CorePlotting][Layout][Culling]") {
    LayoutResponse response;
    response.layouts = {
            {"s1", LayoutTransform(0.5f, 0.5f), 0},
            {"s2", LayoutTransform(1.5f, 0.5f), 1},
            {"s3", LayoutTransform(2.5f, 0.5f), 2}};

    SECTION("All visible") {
        auto ids = response.visibleSeriesIds(0.0f, 3.0f);
        REQUIRE(ids.size() == 3);
    }

    SECTION("Only middle lane visible") {
        auto ids = response.visibleSeriesIds(1.1f, 1.9f);
        REQUIRE(ids.size() == 1);
        REQUIRE(ids[0] == "s2");
    }

    SECTION("None visible") {
        auto ids = response.visibleSeriesIds(5.0f, 6.0f);
        REQUIRE(ids.empty());
    }
}
