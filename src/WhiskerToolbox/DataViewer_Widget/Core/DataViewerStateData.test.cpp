#include "DataViewerStateData.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

using Catch::Approx;

// ==================== SeriesStyleData Tests ====================

TEST_CASE("SeriesStyleData serialization", "[DataViewerStateData]") {
    SECTION("Default values serialize correctly") {
        CorePlotting::SeriesStyle const style;

        auto json = rfl::json::write(style);
        auto result = rfl::json::read<CorePlotting::SeriesStyle>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.hex_color == "#007bff");
        REQUIRE(data.alpha == Approx(1.0f));
        REQUIRE(data.line_thickness == 1);
        REQUIRE(data.is_visible == true);
    }

    SECTION("Custom values round-trip") {
        CorePlotting::SeriesStyle style;
        style.hex_color = "#ff0000";
        style.alpha = 0.5f;
        style.line_thickness = 3;
        style.is_visible = false;

        auto json = rfl::json::write(style);
        auto result = rfl::json::read<CorePlotting::SeriesStyle>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.hex_color == "#ff0000");
        REQUIRE(data.alpha == Approx(0.5f));
        REQUIRE(data.line_thickness == 3);
        REQUIRE(data.is_visible == false);
    }
}

// ==================== AnalogSeriesOptionsData Tests ====================

TEST_CASE("AnalogSeriesOptionsData serialization", "[DataViewerStateData]") {
    SECTION("Default values") {
        AnalogSeriesOptionsData const opts;

        auto json = rfl::json::write(opts);
        auto result = rfl::json::read<AnalogSeriesOptionsData>(json);

        REQUIRE(result);
        auto const & data = result.value();

        // Style fields (flattened)
        REQUIRE(data.hex_color() == "#007bff");
        REQUIRE(data.get_alpha() == Approx(1.0f));
        REQUIRE(data.get_line_thickness() == 1);
        REQUIRE(data.get_is_visible() == true);

        // Analog-specific fields
        REQUIRE(data.user_scale_factor == Approx(1.0f));
        REQUIRE(data.y_offset == Approx(0.0f));
        REQUIRE(data.gap_handling == AnalogGapHandlingMode::AlwaysConnect);
        REQUIRE(data.enable_gap_detection == false);
        REQUIRE(data.gap_threshold == Approx(5.0f));
        REQUIRE(data.enable_min_max_line_decimation == false);
    }

    SECTION("AnalogGapHandlingMode enum serializes as string") {
        AnalogSeriesOptionsData opts;
        opts.gap_handling = AnalogGapHandlingMode::DetectGaps;

        auto json = rfl::json::write(opts);

        // Enum should be serialized as string, not integer
        REQUIRE(json.find("\"DetectGaps\"") != std::string::npos);
        REQUIRE(json.find("\"gap_handling\":0") == std::string::npos);
        REQUIRE(json.find("\"gap_handling\":1") == std::string::npos);
    }

    SECTION("rfl::Flatten produces flat JSON") {
        AnalogSeriesOptionsData opts;
        opts.hex_color() = "#00ff00";

        auto json = rfl::json::write(opts);

        // hex_color should be at top level, not nested under "style"
        REQUIRE(json.find("\"hex_color\":\"#00ff00\"") != std::string::npos);
        REQUIRE(json.find("\"style\":{") == std::string::npos);
    }

    SECTION("Full analog options round-trip") {
        AnalogSeriesOptionsData opts;
        opts.hex_color() = "#ff00ff";
        opts.alpha() = 0.8f;
        opts.line_thickness() = 2;
        opts.is_visible() = false;
        opts.user_scale_factor = 2.5f;
        opts.y_offset = 0.3f;
        opts.gap_handling = AnalogGapHandlingMode::ShowMarkers;
        opts.enable_gap_detection = true;
        opts.gap_threshold = 10.0f;
        opts.enable_min_max_line_decimation = true;

        auto json = rfl::json::write(opts);
        auto result = rfl::json::read<AnalogSeriesOptionsData>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.hex_color() == "#ff00ff");
        REQUIRE(data.get_alpha() == Approx(0.8f));
        REQUIRE(data.get_line_thickness() == 2);
        REQUIRE(data.get_is_visible() == false);
        REQUIRE(data.user_scale_factor == Approx(2.5f));
        REQUIRE(data.y_offset == Approx(0.3f));
        REQUIRE(data.gap_handling == AnalogGapHandlingMode::ShowMarkers);
        REQUIRE(data.enable_gap_detection == true);
        REQUIRE(data.gap_threshold == Approx(10.0f));
        REQUIRE(data.enable_min_max_line_decimation == true);
    }
}

