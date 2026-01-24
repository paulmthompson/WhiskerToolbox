/**
 * @file DataViewerState.test.cpp
 * @brief Unit tests for DataViewerState
 * 
 * Tests the EditorState subclass for DataViewer_Widget, including:
 * - Typed accessors for all state properties
 * - Signal emission verification
 * - JSON serialization/deserialization round-trip
 * - Registry integration
 */

#include "DataViewer_Widget/DataViewerState.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <QSignalSpy>

// ==================== Construction Tests ====================

TEST_CASE("DataViewerState construction", "[DataViewerState]")
{
    SECTION("default construction creates valid state") {
        DataViewerState state;
        
        REQUIRE(state.getTypeName() == "DataViewer");
        REQUIRE(state.getDisplayName() == "Data Viewer");
        REQUIRE_FALSE(state.getInstanceId().isEmpty());
        REQUIRE_FALSE(state.isDirty());
    }
    
    SECTION("instance IDs are unique") {
        DataViewerState state1;
        DataViewerState state2;
        
        REQUIRE(state1.getInstanceId() != state2.getInstanceId());
    }
}

// ==================== Display Name Tests ====================

TEST_CASE("DataViewerState display name", "[DataViewerState]")
{
    DataViewerState state;
    
    SECTION("setDisplayName changes name") {
        state.setDisplayName("My Custom Viewer");
        REQUIRE(state.getDisplayName() == "My Custom Viewer");
    }
    
    SECTION("setDisplayName emits signal") {
        QSignalSpy spy(&state, &DataViewerState::displayNameChanged);
        
        state.setDisplayName("New Name");
        
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toString() == "New Name");
    }
    
    SECTION("setDisplayName marks dirty") {
        state.markClean();
        REQUIRE_FALSE(state.isDirty());
        
        state.setDisplayName("Changed");
        REQUIRE(state.isDirty());
    }
    
    SECTION("setting same name does not emit signal") {
        state.setDisplayName("Test");
        QSignalSpy spy(&state, &DataViewerState::displayNameChanged);
        
        state.setDisplayName("Test");
        
        REQUIRE(spy.count() == 0);
    }
}

// ==================== View State Tests ====================

TEST_CASE("DataViewerState view state", "[DataViewerState]")
{
    DataViewerState state;
    
    SECTION("setTimeWindow changes values") {
        state.setTimeWindow(100, 5000);
        
        auto [start, end] = state.timeWindow();
        REQUIRE(start == 100);
        REQUIRE(end == 5000);
    }
    
    SECTION("setTimeWindow emits viewStateChanged") {
        QSignalSpy spy(&state, &DataViewerState::viewStateChanged);
        
        state.setTimeWindow(0, 10000);
        
        REQUIRE(spy.count() == 1);
    }
    
    SECTION("setYBounds changes values") {
        state.setYBounds(-2.0f, 2.0f);
        
        auto [y_min, y_max] = state.yBounds();
        REQUIRE_THAT(y_min, Catch::Matchers::WithinAbs(-2.0f, 1e-6f));
        REQUIRE_THAT(y_max, Catch::Matchers::WithinAbs(2.0f, 1e-6f));
    }
    
    SECTION("setVerticalPanOffset changes value") {
        state.setVerticalPanOffset(0.5f);
        REQUIRE_THAT(state.verticalPanOffset(), Catch::Matchers::WithinAbs(0.5f, 1e-6f));
    }
    
    SECTION("setGlobalZoom changes value") {
        state.setGlobalZoom(2.5f);
        REQUIRE_THAT(state.globalZoom(), Catch::Matchers::WithinAbs(2.5f, 1e-6f));
    }
    
    SECTION("setGlobalVerticalScale changes value") {
        state.setGlobalVerticalScale(1.5f);
        REQUIRE_THAT(state.globalVerticalScale(), Catch::Matchers::WithinAbs(1.5f, 1e-6f));
    }
    
    SECTION("setViewState changes all values") {
        CorePlotting::TimeSeriesViewState view;
        view.time_start = 500;
        view.time_end = 1500;
        view.y_min = -3.0f;
        view.y_max = 3.0f;
        view.global_zoom = 2.0f;
        
        state.setViewState(view);
        
        REQUIRE(state.viewState().time_start == 500);
        REQUIRE(state.viewState().time_end == 1500);
        REQUIRE_THAT(state.viewState().y_min, Catch::Matchers::WithinAbs(-3.0f, 1e-6f));
    }
    
    SECTION("setting same values does not emit signal") {
        state.setTimeWindow(0, 1000);
        QSignalSpy spy(&state, &DataViewerState::viewStateChanged);
        
        state.setTimeWindow(0, 1000);
        
        REQUIRE(spy.count() == 0);
    }
}

