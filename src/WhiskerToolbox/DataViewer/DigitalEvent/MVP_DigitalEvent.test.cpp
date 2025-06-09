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

        // Test coordinate allocation for multiple series (should be stacked)
        float center1, height1;
        float center2, height2;
        float center3, height3;

        manager.calculateDigitalEventSeriesAllocation(event1, center1, height1);
        manager.calculateDigitalEventSeriesAllocation(event2, center2, height2);
        manager.calculateDigitalEventSeriesAllocation(event3, center3, height3);

        // Multiple event series should divide the canvas
        REQUIRE_THAT(height1, WithinRel(2.0f / 3.0f, 0.01f));
        REQUIRE_THAT(height2, WithinRel(2.0f / 3.0f, 0.01f));
        REQUIRE_THAT(height3, WithinRel(2.0f / 3.0f, 0.01f));

        // Centers should be different for each series
        REQUIRE_FALSE(center1 == center2);
        REQUIRE_FALSE(center2 == center3);
        REQUIRE_FALSE(center1 == center3);
    }

    SECTION("Single event series allocation behavior") {
        PlottingManager manager;

        // Single event series should get full canvas allocation
        int event_series = manager.addDigitalEventSeries();
        REQUIRE(manager.total_event_series == 1);

        float center, height;
        manager.calculateDigitalEventSeriesAllocation(event_series, center, height);

        // Should get full canvas
        REQUIRE_THAT(center, Catch::Matchers::WithinAbs(0.0f, 0.01f));// Center of viewport
        REQUIRE_THAT(height, WithinRel(2.0f, 0.01f));                 // Full viewport height

        // This ensures equivalent behavior for both FullCanvas and Stacked modes
        // when there's only one event series
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

    SECTION("Multiple event series stacked allocation") {
        PlottingManager manager;

        // Add 3 event series
        int event1 = manager.addDigitalEventSeries();
        int event2 = manager.addDigitalEventSeries();
        int event3 = manager.addDigitalEventSeries();

        REQUIRE(event1 == 0);
        REQUIRE(event2 == 1);
        REQUIRE(event3 == 2);
        REQUIRE(manager.total_event_series == 3);

        // Test coordinate allocation for each series
        float center1, height1;
        float center2, height2;
        float center3, height3;

        manager.calculateDigitalEventSeriesAllocation(event1, center1, height1);
        manager.calculateDigitalEventSeriesAllocation(event2, center2, height2);
        manager.calculateDigitalEventSeriesAllocation(event3, center3, height3);

        // Each series should get 1/3 of the total canvas height (2.0)
        float expected_height = 2.0f / 3.0f;
        REQUIRE_THAT(height1, WithinRel(expected_height, 0.01f));
        REQUIRE_THAT(height2, WithinRel(expected_height, 0.01f));
        REQUIRE_THAT(height3, WithinRel(expected_height, 0.01f));

        // Centers should be properly spaced: -1.0 + height/2, -1.0 + 1.5*height, -1.0 + 2.5*height
        float expected_center1 = -1.0f + expected_height * 0.5f;// First third
        float expected_center2 = -1.0f + expected_height * 1.5f;// Second third
        float expected_center3 = -1.0f + expected_height * 2.5f;// Third third

        REQUIRE_THAT(center1, WithinRel(expected_center1, 0.01f));
        REQUIRE_THAT(center2, Catch::Matchers::WithinAbs(expected_center2, 0.01f));
        REQUIRE_THAT(center3, WithinRel(expected_center3, 0.01f));

        // Test that stacked mode uses the allocated space correctly
        manager.setVisibleDataRange(1, 1000);

        NewDigitalEventSeriesDisplayOptions options1, options2, options3;
        options1.plotting_mode = EventPlottingMode::Stacked;
        options2.plotting_mode = EventPlottingMode::Stacked;
        options3.plotting_mode = EventPlottingMode::Stacked;

        options1.allocated_y_center = center1;
        options1.allocated_height = height1;
        options2.allocated_y_center = center2;
        options2.allocated_height = height2;
        options3.allocated_y_center = center3;
        options3.allocated_height = height3;

        // Generate model matrices for each series
        glm::mat4 model1 = new_getEventModelMat(options1, manager);
        glm::mat4 model2 = new_getEventModelMat(options2, manager);
        glm::mat4 model3 = new_getEventModelMat(options3, manager);

        // Each series should be positioned at its allocated center
        REQUIRE_THAT(model1[3][1], WithinRel(center1, 0.01f));
        REQUIRE_THAT(model2[3][1], WithinRel(center2, 0.01f));
        REQUIRE_THAT(model3[3][1], WithinRel(center3, 0.01f));

        // All should have positive scaling (but smaller than full canvas)
        REQUIRE(model1[1][1] > 0.0f);
        REQUIRE(model2[1][1] > 0.0f);
        REQUIRE(model3[1][1] > 0.0f);

        // Scaling should be approximately 1/3 of what a single series would get
        NewDigitalEventSeriesDisplayOptions single_options;
        single_options.plotting_mode = EventPlottingMode::Stacked;
        single_options.allocated_y_center = 0.0f;
        single_options.allocated_height = 2.0f;// Full canvas
        glm::mat4 single_model = new_getEventModelMat(single_options, manager);

        float expected_scale = single_model[1][1] / 3.0f;// 1/3 of full canvas scale
        REQUIRE_THAT(model1[1][1], WithinRel(expected_scale, 0.05f));
        REQUIRE_THAT(model2[1][1], WithinRel(expected_scale, 0.05f));
        REQUIRE_THAT(model3[1][1], WithinRel(expected_scale, 0.05f));
    }
}