// ==================== DigitalEventSeriesOptionsData Tests ====================

TEST_CASE("DigitalEventSeriesOptionsData serialization", "[DataViewerStateData]") {
    SECTION("Default values") {
        DigitalEventSeriesOptionsData const opts;

        auto json = rfl::json::write(opts);
        auto result = rfl::json::read<DigitalEventSeriesOptionsData>(json);

        REQUIRE(result);
        auto const & data = result.value();

        REQUIRE(data.hex_color() == "#007bff");
        REQUIRE(data.get_is_visible() == true);
        REQUIRE(data.plotting_mode == EventPlottingModeData::FullCanvas);
        REQUIRE(data.glyph_shape == EventGlyphShapeData::Tick);
        REQUIRE(data.box_width_ticks == 10);
        REQUIRE(data.vertical_spacing == Approx(0.1f));
        REQUIRE(data.event_height == Approx(0.05f));
        REQUIRE(data.margin_factor == Approx(0.95f));
    }

    SECTION("EventPlottingModeData enum serializes as string") {
        DigitalEventSeriesOptionsData opts;
        opts.plotting_mode = EventPlottingModeData::Stacked;

        auto json = rfl::json::write(opts);

        REQUIRE(json.find("\"Stacked\"") != std::string::npos);
    }

    SECTION("EventGlyphShapeData enum serializes as string") {
        DigitalEventSeriesOptionsData opts;
        opts.glyph_shape = EventGlyphShapeData::Box;

        auto json = rfl::json::write(opts);

        REQUIRE(json.find("\"Box\"") != std::string::npos);
        REQUIRE(json.find("\"glyph_shape\":0") == std::string::npos);
    }

    SECTION("Full event options round-trip") {
        DigitalEventSeriesOptionsData opts;
        opts.hex_color() = "#ff9500";
        opts.alpha() = 0.7f;
        opts.plotting_mode = EventPlottingModeData::Stacked;
        opts.glyph_shape = EventGlyphShapeData::TopLine;
        opts.box_width_ticks = 24;
        opts.vertical_spacing = 0.2f;
        opts.event_height = 0.1f;
        opts.margin_factor = 0.9f;

        auto json = rfl::json::write(opts);
        auto result = rfl::json::read<DigitalEventSeriesOptionsData>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.hex_color() == "#ff9500");
        REQUIRE(data.get_alpha() == Approx(0.7f));
        REQUIRE(data.plotting_mode == EventPlottingModeData::Stacked);
        REQUIRE(data.glyph_shape == EventGlyphShapeData::TopLine);
        REQUIRE(data.box_width_ticks == 24);
        REQUIRE(data.vertical_spacing == Approx(0.2f));
        REQUIRE(data.event_height == Approx(0.1f));
        REQUIRE(data.margin_factor == Approx(0.9f));
    }

    SECTION("All EventGlyphShapeData values round-trip") {
        for (auto shape: {EventGlyphShapeData::Tick, EventGlyphShapeData::TopLine, EventGlyphShapeData::Box}) {
            DigitalEventSeriesOptionsData opts;
            opts.glyph_shape = shape;
            opts.box_width_ticks = 7;

            auto json = rfl::json::write(opts);
            auto result = rfl::json::read<DigitalEventSeriesOptionsData>(json);

            REQUIRE(result);
            REQUIRE(result.value().glyph_shape == shape);
            REQUIRE(result.value().box_width_ticks == 7);
        }
    }
}

// ==================== DigitalIntervalSeriesOptionsData Tests ====================