// ==================== Theme Tests ====================

TEST_CASE("DataViewerState theme", "[DataViewerState]")
{
    DataViewerState state;
    
    SECTION("setTheme changes value") {
        state.setTheme(DataViewerTheme::Light);
        REQUIRE(state.theme() == DataViewerTheme::Light);
    }
    
    SECTION("setTheme emits themeChanged") {
        QSignalSpy spy(&state, &DataViewerState::themeChanged);
        
        state.setTheme(DataViewerTheme::Light);
        
        REQUIRE(spy.count() == 1);
    }
    
    SECTION("setBackgroundColor changes value") {
        state.setBackgroundColor("#1a1a1a");
        REQUIRE(state.backgroundColor() == "#1a1a1a");
    }
    
    SECTION("setAxisColor changes value") {
        state.setAxisColor("#cccccc");
        REQUIRE(state.axisColor() == "#cccccc");
    }
    
    SECTION("setThemeState changes all values") {
        DataViewerThemeState theme_state;
        theme_state.theme = DataViewerTheme::Light;
        theme_state.background_color = "#ffffff";
        theme_state.axis_color = "#000000";
        
        state.setThemeState(theme_state);
        
        REQUIRE(state.theme() == DataViewerTheme::Light);
        REQUIRE(state.backgroundColor() == "#ffffff");
        REQUIRE(state.axisColor() == "#000000");
    }
}

// ==================== Grid Tests ====================

TEST_CASE("DataViewerState grid", "[DataViewerState]")
{
    DataViewerState state;
    
    SECTION("setGridEnabled changes value") {
        REQUIRE_FALSE(state.gridEnabled());  // Default is false
        
        state.setGridEnabled(true);
        REQUIRE(state.gridEnabled());
    }
    
    SECTION("setGridEnabled emits gridChanged") {
        QSignalSpy spy(&state, &DataViewerState::gridChanged);
        
        state.setGridEnabled(true);
        
        REQUIRE(spy.count() == 1);
    }
    
    SECTION("setGridSpacing changes value") {
        state.setGridSpacing(200);
        REQUIRE(state.gridSpacing() == 200);
    }
    
    SECTION("setGridState changes all values") {
        DataViewerGridState grid_state;
        grid_state.enabled = true;
        grid_state.spacing = 50;
        
        state.setGridState(grid_state);
        
        REQUIRE(state.gridEnabled());
        REQUIRE(state.gridSpacing() == 50);
    }
}

// ==================== UI Preferences Tests ====================

TEST_CASE("DataViewerState UI preferences", "[DataViewerState]")
{
    DataViewerState state;
    
    SECTION("setZoomScalingMode changes value") {
        state.setZoomScalingMode(DataViewerZoomScalingMode::Fixed);
        REQUIRE(state.zoomScalingMode() == DataViewerZoomScalingMode::Fixed);
    }
    
    SECTION("setZoomScalingMode emits uiPreferencesChanged") {
        QSignalSpy spy(&state, &DataViewerState::uiPreferencesChanged);
        
        state.setZoomScalingMode(DataViewerZoomScalingMode::Fixed);
        
        REQUIRE(spy.count() == 1);
    }
    
    SECTION("setPropertiesPanelCollapsed changes value") {
        REQUIRE_FALSE(state.propertiesPanelCollapsed());  // Default
        
        state.setPropertiesPanelCollapsed(true);
        REQUIRE(state.propertiesPanelCollapsed());
    }
    
    SECTION("setUIPreferences changes all values") {
        DataViewerUIPreferences prefs;
        prefs.zoom_scaling_mode = DataViewerZoomScalingMode::Fixed;
        prefs.properties_panel_collapsed = true;
        
        state.setUIPreferences(prefs);
        
        REQUIRE(state.zoomScalingMode() == DataViewerZoomScalingMode::Fixed);
        REQUIRE(state.propertiesPanelCollapsed());
    }
}

