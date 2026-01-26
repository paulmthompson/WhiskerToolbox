#include "TableDesignerState.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <QCoreApplication>
#include <QSignalSpy>

// Helper to create QCoreApplication for Qt signal/slot testing
namespace {
struct TestApp {
    TestApp() {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char const * argv[] = {"test"};
            app = std::make_unique<QCoreApplication>(argc, const_cast<char **>(argv));
        }
    }
    std::unique_ptr<QCoreApplication> app;
};
}

TEST_CASE("TableDesignerState - Construction", "[TableDesignerState]") {
    TestApp test_app;
    
    TableDesignerState state;
    
    SECTION("Default values are set") {
        CHECK(state.getTypeName() == "TableDesigner");
        CHECK(state.getDisplayName() == "Table Designer");
        CHECK(state.currentTableId().isEmpty());
        CHECK(state.rowSourceName().isEmpty());
        CHECK(state.captureRange() == 30000);
        CHECK(state.intervalMode() == IntervalRowMode::Beginning);
        CHECK(state.groupModeEnabled() == true);
        CHECK(state.groupingPattern() == "(.+)_\\d+$");
    }
    
    SECTION("Instance ID is generated") {
        CHECK(!state.getInstanceId().isEmpty());
    }
    
    SECTION("Initial state is not dirty") {
        CHECK(!state.isDirty());
    }
}

TEST_CASE("TableDesignerState - Table Selection", "[TableDesignerState]") {
    TestApp test_app;
    
    TableDesignerState state;
    QSignalSpy spy(&state, &TableDesignerState::currentTableIdChanged);
    
    SECTION("Setting table ID emits signal") {
        state.setCurrentTableId("table_1");
        
        CHECK(state.currentTableId() == "table_1");
        REQUIRE(spy.count() == 1);
        CHECK(spy.at(0).at(0).toString() == "table_1");
        CHECK(state.isDirty());
    }
    
    SECTION("Setting same value does not emit signal") {
        state.setCurrentTableId("table_1");
        spy.clear();
        state.markClean();
        
        state.setCurrentTableId("table_1");
        
        CHECK(spy.count() == 0);
        CHECK(!state.isDirty());
    }
    
    SECTION("Clearing table ID") {
        state.setCurrentTableId("table_1");
        spy.clear();
        
        state.setCurrentTableId("");
        
        CHECK(state.currentTableId().isEmpty());
        CHECK(spy.count() == 1);
    }
}

TEST_CASE("TableDesignerState - Row Settings", "[TableDesignerState]") {
    TestApp test_app;
    
    TableDesignerState state;
    QSignalSpy spy(&state, &TableDesignerState::rowSettingsChanged);
    
    SECTION("Setting row source name") {
        state.setRowSourceName("Intervals: trial_intervals");
        
        CHECK(state.rowSourceName() == "Intervals: trial_intervals");
        CHECK(spy.count() == 1);
        CHECK(state.isDirty());
    }
    
    SECTION("Setting capture range") {
        state.setCaptureRange(15000);
        
        CHECK(state.captureRange() == 15000);
        CHECK(spy.count() == 1);
    }
    
    SECTION("Setting interval mode") {
        state.setIntervalMode(IntervalRowMode::End);
        CHECK(state.intervalMode() == IntervalRowMode::End);
        CHECK(spy.count() == 1);
        
        spy.clear();
        state.setIntervalMode(IntervalRowMode::Itself);
        CHECK(state.intervalMode() == IntervalRowMode::Itself);
        CHECK(spy.count() == 1);
    }
    
    SECTION("Setting complete row settings") {
        RowSourceSettings settings;
        settings.source_name = "Events: licks";
        settings.capture_range = 5000;
        settings.interval_mode = IntervalRowMode::Itself;
        
        state.setRowSettings(settings);
        
        CHECK(state.rowSourceName() == "Events: licks");
        CHECK(state.captureRange() == 5000);
        CHECK(state.intervalMode() == IntervalRowMode::Itself);
        CHECK(spy.count() == 1);
    }
}