TEST_CASE("DigitalIntervalSeriesOptionsData serialization", "[DataViewerStateData]") {
    SECTION("Default values") {
        DigitalIntervalSeriesOptionsData const opts;

        auto json = rfl::json::write(opts);
        auto result = rfl::json::read<DigitalIntervalSeriesOptionsData>(json);

        REQUIRE(result);
        auto const & data = result.value();

        REQUIRE(data.hex_color() == "#007bff");
        REQUIRE(data.extend_full_canvas == true);
        REQUIRE(data.margin_factor == Approx(0.95f));
        REQUIRE(data.interval_height == Approx(1.0f));
    }

    SECTION("Full interval options round-trip") {
        DigitalIntervalSeriesOptionsData opts;
        opts.hex_color() = "#ff6b6b";
        opts.alpha() = 0.3f;
        opts.extend_full_canvas = false;
        opts.margin_factor = 0.85f;
        opts.interval_height = 0.5f;

        auto json = rfl::json::write(opts);
        auto result = rfl::json::read<DigitalIntervalSeriesOptionsData>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.hex_color() == "#ff6b6b");
        REQUIRE(data.get_alpha() == Approx(0.3f));
        REQUIRE(data.extend_full_canvas == false);
        REQUIRE(data.margin_factor == Approx(0.85f));
        REQUIRE(data.interval_height == Approx(0.5f));
    }
}

// ==================== ViewStateData Tests ====================

TEST_CASE("ViewStateData serialization", "[DataViewerStateData]") {
    SECTION("Default values") {
        CorePlotting::ViewStateData const view;

        auto json = rfl::json::write(view);
        auto result = rfl::json::read<CorePlotting::ViewStateData>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.x_min == Approx(-500.0));
        REQUIRE(data.x_max == Approx(500.0));
        REQUIRE(data.y_min == Approx(0.0));
        REQUIRE(data.y_max == Approx(100.0));
        REQUIRE(data.y_pan == Approx(0.0));
        REQUIRE(data.y_zoom == Approx(1.0));
    }

    SECTION("Custom values round-trip") {
        CorePlotting::ViewStateData view;
        view.x_min = 1000.0;
        view.x_max = 50000.0;
        view.y_min = -2.0;
        view.y_max = 2.0;
        view.y_pan = 0.5;
        view.y_zoom = 1.5;

        auto json = rfl::json::write(view);
        auto result = rfl::json::read<CorePlotting::ViewStateData>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.x_min == Approx(1000.0));
        REQUIRE(data.x_max == Approx(50000.0));
        REQUIRE(data.y_min == Approx(-2.0));
        REQUIRE(data.y_max == Approx(2.0));
        REQUIRE(data.y_pan == Approx(0.5));
        REQUIRE(data.y_zoom == Approx(1.5));
    }

    SECTION("Large time values (double)") {
        CorePlotting::ViewStateData view;
        view.x_min = 1000000000.0;
        view.x_max = 9000000000.0;

        auto json = rfl::json::write(view);
        auto result = rfl::json::read<CorePlotting::ViewStateData>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.x_min == Approx(1000000000.0));
        REQUIRE(data.x_max == Approx(9000000000.0));
    }
}

// ==================== DataViewerThemeState Tests ====================

TEST_CASE("DataViewerThemeState serialization", "[DataViewerStateData]") {
    SECTION("Default values (Dark theme)") {
        DataViewerThemeState const theme;

        auto json = rfl::json::write(theme);
        auto result = rfl::json::read<DataViewerThemeState>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.theme == DataViewerTheme::Dark);
        REQUIRE(data.background_color == "#000000");
        REQUIRE(data.axis_color == "#FFFFFF");
    }

    SECTION("DataViewerTheme enum serializes as string") {
        DataViewerThemeState theme;
        theme.theme = DataViewerTheme::Light;

        auto json = rfl::json::write(theme);

        REQUIRE(json.find("\"Light\"") != std::string::npos);
        REQUIRE(json.find("\"theme\":0") == std::string::npos);
        REQUIRE(json.find("\"theme\":1") == std::string::npos);
    }

    SECTION("Light theme round-trip") {
        DataViewerThemeState theme;
        theme.theme = DataViewerTheme::Light;
        theme.background_color = "#FFFFFF";
        theme.axis_color = "#333333";

        auto json = rfl::json::write(theme);
        auto result = rfl::json::read<DataViewerThemeState>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.theme == DataViewerTheme::Light);
        REQUIRE(data.background_color == "#FFFFFF");
        REQUIRE(data.axis_color == "#333333");
    }
}

// ==================== DataViewerGridState Tests ====================