// ==================== Interaction Tests ====================

TEST_CASE("DataViewerState interaction", "[DataViewerState]")
{
    DataViewerState state;
    
    SECTION("default interaction mode is Normal") {
        REQUIRE(state.interactionMode() == DataViewerInteractionMode::Normal);
    }
    
    SECTION("setInteractionMode changes value") {
        state.setInteractionMode(DataViewerInteractionMode::CreateInterval);
        REQUIRE(state.interactionMode() == DataViewerInteractionMode::CreateInterval);
    }
    
    SECTION("setInteractionMode emits interactionModeChanged") {
        QSignalSpy spy(&state, &DataViewerState::interactionModeChanged);
        
        state.setInteractionMode(DataViewerInteractionMode::CreateLine);
        
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).value<DataViewerInteractionMode>() == DataViewerInteractionMode::CreateLine);
    }
    
    SECTION("setting same mode does not emit signal") {
        state.setInteractionMode(DataViewerInteractionMode::Normal);
        QSignalSpy spy(&state, &DataViewerState::interactionModeChanged);
        
        state.setInteractionMode(DataViewerInteractionMode::Normal);
        
        REQUIRE(spy.count() == 0);
    }
}

// ==================== Series Options Registry Integration ====================

TEST_CASE("DataViewerState registry integration", "[DataViewerState]")
{
    DataViewerState state;
    
    SECTION("seriesOptions returns valid registry") {
        auto& registry = state.seriesOptions();
        
        // Should be able to use registry
        AnalogSeriesOptionsData opts;
        opts.hex_color() = "#ff0000";
        registry.set("channel_1", opts);
        
        REQUIRE(registry.has<AnalogSeriesOptionsData>("channel_1"));
    }
    
    SECTION("registry changes emit state signals") {
        QSignalSpy spy(&state, &DataViewerState::seriesOptionsChanged);
        
        AnalogSeriesOptionsData opts;
        state.seriesOptions().set("channel_1", opts);
        
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == "channel_1");
        REQUIRE(spy.at(0).at(1).toString() == "analog");
    }
    
    SECTION("registry changes mark state dirty") {
        state.markClean();
        
        AnalogSeriesOptionsData opts;
        state.seriesOptions().set("channel_1", opts);
        
        REQUIRE(state.isDirty());
    }
    
    SECTION("registry remove emits seriesOptionsRemoved") {
        AnalogSeriesOptionsData opts;
        state.seriesOptions().set("channel_1", opts);
        
        QSignalSpy spy(&state, &DataViewerState::seriesOptionsRemoved);
        state.seriesOptions().remove<AnalogSeriesOptionsData>("channel_1");
        
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == "channel_1");
    }
    
    SECTION("registry visibility change emits seriesVisibilityChanged") {
        AnalogSeriesOptionsData opts;
        opts.is_visible() = true;
        state.seriesOptions().set("channel_1", opts);
        
        QSignalSpy spy(&state, &DataViewerState::seriesVisibilityChanged);
        state.seriesOptions().setVisible("channel_1", "analog", false);
        
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == "channel_1");
        REQUIRE(spy.at(0).at(2).toBool() == false);
    }
}

// ==================== Serialization Tests ====================

