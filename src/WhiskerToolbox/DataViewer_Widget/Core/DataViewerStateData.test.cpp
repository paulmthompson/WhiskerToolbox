#include "DataViewerStateData.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

using Catch::Approx;

// ==================== SeriesStyleData Tests ====================

TEST_CASE("SeriesStyleData serialization", "[DataViewerStateData]") {
    SECTION("Default values serialize correctly") {
        CorePlotting::SeriesStyle style;
        
        auto json = rfl::json::write(style);
        auto result = rfl::json::read<CorePlotting::SeriesStyle>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
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
        auto const& data = result.value();
        REQUIRE(data.hex_color == "#ff0000");
        REQUIRE(data.alpha == Approx(0.5f));
        REQUIRE(data.line_thickness == 3);
        REQUIRE(data.is_visible == false);
    }
}

// ==================== AnalogSeriesOptionsData Tests ====================

TEST_CASE("AnalogSeriesOptionsData serialization", "[DataViewerStateData]") {
    SECTION("Default values") {
        AnalogSeriesOptionsData opts;
        
        auto json = rfl::json::write(opts);
        auto result = rfl::json::read<AnalogSeriesOptionsData>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
        
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
        
        auto json = rfl::json::write(opts);
        auto result = rfl::json::read<AnalogSeriesOptionsData>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
        REQUIRE(data.hex_color() == "#ff00ff");
        REQUIRE(data.get_alpha() == Approx(0.8f));
        REQUIRE(data.get_line_thickness() == 2);
        REQUIRE(data.get_is_visible() == false);
        REQUIRE(data.user_scale_factor == Approx(2.5f));
        REQUIRE(data.y_offset == Approx(0.3f));
        REQUIRE(data.gap_handling == AnalogGapHandlingMode::ShowMarkers);
        REQUIRE(data.enable_gap_detection == true);
        REQUIRE(data.gap_threshold == Approx(10.0f));
    }
}

// ==================== DigitalEventSeriesOptionsData Tests ====================

