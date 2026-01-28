/**
 * @file DataImportWidgetState.test.cpp
 * @brief Unit tests for DataImportWidgetState
 * 
 * Tests the EditorState subclass for DataImport_Widget, including:
 * - Typed accessors for all state properties
 * - Signal emission verification
 * - JSON serialization/deserialization round-trip
 */

#include "DataImport_Widget/DataImportWidgetState.hpp"

#include <catch2/catch_test_macros.hpp>
#include <QSignalSpy>

// ==================== Construction Tests ====================

TEST_CASE("DataImportWidgetState construction", "[DataImportWidgetState]")
{
    SECTION("default construction creates valid state") {
        DataImportWidgetState state;
        
        REQUIRE(state.getTypeName() == "DataImportWidget");
        REQUIRE(state.getDisplayName() == "Data Import");
        REQUIRE_FALSE(state.getInstanceId().isEmpty());
        REQUIRE_FALSE(state.isDirty());
    }
    
    SECTION("instance IDs are unique") {
        DataImportWidgetState state1;
        DataImportWidgetState state2;
        
        REQUIRE(state1.getInstanceId() != state2.getInstanceId());
    }
    
    SECTION("default values are empty") {
        DataImportWidgetState state;
        
        REQUIRE(state.selectedImportType().isEmpty());
        REQUIRE(state.lastUsedDirectory().isEmpty());
        REQUIRE(state.formatPreference("LineData").isEmpty());
    }
}

// ==================== Display Name Tests ====================

TEST_CASE("DataImportWidgetState display name", "[DataImportWidgetState]")
{
    DataImportWidgetState state;
    
    SECTION("setDisplayName changes name") {
        state.setDisplayName("My Custom Import");
        REQUIRE(state.getDisplayName() == "My Custom Import");
    }
    
    SECTION("setDisplayName emits signal") {
        QSignalSpy spy(&state, &DataImportWidgetState::displayNameChanged);
        
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
        QSignalSpy spy(&state, &DataImportWidgetState::displayNameChanged);
        
        state.setDisplayName("Test");
        
        REQUIRE(spy.count() == 0);
    }
}

// ==================== Selected Import Type Tests ====================

TEST_CASE("DataImportWidgetState selected import type", "[DataImportWidgetState]")
{
    DataImportWidgetState state;
    
    SECTION("setSelectedImportType changes value") {
        state.setSelectedImportType("LineData");
        REQUIRE(state.selectedImportType() == "LineData");
    }
    
    SECTION("setSelectedImportType emits signal") {
        QSignalSpy spy(&state, &DataImportWidgetState::selectedImportTypeChanged);
        
        state.setSelectedImportType("MaskData");
        
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toString() == "MaskData");
    }
    
    SECTION("setSelectedImportType marks dirty") {
        state.markClean();
        REQUIRE_FALSE(state.isDirty());
        
        state.setSelectedImportType("PointData");
        REQUIRE(state.isDirty());
    }
    
    SECTION("setting same type does not emit signal") {
        state.setSelectedImportType("LineData");
        QSignalSpy spy(&state, &DataImportWidgetState::selectedImportTypeChanged);
        
        state.setSelectedImportType("LineData");
        
        REQUIRE(spy.count() == 0);
    }
    
    SECTION("supports all expected data types") {
        std::vector<QString> types = {
            "LineData", "MaskData", "PointData", 
            "AnalogTimeSeries", "DigitalEventSeries", 
            "DigitalIntervalSeries", "TensorData"
        };
        
        for (auto const& type : types) {
            state.setSelectedImportType(type);
            REQUIRE(state.selectedImportType() == type);
        }
    }
}

// ==================== Last Used Directory Tests ====================

TEST_CASE("DataImportWidgetState last used directory", "[DataImportWidgetState]")
{
    DataImportWidgetState state;
    
    SECTION("setLastUsedDirectory changes value") {
        state.setLastUsedDirectory("/home/user/data");
        REQUIRE(state.lastUsedDirectory() == "/home/user/data");
    }
    
    SECTION("setLastUsedDirectory emits signal") {
        QSignalSpy spy(&state, &DataImportWidgetState::lastUsedDirectoryChanged);
        
        state.setLastUsedDirectory("/tmp");
        
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toString() == "/tmp");
    }
    
    SECTION("setLastUsedDirectory marks dirty") {
        state.markClean();
        REQUIRE_FALSE(state.isDirty());
        
        state.setLastUsedDirectory("/new/path");
        REQUIRE(state.isDirty());
    }
    
    SECTION("setting same directory does not emit signal") {
        state.setLastUsedDirectory("/path");
        QSignalSpy spy(&state, &DataImportWidgetState::lastUsedDirectoryChanged);
        
        state.setLastUsedDirectory("/path");
        
        REQUIRE(spy.count() == 0);
    }
}

// ==================== Format Preferences Tests ====================