TEST_CASE("TableDesignerState - Group Mode Settings", "[TableDesignerState]") {
    TestApp test_app;
    
    TableDesignerState state;
    QSignalSpy spy(&state, &TableDesignerState::groupSettingsChanged);
    
    SECTION("Disabling group mode") {
        state.setGroupModeEnabled(false);
        
        CHECK(!state.groupModeEnabled());
        CHECK(spy.count() == 1);
    }
    
    SECTION("Setting grouping pattern") {
        state.setGroupingPattern("^(.+)_trial_\\d+$");
        
        CHECK(state.groupingPattern() == "^(.+)_trial_\\d+$");
        CHECK(spy.count() == 1);
    }
    
    SECTION("Setting complete group settings") {
        GroupModeSettings settings;
        settings.enabled = false;
        settings.pattern = "custom_pattern";
        
        state.setGroupSettings(settings);
        
        CHECK(!state.groupModeEnabled());
        CHECK(state.groupingPattern() == "custom_pattern");
        CHECK(spy.count() == 1);
    }
}

TEST_CASE("TableDesignerState - Computer States", "[TableDesignerState]") {
    TestApp test_app;
    
    TableDesignerState state;
    QSignalSpy spy(&state, &TableDesignerState::computerStateChanged);
    
    QString const key = "analog:signal_1||Mean";
    
    SECTION("Enabling a computer") {
        state.setComputerEnabled(key, true);
        
        CHECK(state.isComputerEnabled(key));
        REQUIRE(spy.count() == 1);
        CHECK(spy.at(0).at(0).toString() == key);
    }
    
    SECTION("Setting custom column name") {
        state.setComputerColumnName(key, "Signal1_Mean");
        
        CHECK(state.computerColumnName(key) == "Signal1_Mean");
        CHECK(spy.count() == 1);
    }
    
    SECTION("Getting non-existent computer state") {
        CHECK(!state.isComputerEnabled(key));
        CHECK(state.computerColumnName(key).isEmpty());
        CHECK(state.getComputerState(key) == nullptr);
    }
    
    SECTION("Setting complete computer state") {
        ComputerStateEntry entry;
        entry.enabled = true;
        entry.column_name = "CustomName";
        
        state.setComputerState(key, entry);
        
        CHECK(state.isComputerEnabled(key));
        CHECK(state.computerColumnName(key) == "CustomName");
        
        auto const * retrieved = state.getComputerState(key);
        REQUIRE(retrieved != nullptr);
        CHECK(retrieved->enabled);
        CHECK(retrieved->column_name == "CustomName");
    }
    
    SECTION("Removing computer state") {
        state.setComputerEnabled(key, true);
        spy.clear();
        
        bool removed = state.removeComputerState(key);
        
        CHECK(removed);
        CHECK(!state.isComputerEnabled(key));
        CHECK(spy.count() == 1);
    }
    
    SECTION("Clearing all computer states") {
        state.setComputerEnabled(key, true);
        state.setComputerEnabled("analog:signal_2||Max", true);
        
        QSignalSpy clearSpy(&state, &TableDesignerState::computerStatesCleared);
        state.clearComputerStates();
        
        CHECK(state.computerStates().empty());
        CHECK(clearSpy.count() == 1);
    }
    
    SECTION("Getting enabled computer keys") {
        state.setComputerEnabled("analog:a||Mean", true);
        state.setComputerEnabled("analog:b||Max", true);
        state.setComputerEnabled("analog:c||Min", false);
        
        auto enabled = state.enabledComputerKeys();
        
        CHECK(enabled.size() == 2);
        CHECK(enabled.contains("analog:a||Mean"));
        CHECK(enabled.contains("analog:b||Max"));
        CHECK(!enabled.contains("analog:c||Min"));
    }
}

TEST_CASE("TableDesignerState - Column Order", "[TableDesignerState]") {
    TestApp test_app;
    
    TableDesignerState state;
    QSignalSpy spy(&state, &TableDesignerState::columnOrderChanged);
    
    QString const table_id = "table_1";
    
    SECTION("Setting column order") {
        QStringList order = {"col_a", "col_b", "col_c"};
        state.setColumnOrder(table_id, order);
        
        auto retrieved = state.columnOrder(table_id);
        CHECK(retrieved == order);
        REQUIRE(spy.count() == 1);
        CHECK(spy.at(0).at(0).toString() == table_id);
    }
    
    SECTION("Getting non-existent column order") {
        auto order = state.columnOrder("nonexistent");
        CHECK(order.isEmpty());
    }
    
    SECTION("Removing column order") {
        state.setColumnOrder(table_id, {"a", "b"});
        spy.clear();
        
        bool removed = state.removeColumnOrder(table_id);
        
        CHECK(removed);
        CHECK(state.columnOrder(table_id).isEmpty());
        CHECK(spy.count() == 1);
    }
    
    SECTION("Clearing all column orders") {
        state.setColumnOrder("table_1", {"a"});
        state.setColumnOrder("table_2", {"b"});
        spy.clear();
        
        state.clearColumnOrders();
        
        CHECK(state.columnOrder("table_1").isEmpty());
        CHECK(state.columnOrder("table_2").isEmpty());
        CHECK(spy.count() == 1);
        CHECK(spy.at(0).at(0).toString().isEmpty());  // Empty string indicates all cleared
    }
}