TEST_CASE("DigitalEventSeriesOptionsData serialization", "[DataViewerStateData]") {
    SECTION("Default values") {
        DigitalEventSeriesOptionsData opts;
        
        auto json = rfl::json::write(opts);
        auto result = rfl::json::read<DigitalEventSeriesOptionsData>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
        
        REQUIRE(data.hex_color() == "#007bff");
        REQUIRE(data.get_is_visible() == true);
        REQUIRE(data.plotting_mode == EventPlottingModeData::FullCanvas);
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
    
    SECTION("Full event options round-trip") {
        DigitalEventSeriesOptionsData opts;
        opts.hex_color() = "#ff9500";
        opts.alpha() = 0.7f;
        opts.plotting_mode = EventPlottingModeData::Stacked;
        opts.vertical_spacing = 0.2f;
        opts.event_height = 0.1f;
        opts.margin_factor = 0.9f;
        
        auto json = rfl::json::write(opts);
        auto result = rfl::json::read<DigitalEventSeriesOptionsData>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
        REQUIRE(data.hex_color() == "#ff9500");
        REQUIRE(data.get_alpha() == Approx(0.7f));
        REQUIRE(data.plotting_mode == EventPlottingModeData::Stacked);
        REQUIRE(data.vertical_spacing == Approx(0.2f));
        REQUIRE(data.event_height == Approx(0.1f));
        REQUIRE(data.margin_factor == Approx(0.9f));
    }
}

// ==================== DigitalIntervalSeriesOptionsData Tests ====================

TEST_CASE("DigitalIntervalSeriesOptionsData serialization", "[DataViewerStateData]") {
    SECTION("Default values") {
        DigitalIntervalSeriesOptionsData opts;
        
        auto json = rfl::json::write(opts);
        auto result = rfl::json::read<DigitalIntervalSeriesOptionsData>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
        
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
        auto const& data = result.value();
        REQUIRE(data.hex_color() == "#ff6b6b");
        REQUIRE(data.get_alpha() == Approx(0.3f));
        REQUIRE(data.extend_full_canvas == false);
        REQUIRE(data.margin_factor == Approx(0.85f));
        REQUIRE(data.interval_height == Approx(0.5f));
    }
}

// ==================== DataViewerViewState Tests ====================

TEST_CASE("DataViewerViewState serialization", "[DataViewerStateData]") {
    SECTION("Default values") {
        CorePlotting::TimeSeriesViewState view;
        
        auto json = rfl::json::write(view);
        auto result = rfl::json::read<CorePlotting::TimeSeriesViewState>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
        REQUIRE(data.time_start == 0);
        REQUIRE(data.time_end == 1000);
        REQUIRE(data.y_min == Approx(-1.0f));
        REQUIRE(data.y_max == Approx(1.0f));
        REQUIRE(data.vertical_pan_offset == Approx(0.0f));
        REQUIRE(data.global_zoom == Approx(1.0f));
        REQUIRE(data.global_vertical_scale == Approx(1.0f));
    }
    
    SECTION("Custom values round-trip") {
        CorePlotting::TimeSeriesViewState view;
        view.time_start = 1000;
        view.time_end = 50000;
        view.y_min = -2.0f;
        view.y_max = 2.0f;
        view.vertical_pan_offset = 0.5f;
        view.global_zoom = 1.5f;
        view.global_vertical_scale = 0.8f;
        
        auto json = rfl::json::write(view);
        auto result = rfl::json::read<CorePlotting::TimeSeriesViewState>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
        REQUIRE(data.time_start == 1000);
        REQUIRE(data.time_end == 50000);
        REQUIRE(data.y_min == Approx(-2.0f));
        REQUIRE(data.y_max == Approx(2.0f));
        REQUIRE(data.vertical_pan_offset == Approx(0.5f));
        REQUIRE(data.global_zoom == Approx(1.5f));
        REQUIRE(data.global_vertical_scale == Approx(0.8f));
    }
    
    SECTION("Large time values (int64_t)") {
        CorePlotting::TimeSeriesViewState view;
        view.time_start = 1000000000LL;
        view.time_end = 9000000000LL;
        
        auto json = rfl::json::write(view);
        auto result = rfl::json::read<CorePlotting::TimeSeriesViewState>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
        REQUIRE(data.time_start == 1000000000LL);
        REQUIRE(data.time_end == 9000000000LL);
    }
}

// ==================== DataViewerThemeState Tests ====================

TEST_CASE("DataViewerThemeState serialization", "[DataViewerStateData]") {
    SECTION("Default values (Dark theme)") {
        DataViewerThemeState theme;
        
        auto json = rfl::json::write(theme);
        auto result = rfl::json::read<DataViewerThemeState>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
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
        auto const& data = result.value();
        REQUIRE(data.theme == DataViewerTheme::Light);
        REQUIRE(data.background_color == "#FFFFFF");
        REQUIRE(data.axis_color == "#333333");
    }
}

// ==================== DataViewerGridState Tests ====================

TEST_CASE("DataViewerGridState serialization", "[DataViewerStateData]") {
    SECTION("Default values") {
        DataViewerGridState grid;
        
        auto json = rfl::json::write(grid);
        auto result = rfl::json::read<DataViewerGridState>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
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
        auto const& data = result.value();
        REQUIRE(data.enabled == true);
        REQUIRE(data.spacing == 500);
    }
}

// ==================== DataViewerUIPreferences Tests ====================

TEST_CASE("DataViewerUIPreferences serialization", "[DataViewerStateData]") {
    SECTION("Default values") {
        DataViewerUIPreferences ui;
        
        auto json = rfl::json::write(ui);
        auto result = rfl::json::read<DataViewerUIPreferences>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
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
        auto const& data = result.value();
        REQUIRE(data.zoom_scaling_mode == DataViewerZoomScalingMode::Fixed);
        REQUIRE(data.properties_panel_collapsed == true);
    }
}

// ==================== DataViewerInteractionState Tests ====================

TEST_CASE("DataViewerInteractionState serialization", "[DataViewerStateData]") {
    SECTION("Default values") {
        DataViewerInteractionState interaction;
        
        auto json = rfl::json::write(interaction);
        auto result = rfl::json::read<DataViewerInteractionState>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
        REQUIRE(data.mode == DataViewerInteractionMode::Normal);
    }
    
    SECTION("DataViewerInteractionMode enum serializes as string") {
        DataViewerInteractionState interaction;
        interaction.mode = DataViewerInteractionMode::CreateInterval;
        
        auto json = rfl::json::write(interaction);
        
        REQUIRE(json.find("\"CreateInterval\"") != std::string::npos);
    }
    
    SECTION("All interaction modes round-trip") {
        std::vector<DataViewerInteractionMode> modes = {
            DataViewerInteractionMode::Normal,
            DataViewerInteractionMode::CreateInterval,
            DataViewerInteractionMode::ModifyInterval,
            DataViewerInteractionMode::CreateLine
        };
        
        for (auto mode : modes) {
            DataViewerInteractionState interaction;
            interaction.mode = mode;
            
            auto json = rfl::json::write(interaction);
            auto result = rfl::json::read<DataViewerInteractionState>(json);
            
            REQUIRE(result);
            REQUIRE(result.value().mode == mode);
        }
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
        auto const& data = result.value();
        
        REQUIRE(data.instance_id == "test-123");
        REQUIRE(data.display_name == "Data Viewer");
        
        // Nested objects should have defaults
        REQUIRE(data.view.time_start == 0);
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
        state.view.time_start = 5000;
        state.view.time_end = 50000;
        state.view.global_zoom = 2.0f;
        
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
        state.event_options["spikes_1"] = event1;
        
        // Add interval series options
        DigitalIntervalSeriesOptionsData interval1;
        interval1.hex_color() = "#00ff00";
        interval1.alpha() = 0.4f;
        state.interval_options["trial_markers"] = interval1;
        
        // Serialize and deserialize
        auto json = rfl::json::write(state);
        auto result = rfl::json::read<DataViewerStateData>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
        
        // Verify identity
        REQUIRE(data.instance_id == "viewer-abc");
        REQUIRE(data.display_name == "Neural Data Viewer");
        
        // Verify view
        REQUIRE(data.view.time_start == 5000);
        REQUIRE(data.view.time_end == 50000);
        REQUIRE(data.view.global_zoom == Approx(2.0f));
        
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
        
        // Verify interval options
        REQUIRE(data.interval_options.size() == 1);
        REQUIRE(data.interval_options.at("trial_markers").hex_color() == "#00ff00");
        REQUIRE(data.interval_options.at("trial_markers").get_alpha() == Approx(0.4f));
    }
    
    SECTION("JSON structure validation") {
        DataViewerStateData state;
        state.instance_id = "test";
        state.view.time_start = 100;
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
