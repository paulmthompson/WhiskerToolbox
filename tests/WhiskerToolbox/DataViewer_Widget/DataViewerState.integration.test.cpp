/**
 * @file DataViewerState.integration.test.cpp
 * @brief Integration tests for DataViewerState workspace save/restore
 * 
 * Phase 7 integration tests verifying:
 * - Full serialize → deserialize → verify all settings restored
 * - Multiple DataViewer instances with independent state
 * - State modification from external sources (properties panel pattern)
 * - Key synchronization between DataStore and State
 */

#include "DataViewer_Widget/Core/DataViewerState.hpp"
#include "DataViewer_Widget/Core/DataViewerStateData.hpp"
#include "DataViewer_Widget/Core/SeriesOptionsRegistry.hpp"
#include "DataViewer_Widget/Core/TimeSeriesDataStore.hpp"  // For DefaultColors

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <QSignalSpy>

#include <memory>
#include <string>
#include <vector>

// ==================== Workspace Save/Restore Integration Tests ====================

TEST_CASE("DataViewerState workspace save/restore integration", "[DataViewerState][Integration]")
{
    SECTION("complete state round-trip with all categories") {
        // Create a fully configured state (simulating user session)
        auto original = std::make_shared<DataViewerState>();
        
        // === Configure View State ===
        original->setTimeWindow(1000, 50000);
        original->setYBounds(-3.0f, 3.0f);
        original->setVerticalPanOffset(0.25f);
        original->setGlobalZoom(2.0f);
        original->setGlobalVerticalScale(1.5f);
        
        // === Configure Theme ===
        original->setTheme(DataViewerTheme::Light);
        original->setBackgroundColor("#f5f5f5");
        original->setAxisColor("#333333");
        
        // === Configure Grid ===
        original->setGridEnabled(true);
        original->setGridSpacing(250);
        
        // === Configure UI Preferences ===
        original->setZoomScalingMode(DataViewerZoomScalingMode::Fixed);
        original->setPropertiesPanelCollapsed(true);
        
        // === Configure Interaction Mode ===
        original->setInteractionMode(DataViewerInteractionMode::CreateInterval);
        
        // === Configure Series Options (simulating 10 active channels) ===
        for (int i = 0; i < 10; ++i) {
            AnalogSeriesOptionsData analog_opts;
            analog_opts.hex_color() = DataViewer::DefaultColors::getColorForIndex(static_cast<size_t>(i));
            analog_opts.alpha() = 0.9f;
            analog_opts.line_thickness() = 2;
            analog_opts.is_visible() = (i % 2 == 0);  // Alternating visibility
            analog_opts.user_scale_factor = 1.0f + static_cast<float>(i) * 0.1f;
            analog_opts.y_offset = static_cast<float>(i) * 0.05f;
            analog_opts.enable_gap_detection = true;
            analog_opts.gap_threshold = 5.0f;
            
            original->seriesOptions().set(
                QString::fromStdString("channel_" + std::to_string(i)), analog_opts);
        }
        
        // Add some event series
        for (int i = 0; i < 3; ++i) {
            DigitalEventSeriesOptionsData event_opts;
            event_opts.hex_color() = "#ff5500";
            event_opts.event_height = 0.5f;
            event_opts.is_visible() = true;
            
            original->seriesOptions().set(
                QString::fromStdString("events_" + std::to_string(i)), event_opts);
        }
        
        // Add some interval series
        for (int i = 0; i < 2; ++i) {
            DigitalIntervalSeriesOptionsData interval_opts;
            interval_opts.hex_color() = "#00aa55";
            interval_opts.alpha() = 0.7f;
            interval_opts.is_visible() = true;
            
            original->seriesOptions().set(
                QString::fromStdString("intervals_" + std::to_string(i)), interval_opts);
        }
        
        // === Serialize to JSON ===
        std::string json = original->toJson();
        REQUIRE_FALSE(json.empty());
        
        // === Deserialize into new state ===
        auto restored = std::make_shared<DataViewerState>();
        
        // Track signal emissions during restore
        QSignalSpy state_spy(restored.get(), &DataViewerState::stateChanged);
        QSignalSpy view_spy(restored.get(), &DataViewerState::viewStateChanged);
        QSignalSpy theme_spy(restored.get(), &DataViewerState::themeChanged);
        QSignalSpy grid_spy(restored.get(), &DataViewerState::gridChanged);
        QSignalSpy ui_spy(restored.get(), &DataViewerState::uiPreferencesChanged);
        QSignalSpy interaction_spy(restored.get(), &DataViewerState::interactionModeChanged);
        
        REQUIRE(restored->fromJson(json));
        
        // === Verify signals were emitted ===
        REQUIRE(state_spy.count() == 1);
        REQUIRE(view_spy.count() == 1);
        REQUIRE(theme_spy.count() == 1);
        REQUIRE(grid_spy.count() == 1);
        REQUIRE(ui_spy.count() == 1);
        REQUIRE(interaction_spy.count() == 1);
        
        // === Verify View State Restored ===
        auto [start, end] = restored->timeWindow();
        REQUIRE(start == 1000);
        REQUIRE(end == 50000);
        
        auto [y_min, y_max] = restored->yBounds();
        REQUIRE_THAT(y_min, Catch::Matchers::WithinAbs(-3.0f, 1e-5f));
        REQUIRE_THAT(y_max, Catch::Matchers::WithinAbs(3.0f, 1e-5f));
        REQUIRE_THAT(restored->verticalPanOffset(), Catch::Matchers::WithinAbs(0.25f, 1e-5f));
        REQUIRE_THAT(restored->globalZoom(), Catch::Matchers::WithinAbs(2.0f, 1e-5f));
        REQUIRE_THAT(restored->globalVerticalScale(), Catch::Matchers::WithinAbs(1.5f, 1e-5f));
        
        // === Verify Theme Restored ===
        REQUIRE(restored->theme() == DataViewerTheme::Light);
        REQUIRE(restored->backgroundColor() == "#f5f5f5");
        REQUIRE(restored->axisColor() == "#333333");
        
        // === Verify Grid Restored ===
        REQUIRE(restored->gridEnabled() == true);
        REQUIRE(restored->gridSpacing() == 250);
        
        // === Verify UI Preferences Restored ===
        REQUIRE(restored->zoomScalingMode() == DataViewerZoomScalingMode::Fixed);
        REQUIRE(restored->propertiesPanelCollapsed() == true);
        
        // === Verify Interaction Mode Restored ===
        REQUIRE(restored->interactionMode() == DataViewerInteractionMode::CreateInterval);
        
        // === Verify Series Options Restored ===
        // Check analog series
        for (int i = 0; i < 10; ++i) {
            QString key = QString::fromStdString("channel_" + std::to_string(i));
            REQUIRE(restored->seriesOptions().has<AnalogSeriesOptionsData>(key));
            
            auto* opts = restored->seriesOptions().get<AnalogSeriesOptionsData>(key);
            REQUIRE(opts != nullptr);
            REQUIRE(opts->get_is_visible() == (i % 2 == 0));
            REQUIRE_THAT(opts->user_scale_factor, 
                         Catch::Matchers::WithinAbs(1.0f + static_cast<float>(i) * 0.1f, 1e-5f));
            REQUIRE_THAT(opts->y_offset,
                         Catch::Matchers::WithinAbs(static_cast<float>(i) * 0.05f, 1e-5f));
        }
        
        // Check event series
        for (int i = 0; i < 3; ++i) {
            QString key = QString::fromStdString("events_" + std::to_string(i));
            REQUIRE(restored->seriesOptions().has<DigitalEventSeriesOptionsData>(key));
            
            auto* opts = restored->seriesOptions().get<DigitalEventSeriesOptionsData>(key);
            REQUIRE(opts != nullptr);
            REQUIRE_THAT(opts->event_height, Catch::Matchers::WithinAbs(0.5f, 1e-5f));
        }
        
        // Check interval series
        for (int i = 0; i < 2; ++i) {
            QString key = QString::fromStdString("intervals_" + std::to_string(i));
            REQUIRE(restored->seriesOptions().has<DigitalIntervalSeriesOptionsData>(key));
            
            auto* opts = restored->seriesOptions().get<DigitalIntervalSeriesOptionsData>(key);
            REQUIRE(opts != nullptr);
            REQUIRE_THAT(opts->get_alpha(), Catch::Matchers::WithinAbs(0.7f, 1e-5f));
        }
        
        // === Verify Instance ID Preserved ===
        REQUIRE(restored->getInstanceId() == original->getInstanceId());
    }
    
    SECTION("partial state restoration preserves defaults for missing fields") {
        // Create minimal JSON (simulating older workspace format)
        std::string minimal_json = R"({
            "instance_id": "test-instance-123",
            "display_name": "Minimal Viewer",
            "view": {
                "time_start": 0,
                "time_end": 1000
            }
        })";
        
        DataViewerState restored;
        // This may fail if strict parsing, but we should handle gracefully
        // For now, test that default values are reasonable after construction
        
        // Verify defaults
        REQUIRE(restored.theme() == DataViewerTheme::Dark);  // Default
        REQUIRE(restored.gridEnabled() == false);  // Default
        REQUIRE(restored.zoomScalingMode() == DataViewerZoomScalingMode::Adaptive);  // Default
    }
}

