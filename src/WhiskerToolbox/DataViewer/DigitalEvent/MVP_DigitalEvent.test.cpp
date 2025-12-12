#include "MVP_DigitalEvent.hpp"

#include "DigitalEventSeriesDisplayOptions.hpp"
#include "PlottingManager/PlottingManager.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

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

    SECTION("MVP transforms yield NDC Y within lane (two stacked events)") {
        PlottingManager manager;

        // Two stacked series
        std::vector<int> times(100);
        for(int i=0; i<100; ++i) times[i] = i;
        auto time_frame = std::make_shared<TimeFrame>(times);
        
        auto series = std::make_shared<DigitalEventSeries>();
        series->setTimeFrame(time_frame);

        manager.total_event_series = 2;
        int e0 = 0;
        int e1 = 1;
        REQUIRE(e0 == 0);
        REQUIRE(e1 == 1);

        float c0, h0, c1, h1;
        manager.calculateDigitalEventSeriesAllocation(e0, c0, h0);
        manager.calculateDigitalEventSeriesAllocation(e1, c1, h1);

        NewDigitalEventSeriesDisplayOptions o0, o1;
        o0.plotting_mode = EventPlottingMode::Stacked;
        o1.plotting_mode = EventPlottingMode::Stacked;
        o0.allocated_y_center = c0; o0.allocated_height = h0; o0.margin_factor = 0.95f;
        o1.allocated_y_center = c1; o1.allocated_height = h1; o1.margin_factor = 0.95f;

        // Use default event_height; ensure it does not exceed lane
        glm::mat4 m0 = new_getEventModelMat(o0, manager);
        glm::mat4 v0 = new_getEventViewMat(o0, manager);
        glm::mat4 p0 = new_getEventProjectionMat(0, 1000, -1.0f, 1.0f, manager);

        glm::mat4 m1 = new_getEventModelMat(o1, manager);
        glm::mat4 v1 = new_getEventViewMat(o1, manager);
        glm::mat4 p1 = new_getEventProjectionMat(0, 1000, -1.0f, 1.0f, manager);

        auto ndcY = [](glm::mat4 const & p, glm::mat4 const & v, glm::mat4 const & m, float localY) {
            glm::vec4 vtx(500.0f, localY, 0.0f, 1.0f);
            glm::vec4 clip = p * v * m * vtx;
            return clip.y / clip.w;
        };

        float y0_min = ndcY(p0, v0, m0, -1.0f);
        float y0_max = ndcY(p0, v0, m0,  1.0f);
        float y1_min = ndcY(p1, v1, m1, -1.0f);
        float y1_max = ndcY(p1, v1, m1,  1.0f);

        std::cout << "y0_min: " << y0_min 
        << ", y0_max: " << y0_max 
        << ", y1_min: " << y1_min 
        << ", y1_max: " << y1_max 
        << std::endl;

        // All endpoints should be inside NDC [-1, 1]
        REQUIRE(y0_min >= -1.0f);
        REQUIRE(y0_max <=  1.0f);
        REQUIRE(y1_min >= -1.0f);
        REQUIRE(y1_max <=  1.0f);

        // Each lane height in NDC should be notably smaller than full canvas
        REQUIRE((y0_max - y0_min) < 0.8f);
        REQUIRE((y1_max - y1_min) < 0.8f);

        // Lanes should not overlap significantly (allow tiny numeric tolerance)
        REQUIRE(y0_max <= y1_min + 0.05f);
    }

    SECTION("MVP transforms produce non-overlapping lanes for three stacked events") {
        PlottingManager manager;

        std::vector<int> times(100);
        for(int i=0; i<100; ++i) times[i] = i;
        auto time_frame = std::make_shared<TimeFrame>(times);
        
        auto series = std::make_shared<DigitalEventSeries>();
        series->setTimeFrame(time_frame);

        manager.total_event_series = 3;
        int e0 = 0;
        int e1 = 1;
        int e2 = 2;

        float c0, h0, c1, h1, c2, h2;
        manager.calculateDigitalEventSeriesAllocation(e0, c0, h0);
        manager.calculateDigitalEventSeriesAllocation(e1, c1, h1);
        manager.calculateDigitalEventSeriesAllocation(e2, c2, h2);

        auto make_opts = [](float c, float h) {
            NewDigitalEventSeriesDisplayOptions o;
            o.plotting_mode = EventPlottingMode::Stacked;
            o.allocated_y_center = c;
            o.allocated_height = h;
            o.margin_factor = 0.95f;
            return o;
        };

        auto o0 = make_opts(c0, h0);
        auto o1 = make_opts(c1, h1);
        auto o2 = make_opts(c2, h2);

        auto ndcY = [](glm::mat4 const & p, glm::mat4 const & v, glm::mat4 const & m, float localY) {
            glm::vec4 vtx(600.0f, localY, 0.0f, 1.0f);
            glm::vec4 clip = p * v * m * vtx;
            return clip.y / clip.w;
        };

        auto check = [&](NewDigitalEventSeriesDisplayOptions const & o) {
            glm::mat4 M = new_getEventModelMat(o, manager);
            glm::mat4 V = new_getEventViewMat(o, manager);
            glm::mat4 P = new_getEventProjectionMat(0, 1200, -1.0f, 1.0f, manager);
            float y_min = ndcY(P, V, M, -1.0f);
            float y_max = ndcY(P, V, M,  1.0f);
            REQUIRE(y_min >= -1.0f);
            REQUIRE(y_max <=  1.0f);
            REQUIRE((y_max - y_min) < 0.8f);
            return std::pair<float,float>(y_min, y_max);
        };

        auto [a0, b0] = check(o0);
        auto [a1, b1] = check(o1);
        auto [a2, b2] = check(o2);

        // Ensure ordering and no significant overlap
        REQUIRE(b0 <= a1 + 0.05f);
        REQUIRE(b1 <= a2 + 0.05f);
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
        std::vector<int> times(100);
        for(int i=0; i<100; ++i) times[i] = i;
        auto time_frame = std::make_shared<TimeFrame>(times);
        
        auto series = std::make_shared<DigitalEventSeries>();
        series->setTimeFrame(time_frame);

        manager.total_event_series = 1;
        int event_series = 0;
        REQUIRE(event_series == 0);
        REQUIRE(manager.total_event_series == 1);

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
        std::vector<int> times(100);
        for(int i=0; i<100; ++i) times[i] = i;
        auto time_frame = std::make_shared<TimeFrame>(times);
        
        auto series = std::make_shared<DigitalEventSeries>();
        series->setTimeFrame(time_frame);

        manager.total_event_series = 1;
        int event_series = 0;

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
        std::vector<int> times(100);
        for(int i=0; i<100; ++i) times[i] = i;
        auto time_frame = std::make_shared<TimeFrame>(times);
        
        auto series = std::make_shared<DigitalEventSeries>();
        series->setTimeFrame(time_frame);

        manager.total_event_series = 3;
        int event1 = 0;
        int event2 = 1;
        int event3 = 2;

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
        std::vector<int> times(100);
        for(int i=0; i<100; ++i) times[i] = i;
        auto time_frame = std::make_shared<TimeFrame>(times);
        
        auto series = std::make_shared<DigitalEventSeries>();
        series->setTimeFrame(time_frame);

        manager.total_event_series = 1;
        int event_series = 0;
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
        std::vector<int> times(100);
        for(int i=0; i<100; ++i) times[i] = i;
        auto time_frame = std::make_shared<TimeFrame>(times);
        
        auto series = std::make_shared<DigitalEventSeries>();
        series->setTimeFrame(time_frame);

        manager.total_event_series = 3;
        int event1 = 0;
        int event2 = 1;
        int event3 = 2;

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

        // Scaling should match single-series scale due to capped event height
        NewDigitalEventSeriesDisplayOptions single_options;
        single_options.plotting_mode = EventPlottingMode::Stacked;
        single_options.allocated_y_center = 0.0f;
        single_options.allocated_height = 2.0f;// Full canvas
        glm::mat4 single_model = new_getEventModelMat(single_options, manager);

        float expected_scale = single_model[1][1];
        REQUIRE_THAT(model1[1][1], WithinRel(expected_scale, 0.1f));
        REQUIRE_THAT(model2[1][1], WithinRel(expected_scale, 0.1f));
        REQUIRE_THAT(model3[1][1], WithinRel(expected_scale, 0.1f));
    }

    SECTION("Panning behavior - FullCanvas mode (viewport-pinned)") {
        PlottingManager manager;

        std::vector<int> times(100);
        for(int i=0; i<100; ++i) times[i] = i;
        auto time_frame = std::make_shared<TimeFrame>(times);
        
        auto series = std::make_shared<DigitalEventSeries>();
        series->setTimeFrame(time_frame);

        manager.total_event_series = 1;
        int event_series = 0;

        NewDigitalEventSeriesDisplayOptions options;
        options.plotting_mode = EventPlottingMode::FullCanvas;

        float center, height;
        manager.calculateDigitalEventSeriesAllocation(event_series, center, height);
        options.allocated_y_center = center;
        options.allocated_height = height;

        // Test without panning (baseline)
        glm::mat4 model_no_pan = new_getEventModelMat(options, manager);
        glm::mat4 view_no_pan = new_getEventViewMat(options, manager);

        // Events should extend full canvas height
        REQUIRE(model_no_pan[1][1] > 0.0f);
        REQUIRE(model_no_pan[3][1] == 0.0f);// Centered
        REQUIRE(view_no_pan[3][1] == 0.0f); // No pan translation

        // Test with positive panning
        manager.setPanOffset(1.5f);
        glm::mat4 model_pan_up = new_getEventModelMat(options, manager);
        glm::mat4 view_pan_up = new_getEventViewMat(options, manager);

        // FullCanvas mode: Events should NOT move with panning (viewport-pinned)
        REQUIRE(model_pan_up[1][1] == model_no_pan[1][1]);// Same scaling
        REQUIRE(model_pan_up[3][1] == model_no_pan[3][1]);// Same position
        REQUIRE(view_pan_up[3][1] == 0.0f);               // No pan applied

        // Test with negative panning
        manager.setPanOffset(-1.2f);
        glm::mat4 model_pan_down = new_getEventModelMat(options, manager);
        glm::mat4 view_pan_down = new_getEventViewMat(options, manager);

        // FullCanvas mode: Events should still NOT move
        REQUIRE(model_pan_down[1][1] == model_no_pan[1][1]);// Same scaling
        REQUIRE(model_pan_down[3][1] == model_no_pan[3][1]);// Same position
        REQUIRE(view_pan_down[3][1] == 0.0f);               // No pan applied

        manager.resetPan();
        REQUIRE(manager.getPanOffset() == 0.0f);
    }

    SECTION("Panning behavior - Stacked mode (content-following)") {
        PlottingManager manager;

        std::vector<int> times(100);
        for(int i=0; i<100; ++i) times[i] = i;
        auto time_frame = std::make_shared<TimeFrame>(times);
        
        auto series = std::make_shared<DigitalEventSeries>();
        series->setTimeFrame(time_frame);

        manager.total_event_series = 1;
        int event_series = 0;

        NewDigitalEventSeriesDisplayOptions options;
        options.plotting_mode = EventPlottingMode::Stacked;
        options.allocated_y_center = -0.3f;// Simulate specific allocation
        options.allocated_height = 1.2f;

        // Test without panning (baseline)
        glm::mat4 model_no_pan = new_getEventModelMat(options, manager);
        glm::mat4 view_no_pan = new_getEventViewMat(options, manager);

        REQUIRE(model_no_pan[1][1] > 0.0f);
        REQUIRE(model_no_pan[3][1] == -0.3f);// At allocated center
        REQUIRE(view_no_pan[3][1] == 0.0f);  // No pan translation

        // Test with positive panning
        float pan_offset_up = 0.8f;
        manager.setPanOffset(pan_offset_up);
        glm::mat4 model_pan_up = new_getEventModelMat(options, manager);
        glm::mat4 view_pan_up = new_getEventViewMat(options, manager);

        // Stacked mode: Events should move with panning
        REQUIRE(model_pan_up[1][1] == model_no_pan[1][1]);// Same scaling
        REQUIRE(model_pan_up[3][1] == model_no_pan[3][1]);// Model position unchanged
        REQUIRE(view_pan_up[3][1] == pan_offset_up);      // Pan applied in view matrix

        // Test with negative panning
        float pan_offset_down = -1.1f;
        manager.setPanOffset(pan_offset_down);
        glm::mat4 model_pan_down = new_getEventModelMat(options, manager);
        glm::mat4 view_pan_down = new_getEventViewMat(options, manager);

        // Stacked mode: Events should move with negative panning too
        REQUIRE(model_pan_down[1][1] == model_no_pan[1][1]);// Same scaling
        REQUIRE(model_pan_down[3][1] == model_no_pan[3][1]);// Model position unchanged
        REQUIRE(view_pan_down[3][1] == pan_offset_down);    // Pan applied in view matrix

        manager.resetPan();
        REQUIRE(manager.getPanOffset() == 0.0f);
    }

    SECTION("Panning behavior - Multiple stacked series") {
        PlottingManager manager;

        // Add 3 stacked event series
        std::vector<int> times(100);
        for(int i=0; i<100; ++i) times[i] = i;
        auto time_frame = std::make_shared<TimeFrame>(times);
        
        auto series = std::make_shared<DigitalEventSeries>();
        series->setTimeFrame(time_frame);

        manager.total_event_series = 3;
        int event1 = 0;
        int event2 = 1;
        int event3 = 2;

        // Get allocations for each series
        float center1, height1, center2, height2, center3, height3;
        manager.calculateDigitalEventSeriesAllocation(event1, center1, height1);
        manager.calculateDigitalEventSeriesAllocation(event2, center2, height2);
        manager.calculateDigitalEventSeriesAllocation(event3, center3, height3);

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

        // Test without panning
        glm::mat4 view1_no_pan = new_getEventViewMat(options1, manager);
        glm::mat4 view2_no_pan = new_getEventViewMat(options2, manager);
        glm::mat4 view3_no_pan = new_getEventViewMat(options3, manager);

        REQUIRE(view1_no_pan[3][1] == 0.0f);
        REQUIRE(view2_no_pan[3][1] == 0.0f);
        REQUIRE(view3_no_pan[3][1] == 0.0f);

        // Test with panning - all stacked series should move together
        float pan_offset = 0.6f;
        manager.setPanOffset(pan_offset);

        glm::mat4 view1_panned = new_getEventViewMat(options1, manager);
        glm::mat4 view2_panned = new_getEventViewMat(options2, manager);
        glm::mat4 view3_panned = new_getEventViewMat(options3, manager);

        // All stacked series should move by the same pan offset
        REQUIRE(view1_panned[3][1] == pan_offset);
        REQUIRE(view2_panned[3][1] == pan_offset);
        REQUIRE(view3_panned[3][1] == pan_offset);

        manager.resetPan();
    }

    SECTION("Mixed panning behavior - FullCanvas vs Stacked") {
        PlottingManager manager;

        // Simulate having both types (though they'd typically be separate series)
        NewDigitalEventSeriesDisplayOptions fullcanvas_options;
        fullcanvas_options.plotting_mode = EventPlottingMode::FullCanvas;
        fullcanvas_options.allocated_y_center = 0.0f;
        fullcanvas_options.allocated_height = 2.0f;

        NewDigitalEventSeriesDisplayOptions stacked_options;
        stacked_options.plotting_mode = EventPlottingMode::Stacked;
        stacked_options.allocated_y_center = -0.5f;
        stacked_options.allocated_height = 1.0f;

        // Apply panning
        float pan_offset = 1.0f;
        manager.setPanOffset(pan_offset);

        glm::mat4 fullcanvas_view = new_getEventViewMat(fullcanvas_options, manager);
        glm::mat4 stacked_view = new_getEventViewMat(stacked_options, manager);

        // FullCanvas should NOT move (viewport-pinned)
        REQUIRE(fullcanvas_view[3][1] == 0.0f);

        // Stacked should move (content-following)
        REQUIRE(stacked_view[3][1] == pan_offset);

        manager.resetPan();
    }
}
