/**
 * @file SeriesOptionsRegistry.test.cpp
 * @brief Unit tests for SeriesOptionsRegistry
 * 
 * Tests the generic type-safe registry for series display options,
 * including set/get/remove operations, signal emission, and visibility handling.
 */

#include "DataViewer_Widget/SeriesOptionsRegistry.hpp"
#include "DataViewer_Widget/DataViewerStateData.hpp"

#include <catch2/catch_test_macros.hpp>
#include <QSignalSpy>

// ==================== Type Name Tests ====================

TEST_CASE("SeriesOptionsRegistry typeName returns correct strings", "[SeriesOptionsRegistry]")
{
    SECTION("Analog type name") {
        REQUIRE(SeriesOptionsRegistry::typeName<AnalogSeriesOptionsData>() == "analog");
    }
    
    SECTION("Event type name") {
        REQUIRE(SeriesOptionsRegistry::typeName<DigitalEventSeriesOptionsData>() == "event");
    }
    
    SECTION("Interval type name") {
        REQUIRE(SeriesOptionsRegistry::typeName<DigitalIntervalSeriesOptionsData>() == "interval");
    }
}

// ==================== Analog Series Options Tests ====================

TEST_CASE("SeriesOptionsRegistry analog series operations", "[SeriesOptionsRegistry]")
{
    DataViewerStateData data;
    SeriesOptionsRegistry registry(&data);
    
    SECTION("set and get analog options") {
        AnalogSeriesOptionsData opts;
        opts.hex_color() = "#ff0000";
        opts.user_scale_factor = 2.5f;
        opts.is_visible() = true;
        
        registry.set("channel_1", opts);
        
        auto const* retrieved = registry.get<AnalogSeriesOptionsData>("channel_1");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->hex_color() == "#ff0000");
        REQUIRE(retrieved->user_scale_factor == 2.5f);
        REQUIRE(retrieved->get_is_visible() == true);
    }
    
    SECTION("get returns nullptr for non-existent key") {
        auto const* retrieved = registry.get<AnalogSeriesOptionsData>("nonexistent");
        REQUIRE(retrieved == nullptr);
    }
    
    SECTION("getMutable allows modification") {
        AnalogSeriesOptionsData opts;
        opts.hex_color() = "#0000ff";
        registry.set("channel_1", opts);
        
        auto* mutable_opts = registry.getMutable<AnalogSeriesOptionsData>("channel_1");
        REQUIRE(mutable_opts != nullptr);
        mutable_opts->hex_color() = "#00ff00";
        
        auto const* retrieved = registry.get<AnalogSeriesOptionsData>("channel_1");
        REQUIRE(retrieved->hex_color() == "#00ff00");
    }
    
    SECTION("has returns correct value") {
        REQUIRE_FALSE(registry.has<AnalogSeriesOptionsData>("channel_1"));
        
        AnalogSeriesOptionsData opts;
        registry.set("channel_1", opts);
        
        REQUIRE(registry.has<AnalogSeriesOptionsData>("channel_1"));
    }
    
    SECTION("remove removes options") {
        AnalogSeriesOptionsData opts;
        registry.set("channel_1", opts);
        
        REQUIRE(registry.remove<AnalogSeriesOptionsData>("channel_1"));
        REQUIRE_FALSE(registry.has<AnalogSeriesOptionsData>("channel_1"));
    }
    
    SECTION("remove returns false for non-existent key") {
        REQUIRE_FALSE(registry.remove<AnalogSeriesOptionsData>("nonexistent"));
    }
    
    SECTION("count returns correct value") {
        REQUIRE(registry.count<AnalogSeriesOptionsData>() == 0);
        
        AnalogSeriesOptionsData opts;
        registry.set("ch1", opts);
        REQUIRE(registry.count<AnalogSeriesOptionsData>() == 1);
        
        registry.set("ch2", opts);
        REQUIRE(registry.count<AnalogSeriesOptionsData>() == 2);
    }
    
    SECTION("keys returns all keys") {
        AnalogSeriesOptionsData opts;
        registry.set("alpha", opts);
        registry.set("beta", opts);
        registry.set("gamma", opts);
        
        QStringList keys = registry.keys<AnalogSeriesOptionsData>();
        REQUIRE(keys.size() == 3);
        REQUIRE(keys.contains("alpha"));
        REQUIRE(keys.contains("beta"));
        REQUIRE(keys.contains("gamma"));
    }
    
    SECTION("visibleKeys returns only visible keys") {
        AnalogSeriesOptionsData visible_opts;
        visible_opts.is_visible() = true;
        
        AnalogSeriesOptionsData hidden_opts;
        hidden_opts.is_visible() = false;
        
        registry.set("visible1", visible_opts);
        registry.set("hidden1", hidden_opts);
        registry.set("visible2", visible_opts);
        
        QStringList visible_keys = registry.visibleKeys<AnalogSeriesOptionsData>();
        REQUIRE(visible_keys.size() == 2);
        REQUIRE(visible_keys.contains("visible1"));
        REQUIRE(visible_keys.contains("visible2"));
        REQUIRE_FALSE(visible_keys.contains("hidden1"));
    }
}