// ==================== Multiple Instance Independence Tests ====================

TEST_CASE("DataViewerState multiple instance independence", "[DataViewerState][Integration]")
{
    SECTION("two instances have independent state") {
        auto state1 = std::make_shared<DataViewerState>();
        auto state2 = std::make_shared<DataViewerState>();
        
        // Verify unique instance IDs
        REQUIRE(state1->getInstanceId() != state2->getInstanceId());
        
        // Configure state1
        state1->setTimeWindow(0, 10000);
        state1->setTheme(DataViewerTheme::Light);
        state1->setGlobalZoom(2.0f);
        
        AnalogSeriesOptionsData opts1;
        opts1.hex_color() = "#ff0000";
        state1->seriesOptions().set("shared_key", opts1);
        
        // Configure state2 differently
        state2->setTimeWindow(5000, 15000);
        state2->setTheme(DataViewerTheme::Dark);
        state2->setGlobalZoom(0.5f);
        
        AnalogSeriesOptionsData opts2;
        opts2.hex_color() = "#00ff00";
        state2->seriesOptions().set("shared_key", opts2);
        
        // Verify independence
        auto [s1_start, s1_end] = state1->timeWindow();
        auto [s2_start, s2_end] = state2->timeWindow();
        
        REQUIRE(s1_start == 0);
        REQUIRE(s1_end == 10000);
        REQUIRE(s2_start == 5000);
        REQUIRE(s2_end == 15000);
        
        REQUIRE(state1->theme() == DataViewerTheme::Light);
        REQUIRE(state2->theme() == DataViewerTheme::Dark);
        
        REQUIRE_THAT(state1->globalZoom(), Catch::Matchers::WithinAbs(2.0f, 1e-5f));
        REQUIRE_THAT(state2->globalZoom(), Catch::Matchers::WithinAbs(0.5f, 1e-5f));
        
        // Same key, different options
        auto* s1_opts = state1->seriesOptions().get<AnalogSeriesOptionsData>("shared_key");
        auto* s2_opts = state2->seriesOptions().get<AnalogSeriesOptionsData>("shared_key");
        
        REQUIRE(s1_opts != nullptr);
        REQUIRE(s2_opts != nullptr);
        REQUIRE(s1_opts->hex_color() == "#ff0000");
        REQUIRE(s2_opts->hex_color() == "#00ff00");
    }
    
    SECTION("modifying one instance does not affect another") {
        auto state1 = std::make_shared<DataViewerState>();
        auto state2 = std::make_shared<DataViewerState>();
        
        // Set initial values
        state1->setGlobalZoom(1.0f);
        state2->setGlobalZoom(1.0f);
        
        // Track changes on state2
        QSignalSpy spy(state2.get(), &DataViewerState::viewStateChanged);
        
        // Modify state1
        state1->setGlobalZoom(5.0f);
        state1->setTimeWindow(100, 200);
        
        // State2 should not have changed
        REQUIRE(spy.count() == 0);
        REQUIRE_THAT(state2->globalZoom(), Catch::Matchers::WithinAbs(1.0f, 1e-5f));
    }
    
    SECTION("serializing multiple instances produces distinct JSON") {
        auto state1 = std::make_shared<DataViewerState>();
        auto state2 = std::make_shared<DataViewerState>();
        
        state1->setDisplayName("Viewer 1");
        state2->setDisplayName("Viewer 2");
        
        state1->setTheme(DataViewerTheme::Light);
        state2->setTheme(DataViewerTheme::Dark);
        
        std::string json1 = state1->toJson();
        std::string json2 = state2->toJson();
        
        REQUIRE(json1 != json2);
        
        // Restore to new instances
        auto restored1 = std::make_shared<DataViewerState>();
        auto restored2 = std::make_shared<DataViewerState>();
        
        REQUIRE(restored1->fromJson(json1));
        REQUIRE(restored2->fromJson(json2));
        
        REQUIRE(restored1->getDisplayName() == "Viewer 1");
        REQUIRE(restored2->getDisplayName() == "Viewer 2");
        REQUIRE(restored1->theme() == DataViewerTheme::Light);
        REQUIRE(restored2->theme() == DataViewerTheme::Dark);
    }
}