TEST_CASE("DataViewerGridState serialization", "[DataViewerStateData]") {
    SECTION("Default values") {
        DataViewerGridState const grid;

        auto json = rfl::json::write(grid);
        auto result = rfl::json::read<DataViewerGridState>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.enabled == false);
        REQUIRE(data.spacing == 100);
    }

    SECTION("Custom values round-trip") {
        DataViewerGridState grid;
        grid.enabled = true;
        grid.spacing = 500;

        auto json = rfl::json::write(grid);
        auto result = rfl::json::read<DataViewerGridState>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.enabled == true);
        REQUIRE(data.spacing == 500);
    }
}

// ==================== DataViewerUIPreferences Tests ====================

TEST_CASE("DataViewerUIPreferences serialization", "[DataViewerStateData]") {
    SECTION("Default values") {
        DataViewerUIPreferences const ui;

        auto json = rfl::json::write(ui);
        auto result = rfl::json::read<DataViewerUIPreferences>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.zoom_scaling_mode == DataViewerZoomScalingMode::Adaptive);
        REQUIRE(data.properties_panel_collapsed == false);
    }

    SECTION("DataViewerZoomScalingMode enum serializes as string") {
        DataViewerUIPreferences ui;
        ui.zoom_scaling_mode = DataViewerZoomScalingMode::Fixed;

        auto json = rfl::json::write(ui);

        REQUIRE(json.find("\"Fixed\"") != std::string::npos);
    }

    SECTION("Custom values round-trip") {
        DataViewerUIPreferences ui;
        ui.zoom_scaling_mode = DataViewerZoomScalingMode::Fixed;
        ui.properties_panel_collapsed = true;

        auto json = rfl::json::write(ui);
        auto result = rfl::json::read<DataViewerUIPreferences>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.zoom_scaling_mode == DataViewerZoomScalingMode::Fixed);
        REQUIRE(data.properties_panel_collapsed == true);
    }
}

// ==================== DataViewerInteractionState Tests ====================

TEST_CASE("DataViewerInteractionState serialization", "[DataViewerStateData]") {
    SECTION("Default values") {
        DataViewerInteractionState const interaction;

        auto json = rfl::json::write(interaction);
        auto result = rfl::json::read<DataViewerInteractionState>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.mode == DataViewerInteractionMode::Normal);
    }

    SECTION("DataViewerInteractionMode enum serializes as string") {
        DataViewerInteractionState interaction;
        interaction.mode = DataViewerInteractionMode::CreateInterval;

        auto json = rfl::json::write(interaction);

        REQUIRE(json.find("\"CreateInterval\"") != std::string::npos);
    }

    SECTION("All interaction modes round-trip") {
        std::vector<DataViewerInteractionMode> const modes = {
                DataViewerInteractionMode::Normal,
                DataViewerInteractionMode::CreateInterval,
                DataViewerInteractionMode::ModifyInterval,
                DataViewerInteractionMode::CreateLine};

        for (auto mode: modes) {
            DataViewerInteractionState interaction;
            interaction.mode = mode;

            auto json = rfl::json::write(interaction);
            auto result = rfl::json::read<DataViewerInteractionState>(json);

            REQUIRE(result);
            REQUIRE(result.value().mode == mode);
        }
    }
}

// ==================== Mixed-Lane Override Tests ====================

TEST_CASE("SeriesLaneOverrideData serialization", "[DataViewerStateData]") {
    SECTION("Default values") {
        SeriesLaneOverrideData const override_data;

        auto json = rfl::json::write(override_data);
        auto result = rfl::json::read<SeriesLaneOverrideData>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.lane_id.empty());
        REQUIRE_FALSE(data.lane_order.has_value());
        REQUIRE(data.lane_weight == Approx(1.0f));
        REQUIRE(data.overlay_mode == LaneOverlayMode::Auto);
        REQUIRE(data.overlay_z == 0);
    }

    SECTION("Full values round-trip") {
        SeriesLaneOverrideData override_data;
        override_data.lane_id = "lane_primary";
        override_data.lane_order = 7;
        override_data.lane_weight = 2.5f;
        override_data.overlay_mode = LaneOverlayMode::Overlay;
        override_data.overlay_z = 3;

        auto json = rfl::json::write(override_data);
        auto result = rfl::json::read<SeriesLaneOverrideData>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.lane_id == "lane_primary");
        REQUIRE(data.lane_order.has_value());
        REQUIRE(data.lane_order == std::optional<int>{7});
        REQUIRE(data.lane_weight == Approx(2.5f));
        REQUIRE(data.overlay_mode == LaneOverlayMode::Overlay);
        REQUIRE(data.overlay_z == 3);
    }

    SECTION("LaneOverlayMode enum serializes as string") {
        SeriesLaneOverrideData override_data;
        override_data.overlay_mode = LaneOverlayMode::Separate;

        auto json = rfl::json::write(override_data);

        REQUIRE(json.find("\"Separate\"") != std::string::npos);
    }
}