// ==================== Digital Event Series Options Tests ====================

TEST_CASE("SeriesOptionsRegistry digital event series operations", "[SeriesOptionsRegistry]")
{
    DataViewerStateData data;
    SeriesOptionsRegistry registry(&data);
    
    SECTION("set and get event options") {
        DigitalEventSeriesOptionsData opts;
        opts.hex_color() = "#00ff00";
        opts.plotting_mode = EventPlottingModeData::Stacked;
        opts.event_height = 0.1f;
        
        registry.set("events_1", opts);
        
        auto const* retrieved = registry.get<DigitalEventSeriesOptionsData>("events_1");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->hex_color() == "#00ff00");
        REQUIRE(retrieved->plotting_mode == EventPlottingModeData::Stacked);
        REQUIRE(retrieved->event_height == 0.1f);
    }
    
    SECTION("has and remove work correctly") {
        DigitalEventSeriesOptionsData opts;
        registry.set("events_1", opts);
        
        REQUIRE(registry.has<DigitalEventSeriesOptionsData>("events_1"));
        REQUIRE(registry.remove<DigitalEventSeriesOptionsData>("events_1"));
        REQUIRE_FALSE(registry.has<DigitalEventSeriesOptionsData>("events_1"));
    }
    
    SECTION("count and keys work correctly") {
        DigitalEventSeriesOptionsData opts;
        registry.set("evt1", opts);
        registry.set("evt2", opts);
        
        REQUIRE(registry.count<DigitalEventSeriesOptionsData>() == 2);
        
        QStringList keys = registry.keys<DigitalEventSeriesOptionsData>();
        REQUIRE(keys.size() == 2);
    }
}

// ==================== Digital Interval Series Options Tests ====================

TEST_CASE("SeriesOptionsRegistry digital interval series operations", "[SeriesOptionsRegistry]")
{
    DataViewerStateData data;
    SeriesOptionsRegistry registry(&data);
    
    SECTION("set and get interval options") {
        DigitalIntervalSeriesOptionsData opts;
        opts.hex_color() = "#ffff00";
        opts.extend_full_canvas = false;
        opts.interval_height = 0.5f;
        
        registry.set("interval_1", opts);
        
        auto const* retrieved = registry.get<DigitalIntervalSeriesOptionsData>("interval_1");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->hex_color() == "#ffff00");
        REQUIRE(retrieved->extend_full_canvas == false);
        REQUIRE(retrieved->interval_height == 0.5f);
    }
    
    SECTION("visibleKeys filters correctly") {
        DigitalIntervalSeriesOptionsData visible_opts;
        visible_opts.is_visible() = true;
        
        DigitalIntervalSeriesOptionsData hidden_opts;
        hidden_opts.is_visible() = false;
        
        registry.set("visible_int", visible_opts);
        registry.set("hidden_int", hidden_opts);
        
        QStringList visible_keys = registry.visibleKeys<DigitalIntervalSeriesOptionsData>();
        REQUIRE(visible_keys.size() == 1);
        REQUIRE(visible_keys.contains("visible_int"));
    }
}