TEST_CASE("DataImportWidgetState format preferences", "[DataImportWidgetState]")
{
    DataImportWidgetState state;
    
    SECTION("setFormatPreference sets value") {
        state.setFormatPreference("LineData", "CSV");
        REQUIRE(state.formatPreference("LineData") == "CSV");
    }
    
    SECTION("setFormatPreference emits signal") {
        QSignalSpy spy(&state, &DataImportWidgetState::formatPreferenceChanged);
        
        state.setFormatPreference("MaskData", "HDF5");
        
        REQUIRE(spy.count() == 1);
        auto args = spy.takeFirst();
        REQUIRE(args.at(0).toString() == "MaskData");
        REQUIRE(args.at(1).toString() == "HDF5");
    }
    
    SECTION("setFormatPreference marks dirty") {
        state.markClean();
        REQUIRE_FALSE(state.isDirty());
        
        state.setFormatPreference("PointData", "CSV");
        REQUIRE(state.isDirty());
    }
    
    SECTION("setting same preference does not emit signal") {
        state.setFormatPreference("LineData", "Binary");
        QSignalSpy spy(&state, &DataImportWidgetState::formatPreferenceChanged);
        
        state.setFormatPreference("LineData", "Binary");
        
        REQUIRE(spy.count() == 0);
    }
    
    SECTION("different data types have independent preferences") {
        state.setFormatPreference("LineData", "CSV");
        state.setFormatPreference("MaskData", "HDF5");
        state.setFormatPreference("AnalogTimeSeries", "Binary");
        
        REQUIRE(state.formatPreference("LineData") == "CSV");
        REQUIRE(state.formatPreference("MaskData") == "HDF5");
        REQUIRE(state.formatPreference("AnalogTimeSeries") == "Binary");
    }
    
    SECTION("unset preference returns empty string") {
        REQUIRE(state.formatPreference("UnknownType").isEmpty());
    }
}

// ==================== JSON Serialization Tests ====================

TEST_CASE("DataImportWidgetState JSON serialization", "[DataImportWidgetState]")
{
    SECTION("round-trip preserves all data") {
        DataImportWidgetState state;
        state.setDisplayName("My Import Widget");
        state.setSelectedImportType("LineData");
        state.setLastUsedDirectory("/home/user/recordings");
        state.setFormatPreference("LineData", "CSV");
        state.setFormatPreference("MaskData", "HDF5");
        
        auto json = state.toJson();
        
        DataImportWidgetState restored;
        REQUIRE(restored.fromJson(json));
        
        REQUIRE(restored.getDisplayName() == "My Import Widget");
        REQUIRE(restored.selectedImportType() == "LineData");
        REQUIRE(restored.lastUsedDirectory() == "/home/user/recordings");
        REQUIRE(restored.formatPreference("LineData") == "CSV");
        REQUIRE(restored.formatPreference("MaskData") == "HDF5");
    }
    
    SECTION("instance ID is preserved through serialization") {
        DataImportWidgetState state;
        auto original_id = state.getInstanceId();
        
        auto json = state.toJson();
        
        DataImportWidgetState restored;
        REQUIRE(restored.fromJson(json));
        
        REQUIRE(restored.getInstanceId() == original_id);
    }
    
    SECTION("fromJson handles invalid JSON gracefully") {
        DataImportWidgetState state;
        
        REQUIRE_FALSE(state.fromJson("not valid json"));
        REQUIRE_FALSE(state.fromJson("{}"));
        REQUIRE_FALSE(state.fromJson(""));
    }
    
    SECTION("toJson produces valid JSON") {
        DataImportWidgetState state;
        auto json = state.toJson();
        
        // Should be able to parse it back
        DataImportWidgetState restored;
        REQUIRE(restored.fromJson(json));
    }
    
    SECTION("default state serializes correctly") {
        DataImportWidgetState state;
        auto json = state.toJson();
        
        DataImportWidgetState restored;
        REQUIRE(restored.fromJson(json));
        
        // Default values should be preserved
        REQUIRE(restored.getDisplayName() == "Data Import");
        REQUIRE(restored.selectedImportType().isEmpty());
        REQUIRE(restored.lastUsedDirectory().isEmpty());
    }
}

// ==================== Clean/Dirty State Tests ====================

TEST_CASE("DataImportWidgetState clean/dirty tracking", "[DataImportWidgetState]")
{
    DataImportWidgetState state;
    
    SECTION("newly created state is clean") {
        REQUIRE_FALSE(state.isDirty());
    }
    
    SECTION("setters mark state as dirty") {
        // Verify dirtyChanged signal is emitted when state becomes dirty
        QSignalSpy spy(&state, &DataImportWidgetState::dirtyChanged);
        
        state.setSelectedImportType("LineData");
        REQUIRE(state.isDirty());
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toBool() == true);
    }
    
    SECTION("markClean clears dirty flag") {
        state.setSelectedImportType("LineData");
        REQUIRE(state.isDirty());
        
        state.markClean();
        REQUIRE_FALSE(state.isDirty());
    }
    
    SECTION("markClean emits dirtyChanged signal") {
        state.setSelectedImportType("LineData");
        QSignalSpy spy(&state, &DataImportWidgetState::dirtyChanged);
        
        state.markClean();
        
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toBool() == false);
    }
    
    SECTION("all setters mark state as dirty") {
        state.markClean();
        state.setSelectedImportType("LineData");
        REQUIRE(state.isDirty());
        
        state.markClean();
        state.setLastUsedDirectory("/tmp");
        REQUIRE(state.isDirty());
        
        state.markClean();
        state.setFormatPreference("LineData", "CSV");
        REQUIRE(state.isDirty());
        
        state.markClean();
        state.setDisplayName("New Name");
        REQUIRE(state.isDirty());
    }
}