TEST_CASE("LaneOverrideData serialization", "[DataViewerStateData]") {
    SECTION("Default values") {
        LaneOverrideData const override_data;

        auto json = rfl::json::write(override_data);
        auto result = rfl::json::read<LaneOverrideData>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.display_label.empty());
        REQUIRE_FALSE(data.lane_weight.has_value());
    }

    SECTION("Optional lane weight round-trip") {
        LaneOverrideData override_data;
        override_data.display_label = "L2: Motor";
        override_data.lane_weight = 1.75f;

        auto json = rfl::json::write(override_data);
        auto result = rfl::json::read<LaneOverrideData>(json);

        REQUIRE(result);
        auto const & data = result.value();
        REQUIRE(data.display_label == "L2: Motor");
        REQUIRE(data.lane_weight.has_value());
        REQUIRE(data.lane_weight.value_or(-1.0f) == Approx(1.75f));
    }
}

// ==================== DataViewerStateData (Full Structure) Tests ====================

TEST_CASE("DataViewerStateData full serialization", "[DataViewerStateData]") {
    SECTION("Default values") {
        DataViewerStateData state;
        state.instance_id = "test-123";

        auto json = rfl::json::write(state);
        auto result = rfl::json::read<DataViewerStateData>(json);

        REQUIRE(result);
        auto const & data = result.value();

        REQUIRE(data.instance_id == "test-123");
        REQUIRE(data.display_name == "Data Viewer");

        // Nested objects should have defaults
        REQUIRE(data.view.x_min == Approx(0.0));
        REQUIRE(data.theme.theme == DataViewerTheme::Dark);
        REQUIRE(data.grid.enabled == false);
        REQUIRE(data.ui.zoom_scaling_mode == DataViewerZoomScalingMode::Adaptive);
        REQUIRE(data.interaction.mode == DataViewerInteractionMode::Normal);

        // Maps should be empty
        REQUIRE(data.analog_options.empty());
        REQUIRE(data.event_options.empty());
        REQUIRE(data.interval_options.empty());
    }

    SECTION("Full state with series options round-trip") {
        DataViewerStateData state;
        state.instance_id = "viewer-abc";
        state.display_name = "Neural Data Viewer";

        // Configure view
        state.view.x_min = 5000.0;
        state.view.x_max = 50000.0;
        state.global_y_scale = 2.0f;

        // Configure theme
        state.theme.theme = DataViewerTheme::Light;
        state.theme.background_color = "#F5F5F5";

        // Configure grid
        state.grid.enabled = true;
        state.grid.spacing = 250;

        // Configure UI
        state.ui.zoom_scaling_mode = DataViewerZoomScalingMode::Fixed;
        state.ui.properties_panel_collapsed = true;

        // Configure interaction
        state.interaction.mode = DataViewerInteractionMode::CreateInterval;

        // Add analog series options
        AnalogSeriesOptionsData analog1;
        analog1.hex_color() = "#0000ff";
        analog1.user_scale_factor = 1.5f;
        analog1.gap_handling = AnalogGapHandlingMode::DetectGaps;
        state.analog_options["channel_1"] = analog1;

        AnalogSeriesOptionsData analog2;
        analog2.hex_color() = "#ff0000";
        analog2.is_visible() = false;
        state.analog_options["channel_2"] = analog2;

        // Add event series options
        DigitalEventSeriesOptionsData event1;
        event1.hex_color() = "#ff9500";
        event1.plotting_mode = EventPlottingModeData::Stacked;
        event1.glyph_shape = EventGlyphShapeData::Box;
        event1.box_width_ticks = 16;
        state.event_options["spikes_1"] = event1;

        // Add interval series options
        DigitalIntervalSeriesOptionsData interval1;
        interval1.hex_color() = "#00ff00";
        interval1.alpha() = 0.4f;
        state.interval_options["trial_markers"] = interval1;

        // Add mixed-lane overrides
        SeriesLaneOverrideData lane_override_1;
        lane_override_1.lane_id = "lane_a";
        lane_override_1.lane_order = 10;
        lane_override_1.lane_weight = 1.2f;
        lane_override_1.overlay_mode = LaneOverlayMode::Overlay;
        lane_override_1.overlay_z = 1;
        state.series_lane_overrides["channel_1"] = lane_override_1;

        SeriesLaneOverrideData lane_override_2;
        lane_override_2.lane_id = "lane_b";
        lane_override_2.overlay_mode = LaneOverlayMode::Separate;
        state.series_lane_overrides["spikes_1"] = lane_override_2;

        LaneOverrideData lane_meta;
        lane_meta.display_label = "Motor lane";
        lane_meta.lane_weight = 2.0f;
        state.lane_overrides["lane_a"] = lane_meta;

        StackableOrderingConstraintData c1;
        c1.above_series_key = "spikes_1";
        c1.below_series_key = "channel_1";
        state.ordering_constraints.push_back(c1);

        StackableOrderingConstraintData c2;
        c2.above_series_key = "channel_2";
        c2.below_series_key = "spikes_1";
        state.ordering_constraints.push_back(c2);

        // Serialize and deserialize
        auto json = rfl::json::write(state);
        auto result = rfl::json::read<DataViewerStateData>(json);

        REQUIRE(result);
        auto const & data = result.value();

        // Verify identity
        REQUIRE(data.instance_id == "viewer-abc");
        REQUIRE(data.display_name == "Neural Data Viewer");

        // Verify view
        REQUIRE(data.view.x_min == Approx(5000.0));
        REQUIRE(data.view.x_max == Approx(50000.0));
        REQUIRE(data.global_y_scale == Approx(2.0f));

        // Verify theme
        REQUIRE(data.theme.theme == DataViewerTheme::Light);
        REQUIRE(data.theme.background_color == "#F5F5F5");

        // Verify grid
        REQUIRE(data.grid.enabled == true);
        REQUIRE(data.grid.spacing == 250);

        // Verify UI
        REQUIRE(data.ui.zoom_scaling_mode == DataViewerZoomScalingMode::Fixed);
        REQUIRE(data.ui.properties_panel_collapsed == true);

        // Verify interaction
        REQUIRE(data.interaction.mode == DataViewerInteractionMode::CreateInterval);

        // Verify analog options
        REQUIRE(data.analog_options.size() == 2);
        REQUIRE(data.analog_options.at("channel_1").hex_color() == "#0000ff");
        REQUIRE(data.analog_options.at("channel_1").user_scale_factor == Approx(1.5f));
        REQUIRE(data.analog_options.at("channel_1").gap_handling == AnalogGapHandlingMode::DetectGaps);
        REQUIRE(data.analog_options.at("channel_2").hex_color() == "#ff0000");
        REQUIRE(data.analog_options.at("channel_2").get_is_visible() == false);

        // Verify event options
        REQUIRE(data.event_options.size() == 1);
        REQUIRE(data.event_options.at("spikes_1").hex_color() == "#ff9500");
        REQUIRE(data.event_options.at("spikes_1").plotting_mode == EventPlottingModeData::Stacked);
        REQUIRE(data.event_options.at("spikes_1").glyph_shape == EventGlyphShapeData::Box);
        REQUIRE(data.event_options.at("spikes_1").box_width_ticks == 16);

        // Verify interval options
        REQUIRE(data.interval_options.size() == 1);
        REQUIRE(data.interval_options.at("trial_markers").hex_color() == "#00ff00");
        REQUIRE(data.interval_options.at("trial_markers").get_alpha() == Approx(0.4f));

        // Verify mixed-lane overrides
        REQUIRE(data.series_lane_overrides.size() == 2);
        REQUIRE(data.series_lane_overrides.at("channel_1").lane_id == "lane_a");
        REQUIRE(data.series_lane_overrides.at("channel_1").lane_order.has_value());
        REQUIRE(data.series_lane_overrides.at("channel_1").lane_order == std::optional<int>{10});
        REQUIRE(data.series_lane_overrides.at("channel_1").lane_weight == Approx(1.2f));
        REQUIRE(data.series_lane_overrides.at("channel_1").overlay_mode == LaneOverlayMode::Overlay);
        REQUIRE(data.series_lane_overrides.at("channel_1").overlay_z == 1);
        REQUIRE(data.series_lane_overrides.at("spikes_1").overlay_mode == LaneOverlayMode::Separate);

        REQUIRE(data.lane_overrides.size() == 1);
        REQUIRE(data.lane_overrides.at("lane_a").display_label == "Motor lane");
        REQUIRE(data.lane_overrides.at("lane_a").lane_weight.has_value());
        REQUIRE(data.lane_overrides.at("lane_a").lane_weight.value_or(-1.0f) == Approx(2.0f));

        REQUIRE(data.ordering_constraints.size() == 2);
        REQUIRE(data.ordering_constraints[0].above_series_key == "spikes_1");
        REQUIRE(data.ordering_constraints[0].below_series_key == "channel_1");
        REQUIRE(data.ordering_constraints[1].above_series_key == "channel_2");
        REQUIRE(data.ordering_constraints[1].below_series_key == "spikes_1");
    }

    SECTION("JSON structure validation") {
        DataViewerStateData state;
        state.instance_id = "test";
        state.view.x_min = 100.0;
        state.theme.theme = DataViewerTheme::Dark;

        AnalogSeriesOptionsData opts;
        opts.hex_color() = "#123456";
        state.analog_options["test_series"] = opts;

        auto json = rfl::json::write(state);

        // Verify nested structure for view, theme, grid, ui, interaction
        REQUIRE(json.find("\"view\":{") != std::string::npos);
        REQUIRE(json.find("\"theme\":{") != std::string::npos);
        REQUIRE(json.find("\"grid\":{") != std::string::npos);
        REQUIRE(json.find("\"ui\":{") != std::string::npos);
        REQUIRE(json.find("\"interaction\":{") != std::string::npos);

        // Verify analog_options is a map
        REQUIRE(json.find("\"analog_options\":{") != std::string::npos);
        REQUIRE(json.find("\"test_series\":{") != std::string::npos);

        // Verify mixed-lane maps are present in serialized schema
        REQUIRE(json.find("\"series_lane_overrides\":{") != std::string::npos);
        REQUIRE(json.find("\"lane_overrides\":{") != std::string::npos);
        REQUIRE(json.find("\"ordering_constraints\":[") != std::string::npos);

        // Verify rfl::Flatten produces flat structure within series options
        // (hex_color should NOT be nested under "style")
        REQUIRE(json.find("\"hex_color\":\"#123456\"") != std::string::npos);
    }
}