TEST_CASE("TableDesignerState - Serialization", "[TableDesignerState]") {
    TestApp test_app;
    
    SECTION("Round-trip serialization preserves state") {
        TableDesignerState original;
        original.setDisplayName("My Table Designer");
        original.setCurrentTableId("table_test");
        original.setRowSourceName("Intervals: my_intervals");
        original.setCaptureRange(20000);
        original.setIntervalMode(IntervalRowMode::End);
        original.setGroupModeEnabled(false);
        original.setGroupingPattern("(.*)_v\\d+");
        original.setComputerEnabled("analog:sig||Mean", true);
        original.setComputerColumnName("analog:sig||Mean", "SigMean");
        original.setColumnOrder("table_test", {"col1", "col2", "col3"});
        
        std::string json = original.toJson();
        
        TableDesignerState restored;
        bool success = restored.fromJson(json);
        
        REQUIRE(success);
        CHECK(restored.getDisplayName() == "My Table Designer");
        CHECK(restored.currentTableId() == "table_test");
        CHECK(restored.rowSourceName() == "Intervals: my_intervals");
        CHECK(restored.captureRange() == 20000);
        CHECK(restored.intervalMode() == IntervalRowMode::End);
        CHECK(!restored.groupModeEnabled());
        CHECK(restored.groupingPattern() == "(.*)_v\\d+");
        CHECK(restored.isComputerEnabled("analog:sig||Mean"));
        CHECK(restored.computerColumnName("analog:sig||Mean") == "SigMean");
        
        auto order = restored.columnOrder("table_test");
        REQUIRE(order.size() == 3);
        CHECK(order.at(0) == "col1");
        CHECK(order.at(1) == "col2");
        CHECK(order.at(2) == "col3");
    }
    
    SECTION("Instance ID is preserved") {
        TableDesignerState original;
        QString original_id = original.getInstanceId();
        
        std::string json = original.toJson();
        
        TableDesignerState restored;
        restored.fromJson(json);
        
        CHECK(restored.getInstanceId() == original_id);
    }
    
    SECTION("Invalid JSON returns false") {
        TableDesignerState state;
        bool success = state.fromJson("{ invalid json }");
        CHECK(!success);
    }
    
    SECTION("Empty state serializes correctly") {
        TableDesignerState state;
        std::string json = state.toJson();
        
        // Should contain the type name and defaults
        CHECK(json.find("Table Designer") != std::string::npos);
        CHECK(json.find("instance_id") != std::string::npos);
    }
}

TEST_CASE("TableDesignerState - Dirty State Tracking", "[TableDesignerState]") {
    TestApp test_app;
    
    TableDesignerState state;
    
    SECTION("State becomes dirty on changes") {
        CHECK(!state.isDirty());
        
        state.setCurrentTableId("test");
        CHECK(state.isDirty());
    }
    
    SECTION("markClean clears dirty state") {
        state.setCurrentTableId("test");
        state.markClean();
        
        CHECK(!state.isDirty());
    }
    
    SECTION("Multiple changes keep state dirty") {
        state.setCurrentTableId("test");
        state.setRowSourceName("Events: test");
        state.setCaptureRange(1000);
        
        CHECK(state.isDirty());
    }
}

TEST_CASE("TableDesignerState - Display Name", "[TableDesignerState]") {
    TestApp test_app;
    
    TableDesignerState state;
    QSignalSpy spy(&state, &TableDesignerState::displayNameChanged);
    
    SECTION("Setting display name emits signal") {
        state.setDisplayName("Custom Name");
        
        CHECK(state.getDisplayName() == "Custom Name");
        CHECK(spy.count() == 1);
        CHECK(spy.at(0).at(0).toString() == "Custom Name");
    }
    
    SECTION("Setting same name does not emit signal") {
        state.setDisplayName("Test");
        spy.clear();
        state.markClean();
        
        state.setDisplayName("Test");
        
        CHECK(spy.count() == 0);
        CHECK(!state.isDirty());
    }
}