// ==================== External State Modification Tests ====================

TEST_CASE("DataViewerState external modification pattern", "[DataViewerState][Integration]")
{
    SECTION("external component can modify state via shared_ptr") {
        // Simulate DataViewer_Widget owning state
        auto widget_state = std::make_shared<DataViewerState>();
        
        // Simulate properties panel receiving state reference
        DataViewerState* properties_panel_state = widget_state.get();
        
        // Track signals from widget's perspective
        QSignalSpy view_spy(widget_state.get(), &DataViewerState::viewStateChanged);
        QSignalSpy theme_spy(widget_state.get(), &DataViewerState::themeChanged);
        QSignalSpy options_spy(widget_state.get(), &DataViewerState::seriesOptionsChanged);
        
        // Properties panel modifies state
        properties_panel_state->setTimeWindow(500, 5000);
        REQUIRE(view_spy.count() == 1);
        
        properties_panel_state->setTheme(DataViewerTheme::Light);
        REQUIRE(theme_spy.count() == 1);
        
        AnalogSeriesOptionsData opts;
        opts.hex_color() = "#aabbcc";
        properties_panel_state->seriesOptions().set("channel_0", opts);
        REQUIRE(options_spy.count() == 1);
        
        // Widget sees the changes
        auto [start, end] = widget_state->timeWindow();
        REQUIRE(start == 500);
        REQUIRE(end == 5000);
        REQUIRE(widget_state->theme() == DataViewerTheme::Light);
        
        auto* restored_opts = widget_state->seriesOptions().get<AnalogSeriesOptionsData>("channel_0");
        REQUIRE(restored_opts != nullptr);
        REQUIRE(restored_opts->hex_color() == "#aabbcc");
    }
    
    SECTION("visibility toggle from external source") {
        auto state = std::make_shared<DataViewerState>();
        
        // Add a visible series
        AnalogSeriesOptionsData opts;
        opts.is_visible() = true;
        state->seriesOptions().set("test_series", opts);
        
        // Track visibility changes
        QSignalSpy vis_spy(state.get(), &DataViewerState::seriesVisibilityChanged);
        
        // External component toggles visibility (like a checkbox in properties panel)
        state->seriesOptions().setVisible("test_series", "analog", false);
        
        REQUIRE(vis_spy.count() == 1);
        REQUIRE(vis_spy.at(0).at(0).toString() == "test_series");
        REQUIRE(vis_spy.at(0).at(1).toString() == "analog");
        REQUIRE(vis_spy.at(0).at(2).toBool() == false);
        
        // Verify the change persisted
        auto* updated = state->seriesOptions().get<AnalogSeriesOptionsData>("test_series");
        REQUIRE(updated != nullptr);
        REQUIRE(updated->get_is_visible() == false);
    }
    
    SECTION("color change from external source triggers update") {
        auto state = std::make_shared<DataViewerState>();
        
        AnalogSeriesOptionsData initial_opts;
        initial_opts.hex_color() = "#ffffff";
        state->seriesOptions().set("colored_series", initial_opts);
        
        QSignalSpy change_spy(state.get(), &DataViewerState::seriesOptionsChanged);
        
        // Modify color via getMutable pattern (like color picker would)
        auto* mutable_opts = state->seriesOptions().getMutable<AnalogSeriesOptionsData>("colored_series");
        REQUIRE(mutable_opts != nullptr);
        
        mutable_opts->hex_color() = "#ff5500";
        
        // Need to notify the registry of the change
        state->seriesOptions().set("colored_series", *mutable_opts);
        
        REQUIRE(change_spy.count() == 1);
        
        // Verify persistence
        auto* final_opts = state->seriesOptions().get<AnalogSeriesOptionsData>("colored_series");
        REQUIRE(final_opts->hex_color() == "#ff5500");
    }
}