// ==================== Signal Emission Tests ====================

TEST_CASE("SeriesOptionsRegistry emits signals correctly", "[SeriesOptionsRegistry][signals]")
{
    DataViewerStateData data;
    SeriesOptionsRegistry registry(&data);
    
    SECTION("optionsChanged emitted on set") {
        QSignalSpy spy(&registry, &SeriesOptionsRegistry::optionsChanged);
        
        AnalogSeriesOptionsData opts;
        registry.set("channel_1", opts);
        
        REQUIRE(spy.count() == 1);
        QList<QVariant> args = spy.takeFirst();
        REQUIRE(args.at(0).toString() == "channel_1");
        REQUIRE(args.at(1).toString() == "analog");
    }
    
    SECTION("optionsChanged emitted for different types") {
        QSignalSpy spy(&registry, &SeriesOptionsRegistry::optionsChanged);
        
        AnalogSeriesOptionsData analog_opts;
        DigitalEventSeriesOptionsData event_opts;
        DigitalIntervalSeriesOptionsData interval_opts;
        
        registry.set("analog_1", analog_opts);
        registry.set("event_1", event_opts);
        registry.set("interval_1", interval_opts);
        
        REQUIRE(spy.count() == 3);
        
        // Verify each signal has correct type name
        REQUIRE(spy.at(0).at(1).toString() == "analog");
        REQUIRE(spy.at(1).at(1).toString() == "event");
        REQUIRE(spy.at(2).at(1).toString() == "interval");
    }
    
    SECTION("optionsRemoved emitted on remove") {
        QSignalSpy spy(&registry, &SeriesOptionsRegistry::optionsRemoved);
        
        AnalogSeriesOptionsData opts;
        registry.set("channel_1", opts);
        
        registry.remove<AnalogSeriesOptionsData>("channel_1");
        
        REQUIRE(spy.count() == 1);
        QList<QVariant> args = spy.takeFirst();
        REQUIRE(args.at(0).toString() == "channel_1");
        REQUIRE(args.at(1).toString() == "analog");
    }
    
    SECTION("optionsRemoved not emitted for non-existent key") {
        QSignalSpy spy(&registry, &SeriesOptionsRegistry::optionsRemoved);
        
        registry.remove<AnalogSeriesOptionsData>("nonexistent");
        
        REQUIRE(spy.count() == 0);
    }
    
    SECTION("notifyChanged emits optionsChanged") {
        AnalogSeriesOptionsData opts;
        registry.set("channel_1", opts);
        
        QSignalSpy spy(&registry, &SeriesOptionsRegistry::optionsChanged);
        
        registry.notifyChanged<AnalogSeriesOptionsData>("channel_1");
        
        REQUIRE(spy.count() == 1);
        QList<QVariant> args = spy.takeFirst();
        REQUIRE(args.at(0).toString() == "channel_1");
        REQUIRE(args.at(1).toString() == "analog");
    }
    
    SECTION("visibilityChanged emitted when visibility changes") {
        AnalogSeriesOptionsData opts;
        opts.is_visible() = true;
        registry.set("channel_1", opts);
        
        QSignalSpy spy(&registry, &SeriesOptionsRegistry::visibilityChanged);
        
        registry.setVisible("channel_1", "analog", false);
        
        REQUIRE(spy.count() == 1);
        QList<QVariant> args = spy.takeFirst();
        REQUIRE(args.at(0).toString() == "channel_1");
        REQUIRE(args.at(1).toString() == "analog");
        REQUIRE(args.at(2).toBool() == false);
    }
    
    SECTION("visibilityChanged not emitted when visibility unchanged") {
        AnalogSeriesOptionsData opts;
        opts.is_visible() = true;
        registry.set("channel_1", opts);
        
        QSignalSpy spy(&registry, &SeriesOptionsRegistry::visibilityChanged);
        
        registry.setVisible("channel_1", "analog", true);  // Same as current
        
        REQUIRE(spy.count() == 0);
    }
}