TEST_CASE("DataViewerState serialization", "[DataViewerState]")
{
    SECTION("round-trip preserves all state") {
        DataViewerState original;
        
        // Set various state
        original.setDisplayName("Test Viewer");
        original.setTimeWindow(100, 5000);
        original.setYBounds(-2.0f, 2.0f);
        original.setGlobalZoom(1.5f);
        original.setTheme(DataViewerTheme::Light);
        original.setBackgroundColor("#ffffff");
        original.setGridEnabled(true);
        original.setGridSpacing(200);
        original.setZoomScalingMode(DataViewerZoomScalingMode::Fixed);
        original.setPropertiesPanelCollapsed(true);
        original.setInteractionMode(DataViewerInteractionMode::CreateInterval);
        
        // Add series options
        AnalogSeriesOptionsData analog_opts;
        analog_opts.hex_color() = "#ff0000";
        analog_opts.user_scale_factor = 2.0f;
        original.seriesOptions().set("channel_1", analog_opts);
        
        DigitalEventSeriesOptionsData event_opts;
        event_opts.hex_color() = "#00ff00";
        original.seriesOptions().set("events_1", event_opts);
        
        // Serialize
        std::string json = original.toJson();
        REQUIRE_FALSE(json.empty());
        
        // Deserialize into new state
        DataViewerState restored;
        REQUIRE(restored.fromJson(json));
        
        // Verify all state restored
        REQUIRE(restored.getInstanceId() == original.getInstanceId());
        REQUIRE(restored.getDisplayName() == "Test Viewer");
        
        auto [start, end] = restored.timeWindow();
        REQUIRE(start == 100);
        REQUIRE(end == 5000);
        
        REQUIRE_THAT(restored.globalZoom(), Catch::Matchers::WithinAbs(1.5f, 1e-6f));
        REQUIRE(restored.theme() == DataViewerTheme::Light);
        REQUIRE(restored.backgroundColor() == "#ffffff");
        REQUIRE(restored.gridEnabled());
        REQUIRE(restored.gridSpacing() == 200);
        REQUIRE(restored.zoomScalingMode() == DataViewerZoomScalingMode::Fixed);
        REQUIRE(restored.propertiesPanelCollapsed());
        REQUIRE(restored.interactionMode() == DataViewerInteractionMode::CreateInterval);
        
        // Verify series options restored
        REQUIRE(restored.seriesOptions().has<AnalogSeriesOptionsData>("channel_1"));
        auto* restored_analog = restored.seriesOptions().get<AnalogSeriesOptionsData>("channel_1");
        REQUIRE(restored_analog->hex_color() == "#ff0000");
        REQUIRE_THAT(restored_analog->user_scale_factor, Catch::Matchers::WithinAbs(2.0f, 1e-6f));
        
        REQUIRE(restored.seriesOptions().has<DigitalEventSeriesOptionsData>("events_1"));
    }
    
    SECTION("fromJson returns false on invalid JSON") {
        DataViewerState state;
        
        REQUIRE_FALSE(state.fromJson("not valid json"));
        REQUIRE_FALSE(state.fromJson("{\"invalid\": \"structure\"}"));
    }
    
    SECTION("fromJson emits signals") {
        DataViewerState original;
        original.setTimeWindow(100, 5000);
        original.setTheme(DataViewerTheme::Light);
        std::string json = original.toJson();
        
        DataViewerState restored;
        QSignalSpy state_spy(&restored, &DataViewerState::stateChanged);
        QSignalSpy view_spy(&restored, &DataViewerState::viewStateChanged);
        QSignalSpy theme_spy(&restored, &DataViewerState::themeChanged);
        
        REQUIRE(restored.fromJson(json));
        
        REQUIRE(state_spy.count() == 1);
        REQUIRE(view_spy.count() == 1);
        REQUIRE(theme_spy.count() == 1);
    }
}

// ==================== Direct Data Access ====================

TEST_CASE("DataViewerState direct data access", "[DataViewerState]")
{
    DataViewerState state;
    
    SECTION("data() returns const reference to underlying data") {
        state.setTimeWindow(100, 5000);
        
        auto const& data = state.data();
        REQUIRE(data.view.time_start == 100);
        REQUIRE(data.view.time_end == 5000);
    }
    
    SECTION("data() reflects series options changes") {
        AnalogSeriesOptionsData opts;
        opts.hex_color() = "#ff0000";
        state.seriesOptions().set("ch1", opts);
        
        auto const& data = state.data();
        REQUIRE(data.analog_options.contains("ch1"));
        REQUIRE(data.analog_options.at("ch1").hex_color() == "#ff0000");
    }
}