// ==================== Edge Cases ====================

TEST_CASE("DataViewerStateData edge cases", "[DataViewerStateData]") {
    SECTION("Empty maps serialize correctly") {
        DataViewerStateData state;
        state.instance_id = "empty-test";

        auto json = rfl::json::write(state);
        auto result = rfl::json::read<DataViewerStateData>(json);

        REQUIRE(result);
        REQUIRE(result.value().analog_options.empty());
        REQUIRE(result.value().event_options.empty());
        REQUIRE(result.value().interval_options.empty());
        REQUIRE(result.value().series_lane_overrides.empty());
        REQUIRE(result.value().lane_overrides.empty());
        REQUIRE(result.value().ordering_constraints.empty());
    }

    SECTION("Special characters in keys") {
        DataViewerStateData state;
        state.instance_id = "special-chars";

        AnalogSeriesOptionsData opts;
        opts.hex_color() = "#abcdef";
        state.analog_options["series with spaces"] = opts;
        state.analog_options["series/with/slashes"] = opts;
        state.analog_options["series_with_underscores"] = opts;

        auto json = rfl::json::write(state);
        auto result = rfl::json::read<DataViewerStateData>(json);

        REQUIRE(result);
        REQUIRE(result.value().analog_options.size() == 3);
        REQUIRE(result.value().analog_options.count("series with spaces") == 1);
        REQUIRE(result.value().analog_options.count("series/with/slashes") == 1);
        REQUIRE(result.value().analog_options.count("series_with_underscores") == 1);
    }

    SECTION("Unicode in display name") {
        DataViewerStateData state;
        state.instance_id = "unicode-test";
        state.display_name = "データビューア 📊";

        auto json = rfl::json::write(state);
        auto result = rfl::json::read<DataViewerStateData>(json);

        REQUIRE(result);
        REQUIRE(result.value().display_name == "データビューア 📊");
    }
}