// ==================== Non-Template Visibility Methods ====================

TEST_CASE("SeriesOptionsRegistry non-template visibility methods", "[SeriesOptionsRegistry]")
{
    DataViewerStateData data;
    SeriesOptionsRegistry registry(&data);
    
    SECTION("setVisible works for analog") {
        AnalogSeriesOptionsData opts;
        opts.is_visible() = true;
        registry.set("ch1", opts);
        
        REQUIRE(registry.setVisible("ch1", "analog", false));
        REQUIRE_FALSE(registry.get<AnalogSeriesOptionsData>("ch1")->get_is_visible());
    }
    
    SECTION("setVisible works for event") {
        DigitalEventSeriesOptionsData opts;
        opts.is_visible() = false;
        registry.set("evt1", opts);
        
        REQUIRE(registry.setVisible("evt1", "event", true));
        REQUIRE(registry.get<DigitalEventSeriesOptionsData>("evt1")->get_is_visible());
    }
    
    SECTION("setVisible works for interval") {
        DigitalIntervalSeriesOptionsData opts;
        opts.is_visible() = true;
        registry.set("int1", opts);
        
        REQUIRE(registry.setVisible("int1", "interval", false));
        REQUIRE_FALSE(registry.get<DigitalIntervalSeriesOptionsData>("int1")->get_is_visible());
    }
    
    SECTION("setVisible returns false for non-existent key") {
        REQUIRE_FALSE(registry.setVisible("nonexistent", "analog", true));
    }
    
    SECTION("setVisible returns false for invalid type name") {
        AnalogSeriesOptionsData opts;
        registry.set("ch1", opts);
        
        REQUIRE_FALSE(registry.setVisible("ch1", "invalid_type", false));
    }
    
    SECTION("isVisible returns correct value for analog") {
        AnalogSeriesOptionsData opts;
        opts.is_visible() = true;
        registry.set("ch1", opts);
        
        REQUIRE(registry.isVisible("ch1", "analog"));
        
        registry.setVisible("ch1", "analog", false);
        REQUIRE_FALSE(registry.isVisible("ch1", "analog"));
    }
    
    SECTION("isVisible returns false for non-existent key") {
        REQUIRE_FALSE(registry.isVisible("nonexistent", "analog"));
    }
    
    SECTION("isVisible returns false for invalid type name") {
        AnalogSeriesOptionsData opts;
        opts.is_visible() = true;
        registry.set("ch1", opts);
        
        REQUIRE_FALSE(registry.isVisible("ch1", "invalid_type"));
    }
}

// ==================== Cross-Type Isolation Tests ====================

TEST_CASE("SeriesOptionsRegistry types are isolated", "[SeriesOptionsRegistry]")
{
    DataViewerStateData data;
    SeriesOptionsRegistry registry(&data);
    
    SECTION("same key can exist for different types") {
        AnalogSeriesOptionsData analog_opts;
        analog_opts.hex_color() = "#ff0000";
        
        DigitalEventSeriesOptionsData event_opts;
        event_opts.hex_color() = "#00ff00";
        
        DigitalIntervalSeriesOptionsData interval_opts;
        interval_opts.hex_color() = "#0000ff";
        
        registry.set("data_1", analog_opts);
        registry.set("data_1", event_opts);
        registry.set("data_1", interval_opts);
        
        // All three should exist
        REQUIRE(registry.has<AnalogSeriesOptionsData>("data_1"));
        REQUIRE(registry.has<DigitalEventSeriesOptionsData>("data_1"));
        REQUIRE(registry.has<DigitalIntervalSeriesOptionsData>("data_1"));
        
        // Colors should be independent
        REQUIRE(registry.get<AnalogSeriesOptionsData>("data_1")->hex_color() == "#ff0000");
        REQUIRE(registry.get<DigitalEventSeriesOptionsData>("data_1")->hex_color() == "#00ff00");
        REQUIRE(registry.get<DigitalIntervalSeriesOptionsData>("data_1")->hex_color() == "#0000ff");
    }
    
    SECTION("removing one type doesn't affect others") {
        AnalogSeriesOptionsData analog_opts;
        DigitalEventSeriesOptionsData event_opts;
        
        registry.set("data_1", analog_opts);
        registry.set("data_1", event_opts);
        
        registry.remove<AnalogSeriesOptionsData>("data_1");
        
        REQUIRE_FALSE(registry.has<AnalogSeriesOptionsData>("data_1"));
        REQUIRE(registry.has<DigitalEventSeriesOptionsData>("data_1"));
    }
    
    SECTION("count is per-type") {
        AnalogSeriesOptionsData analog_opts;
        DigitalEventSeriesOptionsData event_opts;
        
        registry.set("a1", analog_opts);
        registry.set("a2", analog_opts);
        registry.set("e1", event_opts);
        
        REQUIRE(registry.count<AnalogSeriesOptionsData>() == 2);
        REQUIRE(registry.count<DigitalEventSeriesOptionsData>() == 1);
        REQUIRE(registry.count<DigitalIntervalSeriesOptionsData>() == 0);
    }
}