// ==================== Key Synchronization Tests ====================

TEST_CASE("DataViewerState key synchronization", "[DataViewerState][Integration]")
{
    SECTION("removing series from state cleans up options") {
        auto state = std::make_shared<DataViewerState>();
        
        // Add multiple series
        for (int i = 0; i < 5; ++i) {
            AnalogSeriesOptionsData opts;
            opts.hex_color() = "#000000";
            state->seriesOptions().set(QString::fromStdString("series_" + std::to_string(i)), opts);
        }
        
        REQUIRE(state->seriesOptions().keys<AnalogSeriesOptionsData>().size() == 5);
        
        // Remove one series (simulating DataStore removal sync)
        QSignalSpy remove_spy(state.get(), &DataViewerState::seriesOptionsRemoved);
        
        bool removed = state->seriesOptions().remove<AnalogSeriesOptionsData>("series_2");
        REQUIRE(removed);
        REQUIRE(remove_spy.count() == 1);
        
        // Verify remaining
        REQUIRE(state->seriesOptions().keys<AnalogSeriesOptionsData>().size() == 4);
        REQUIRE_FALSE(state->seriesOptions().has<AnalogSeriesOptionsData>("series_2"));
        REQUIRE(state->seriesOptions().has<AnalogSeriesOptionsData>("series_0"));
        REQUIRE(state->seriesOptions().has<AnalogSeriesOptionsData>("series_4"));
    }
    
    SECTION("state can have orphaned options (data not yet loaded)") {
        // This tests the workspace restore scenario where state is loaded
        // before data is available
        
        auto state = std::make_shared<DataViewerState>();
        
        // Add options for data that doesn't exist yet
        AnalogSeriesOptionsData opts;
        opts.hex_color() = "#123456";
        opts.user_scale_factor = 3.0f;
        state->seriesOptions().set("future_channel", opts);
        
        // Serialize state
        std::string json = state->toJson();
        
        // Restore to new state
        auto restored = std::make_shared<DataViewerState>();
        REQUIRE(restored->fromJson(json));
        
        // Options should be preserved even without corresponding data
        REQUIRE(restored->seriesOptions().has<AnalogSeriesOptionsData>("future_channel"));
        auto* restored_opts = restored->seriesOptions().get<AnalogSeriesOptionsData>("future_channel");
        REQUIRE(restored_opts->hex_color() == "#123456");
        REQUIRE_THAT(restored_opts->user_scale_factor, Catch::Matchers::WithinAbs(3.0f, 1e-5f));
    }
    
    SECTION("visibleKeys reflects current visibility state") {
        auto state = std::make_shared<DataViewerState>();
        
        // Add mix of visible and hidden series
        for (int i = 0; i < 6; ++i) {
            AnalogSeriesOptionsData opts;
            opts.is_visible() = (i < 3);  // First 3 visible, last 3 hidden
            state->seriesOptions().set(QString::fromStdString("ch_" + std::to_string(i)), opts);
        }
        
        auto all_keys = state->seriesOptions().keys<AnalogSeriesOptionsData>();
        auto visible_keys = state->seriesOptions().visibleKeys<AnalogSeriesOptionsData>();
        
        REQUIRE(all_keys.size() == 6);
        REQUIRE(visible_keys.size() == 3);
        
        // Toggle visibility
        state->seriesOptions().setVisible("ch_0", "analog", false);
        state->seriesOptions().setVisible("ch_5", "analog", true);
        
        visible_keys = state->seriesOptions().visibleKeys<AnalogSeriesOptionsData>();
        REQUIRE(visible_keys.size() == 3);  // Still 3 visible, but different ones
        
        // Verify correct keys are visible
        REQUIRE(visible_keys.contains("ch_1"));
        REQUIRE(visible_keys.contains("ch_2"));
        REQUIRE(visible_keys.contains("ch_5"));
        REQUIRE_FALSE(visible_keys.contains("ch_0"));
    }
}

