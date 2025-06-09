#include "MVP_DigitalEvent.hpp"
#include "../PlottingManager/PlottingManager.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using Catch::Matchers::WithinRel;

TEST_CASE("Digital Event MVP Functions", "[digital_event][mvp]") {

    SECTION("Event data validation") {
        // Test EventData struct
        EventData event1(5.5f);
        EventData event2;
        EventData event3(-1.0f);

        REQUIRE(event1.isValid());
        REQUIRE(event1.time == 5.5f);

        REQUIRE(event2.isValid());// Default time is 0.0f which is valid
        REQUIRE(event2.time == 0.0f);

        REQUIRE_FALSE(event3.isValid());// Negative time is invalid
    }

    SECTION("Event test data generation") {
        auto events = generateTestEventData(10, 100.0f, 42);

        REQUIRE(events.size() == 10);

        // Check that events are sorted by time
        for (size_t i = 1; i < events.size(); ++i) {
            REQUIRE(events[i].time >= events[i - 1].time);
        }

        // Check that all events are within range
        for (auto const & event: events) {
            REQUIRE(event.time >= 0.0f);
            REQUIRE(event.time <= 100.0f);
            REQUIRE(event.isValid());
        }
    }

    SECTION("Event intrinsic properties") {
        NewDigitalEventSeriesDisplayOptions options;

        // Test with empty events
        std::vector<EventData> empty_events;
        setEventIntrinsicProperties(empty_events, options);
        REQUIRE(options.alpha == 0.8f);// Should remain unchanged

        // Test with few events
        auto few_events = generateTestEventData(10, 100.0f, 42);
        setEventIntrinsicProperties(few_events, options);
        REQUIRE(options.alpha == 0.8f);      // Should remain unchanged
        REQUIRE(options.line_thickness == 2);// Should remain unchanged

        // Test with many events (should reduce alpha)
        auto many_events = generateTestEventData(150, 100.0f, 42);
        setEventIntrinsicProperties(many_events, options);
        REQUIRE(options.alpha < 0.8f);// Should be reduced

        // Test with very many events (should reduce line thickness too)
        auto very_many_events = generateTestEventData(250, 100.0f, 42);
        options.line_thickness = 2;// Reset
        setEventIntrinsicProperties(very_many_events, options);
        REQUIRE(options.line_thickness < 2);// Should be reduced
    }

    SECTION("Event MVP matrices - FullCanvas mode") {
        PlottingManager manager;
        int event_series = manager.addDigitalEventSeries();
        REQUIRE(event_series == 0);
        REQUIRE(manager.total_event_series == 1);

        manager.setVisibleDataRange(1, 1000);

        NewDigitalEventSeriesDisplayOptions options;
        options.plotting_mode = EventPlottingMode::FullCanvas;
        options.margin_factor = 0.95f;

        float center, height;
        manager.calculateDigitalEventSeriesAllocation(event_series, center, height);

        options.allocated_y_center = center;
        options.allocated_height = height;

        // Generate MVP matrices
        glm::mat4 model = new_getEventModelMat(options, manager);
        glm::mat4 view = new_getEventViewMat(options, manager);
        glm::mat4 projection = new_getEventProjectionMat(1, 1000, -1.0f, 1.0f, manager);

        // Test that model matrix scales to full canvas height
        REQUIRE(model[1][1] > 0.0f); // Y scaling should be positive
        REQUIRE(model[3][1] == 0.0f);// Should be centered

        // Test that view matrix is identity in FullCanvas mode (no panning)
        REQUIRE(view[0][0] == 1.0f);
        REQUIRE(view[1][1] == 1.0f);
        REQUIRE(view[3][1] == 0.0f);// No translation
    }

    SECTION("Event MVP matrices - Stacked mode") {
        PlottingManager manager;
        int event_series = manager.addDigitalEventSeries();

        manager.setVisibleDataRange(1, 1000);

        NewDigitalEventSeriesDisplayOptions options;
        options.plotting_mode = EventPlottingMode::Stacked;
        options.allocated_y_center = -0.5f;// Simulate allocation in lower half
        options.allocated_height = 1.0f;   // Half of total height
        options.margin_factor = 0.9f;

        // Generate MVP matrices
        glm::mat4 model = new_getEventModelMat(options, manager);
        glm::mat4 view = new_getEventViewMat(options, manager);
        glm::mat4 projection = new_getEventProjectionMat(1, 1000, -1.0f, 1.0f, manager);

        // Test that model matrix uses allocated space
        REQUIRE(model[1][1] > 0.0f);  // Y scaling should be positive
        REQUIRE(model[3][1] == -0.5f);// Should be at allocated center

        // Test panning in stacked mode
        manager.setPanOffset(0.5f);
        glm::mat4 view_panned = new_getEventViewMat(options, manager);
        REQUIRE(view_panned[3][1] == 0.5f);// Should apply pan offset
    }

    SECTION("Event panning behavior comparison") {
        PlottingManager manager;
        manager.setVisibleDataRange(1, 1000);

        // Setup two event series with different modes
        NewDigitalEventSeriesDisplayOptions full_canvas_options;
        full_canvas_options.plotting_mode = EventPlottingMode::FullCanvas;
        full_canvas_options.allocated_y_center = 0.0f;
        full_canvas_options.allocated_height = 2.0f;

        NewDigitalEventSeriesDisplayOptions stacked_options;
        stacked_options.plotting_mode = EventPlottingMode::Stacked;
        stacked_options.allocated_y_center = -0.5f;
        stacked_options.allocated_height = 1.0f;

        // Test with panning
        manager.setPanOffset(1.0f);

        glm::mat4 full_canvas_view = new_getEventViewMat(full_canvas_options, manager);
        glm::mat4 stacked_view = new_getEventViewMat(stacked_options, manager);

        // FullCanvas mode should not pan (viewport-pinned)
        REQUIRE(full_canvas_view[3][1] == 0.0f);

        // Stacked mode should pan (content-following)
        REQUIRE(stacked_view[3][1] == 1.0f);
    }

    SECTION("PlottingManager event series integration") {
        PlottingManager manager;

        // Test adding multiple event series
        int event1 = manager.addDigitalEventSeries();
        int event2 = manager.addDigitalEventSeries();
        int event3 = manager.addDigitalEventSeries();

        REQUIRE(event1 == 0);
        REQUIRE(event2 == 1);
        REQUIRE(event3 == 2);
        REQUIRE(manager.total_event_series == 3);

        // Test coordinate allocation
        float center, height;
        manager.calculateDigitalEventSeriesAllocation(event1, center, height);

        // All event series should get the same allocation (full canvas by default)
        REQUIRE_THAT(center, Catch::Matchers::WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(height, WithinRel(2.0f, 0.01f));
    }

    SECTION("Event rendering optimization") {
        // Test alpha reduction for dense event series
        std::vector<EventData> dense_events = generateTestEventData(500, 1000.0f, 123);
        NewDigitalEventSeriesDisplayOptions options;
        options.alpha = 0.8f;
        options.line_thickness = 3;

        setEventIntrinsicProperties(dense_events, options);

        // Should reduce both alpha and line thickness for very dense data
        REQUIRE(options.alpha < 0.8f);
        REQUIRE(options.line_thickness < 3);
        REQUIRE(options.line_thickness >= 1);// Should not go below 1
    }
}