// ==================== Edge Cases ====================

TEST_CASE("SeriesOptionsRegistry edge cases", "[SeriesOptionsRegistry]")
{
    DataViewerStateData data;
    SeriesOptionsRegistry registry(&data);
    
    SECTION("empty key works") {
        AnalogSeriesOptionsData opts;
        registry.set("", opts);
        
        REQUIRE(registry.has<AnalogSeriesOptionsData>(""));
        REQUIRE(registry.get<AnalogSeriesOptionsData>("") != nullptr);
    }
    
    SECTION("special characters in key") {
        AnalogSeriesOptionsData opts;
        registry.set("key/with\\special:chars", opts);
        
        REQUIRE(registry.has<AnalogSeriesOptionsData>("key/with\\special:chars"));
    }
    
    SECTION("overwriting existing options") {
        AnalogSeriesOptionsData opts1;
        opts1.hex_color() = "#111111";
        registry.set("ch1", opts1);
        
        AnalogSeriesOptionsData opts2;
        opts2.hex_color() = "#222222";
        registry.set("ch1", opts2);
        
        REQUIRE(registry.count<AnalogSeriesOptionsData>() == 1);
        REQUIRE(registry.get<AnalogSeriesOptionsData>("ch1")->hex_color() == "#222222");
    }
    
    SECTION("modifying via getMutable persists") {
        AnalogSeriesOptionsData opts;
        opts.user_scale_factor = 1.0f;
        registry.set("ch1", opts);
        
        auto* mutable_opts = registry.getMutable<AnalogSeriesOptionsData>("ch1");
        mutable_opts->user_scale_factor = 5.0f;
        
        // Get through const getter
        auto const* const_opts = registry.get<AnalogSeriesOptionsData>("ch1");
        REQUIRE(const_opts->user_scale_factor == 5.0f);
    }
}

// ==================== Data Synchronization Tests ====================

TEST_CASE("SeriesOptionsRegistry data synchronization", "[SeriesOptionsRegistry]")
{
    DataViewerStateData data;
    SeriesOptionsRegistry registry(&data);
    
    SECTION("registry modifies underlying data directly") {
        AnalogSeriesOptionsData opts;
        opts.hex_color() = "#aabbcc";
        registry.set("ch1", opts);
        
        // Check the underlying data structure directly
        REQUIRE(data.analog_options.contains("ch1"));
        REQUIRE(data.analog_options.at("ch1").hex_color() == "#aabbcc");
    }
    
    SECTION("changes to underlying data visible through registry") {
        // Modify data directly
        AnalogSeriesOptionsData opts;
        opts.hex_color() = "#ddeeff";
        data.analog_options["direct_add"] = opts;
        
        // Should be visible through registry
        REQUIRE(registry.has<AnalogSeriesOptionsData>("direct_add"));
        REQUIRE(registry.get<AnalogSeriesOptionsData>("direct_add")->hex_color() == "#ddeeff");
    }
}