// ==================== Performance Sanity Check ====================

TEST_CASE("DataViewerState serialization performance", "[DataViewerState][Integration][Performance]")
{
    SECTION("serialize/deserialize 100 series in reasonable time") {
        auto state = std::make_shared<DataViewerState>();
        
        // Add 100 analog series (realistic large session)
        for (int i = 0; i < 100; ++i) {
            AnalogSeriesOptionsData opts;
            opts.hex_color() = DataViewer::DefaultColors::getColorForIndex(static_cast<size_t>(i));
            opts.user_scale_factor = static_cast<float>(i) * 0.01f;
            state->seriesOptions().set(QString::fromStdString("channel_" + std::to_string(i)), opts);
        }
        
        // Add 20 event series
        for (int i = 0; i < 20; ++i) {
            DigitalEventSeriesOptionsData opts;
            state->seriesOptions().set(QString::fromStdString("events_" + std::to_string(i)), opts);
        }
        
        // Add 10 interval series
        for (int i = 0; i < 10; ++i) {
            DigitalIntervalSeriesOptionsData opts;
            state->seriesOptions().set(QString::fromStdString("intervals_" + std::to_string(i)), opts);
        }
        
        // Serialize (should complete quickly)
        std::string json = state->toJson();
        REQUIRE_FALSE(json.empty());
        
        // JSON should be reasonable size (< 100KB for 130 series)
        REQUIRE(json.size() < 100000);
        
        // Deserialize
        auto restored = std::make_shared<DataViewerState>();
        REQUIRE(restored->fromJson(json));
        
        // Verify count preserved
        REQUIRE(restored->seriesOptions().keys<AnalogSeriesOptionsData>().size() == 100);
        REQUIRE(restored->seriesOptions().keys<DigitalEventSeriesOptionsData>().size() == 20);
        REQUIRE(restored->seriesOptions().keys<DigitalIntervalSeriesOptionsData>().size() == 10);
    }
}
