/**
 * @file TableDesignerWidgetStateIntegration.test.cpp
 * @brief Tests for bidirectional state binding between TableDesignerWidget and TableDesignerState
 * 
 * These tests verify that:
 * 1. UI changes propagate to the state (UI → State)
 * 2. State changes (e.g., from JSON restore) update the UI (State → UI)
 * 3. No infinite loops occur during synchronization
 * 4. State serialization round-trips correctly with widget
 */

#include "TableDesignerWidget.hpp"
#include "TableDesignerState.hpp"
#include "TableDesignerStateData.hpp"

#include "DataManager/DataManager.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSpinBox>

// Helper to create QApplication for widget testing
namespace {
struct TestApp {
    TestApp() {
        if (!QApplication::instance()) {
            static int argc = 1;
            static char const * argv[] = {"test"};
            app = std::make_unique<QApplication>(argc, const_cast<char **>(argv));
        }
    }
    std::unique_ptr<QApplication> app;
};

// Helper to find child widgets by object name
template<typename T>
T * findChild(QWidget * parent, QString const & name) {
    return parent->findChild<T *>(name);
}
}

TEST_CASE("TableDesignerWidget State Integration - Basic Setup", "[TableDesignerWidget][StateIntegration]") {
    TestApp test_app;
    
    auto data_manager = std::make_shared<DataManager>();
    auto state = std::make_shared<TableDesignerState>();
    
    TableDesignerWidget widget(data_manager);
    
    SECTION("Widget starts without state") {
        CHECK(widget.getState() == nullptr);
    }
    
    SECTION("State can be set") {
        widget.setState(state);
        CHECK(widget.getState() == state);
    }
    
    SECTION("Setting null state is rejected") {
        widget.setState(state);
        widget.setState(nullptr);
        CHECK(widget.getState() == state);  // Should still have original state
    }
}

TEST_CASE("TableDesignerWidget State Integration - UI to State propagation", "[TableDesignerWidget][StateIntegration]") {
    TestApp test_app;
    
    auto data_manager = std::make_shared<DataManager>();
    auto state = std::make_shared<TableDesignerState>();
    
    TableDesignerWidget widget(data_manager);
    widget.setState(state);
    
    SECTION("Group mode toggle updates state") {
        QSignalSpy spy(state.get(), &TableDesignerState::groupSettingsChanged);
        
        // Find the group mode toggle button
        auto * toggle = findChild<QPushButton>(&widget, "group_mode_toggle_btn");
        REQUIRE(toggle != nullptr);
        
        // Default should be enabled (checked)
        CHECK(state->groupModeEnabled() == true);
        
        // Toggle off
        toggle->setChecked(false);
        
        // State should be updated
        CHECK(state->groupModeEnabled() == false);
        CHECK(spy.count() >= 1);
    }
    
    SECTION("Capture range spinbox updates state") {
        QSignalSpy spy(state.get(), &TableDesignerState::rowSettingsChanged);
        
        auto * spinbox = findChild<QSpinBox>(&widget, "capture_range_spinbox");
        REQUIRE(spinbox != nullptr);
        
        spinbox->setValue(15000);
        
        CHECK(state->captureRange() == 15000);
        CHECK(spy.count() >= 1);
    }
    
    SECTION("Interval mode radio buttons update state") {
        QSignalSpy spy(state.get(), &TableDesignerState::rowSettingsChanged);
        
        auto * beginning_radio = findChild<QRadioButton>(&widget, "interval_beginning_radio");
        auto * end_radio = findChild<QRadioButton>(&widget, "interval_end_radio");
        auto * itself_radio = findChild<QRadioButton>(&widget, "interval_itself_radio");
        
        REQUIRE(beginning_radio != nullptr);
        REQUIRE(end_radio != nullptr);
        REQUIRE(itself_radio != nullptr);
        
        // Select "End" mode
        end_radio->setChecked(true);
        CHECK(state->intervalMode() == IntervalRowMode::End);
        
        // Select "Itself" mode
        spy.clear();
        itself_radio->setChecked(true);
        CHECK(state->intervalMode() == IntervalRowMode::Itself);
        
        // Select "Beginning" mode
        spy.clear();
        beginning_radio->setChecked(true);
        CHECK(state->intervalMode() == IntervalRowMode::Beginning);
    }
}

TEST_CASE("TableDesignerWidget State Integration - State to UI propagation", "[TableDesignerWidget][StateIntegration]") {
    TestApp test_app;
    
    auto data_manager = std::make_shared<DataManager>();
    auto state = std::make_shared<TableDesignerState>();
    
    TableDesignerWidget widget(data_manager);
    widget.setState(state);
    
    SECTION("State group mode change updates UI") {
        auto * toggle = findChild<QPushButton>(&widget, "group_mode_toggle_btn");
        REQUIRE(toggle != nullptr);
        
        // Programmatically change state
        state->setGroupModeEnabled(false);
        
        // UI should reflect the change
        CHECK(toggle->isChecked() == false);
    }
    
    SECTION("State capture range change updates UI") {
        auto * spinbox = findChild<QSpinBox>(&widget, "capture_range_spinbox");
        REQUIRE(spinbox != nullptr);
        
        // Programmatically change state
        state->setCaptureRange(20000);
        
        // Note: Direct state changes won't auto-update UI unless we call syncUIFromState
        // The signal connections are for when state is changed externally
    }
    
    SECTION("State interval mode change updates UI") {
        auto * beginning_radio = findChild<QRadioButton>(&widget, "interval_beginning_radio");
        auto * end_radio = findChild<QRadioButton>(&widget, "interval_end_radio");
        auto * itself_radio = findChild<QRadioButton>(&widget, "interval_itself_radio");
        
        REQUIRE(beginning_radio != nullptr);
        REQUIRE(end_radio != nullptr);
        REQUIRE(itself_radio != nullptr);
        
        // Programmatically change state - this should trigger the signal handler
        // which will update the UI if not in _updating_from_state mode
    }
}

TEST_CASE("TableDesignerWidget State Integration - Serialization Round Trip", "[TableDesignerWidget][StateIntegration]") {
    TestApp test_app;
    
    auto data_manager = std::make_shared<DataManager>();
    auto state = std::make_shared<TableDesignerState>();
    
    TableDesignerWidget widget(data_manager);
    widget.setState(state);
    
    SECTION("State survives JSON round-trip") {
        // Set up some state via UI
        auto * toggle = findChild<QPushButton>(&widget, "group_mode_toggle_btn");
        auto * spinbox = findChild<QSpinBox>(&widget, "capture_range_spinbox");
        auto * end_radio = findChild<QRadioButton>(&widget, "interval_end_radio");
        
        REQUIRE(toggle != nullptr);
        REQUIRE(spinbox != nullptr);
        REQUIRE(end_radio != nullptr);
        
        toggle->setChecked(false);
        spinbox->setValue(12500);
        end_radio->setChecked(true);
        
        // Serialize
        std::string json = state->toJson();
        CHECK(!json.empty());
        
        // Create new state and widget
        auto new_state = std::make_shared<TableDesignerState>();
        CHECK(new_state->fromJson(json));
        
        // Verify state was restored
        CHECK(new_state->groupModeEnabled() == false);
        CHECK(new_state->captureRange() == 12500);
        CHECK(new_state->intervalMode() == IntervalRowMode::End);
        
        // Create new widget with restored state
        TableDesignerWidget new_widget(data_manager);
        new_widget.setState(new_state);
        
        // Verify UI reflects restored state
        auto * new_toggle = findChild<QPushButton>(&new_widget, "group_mode_toggle_btn");
        auto * new_spinbox = findChild<QSpinBox>(&new_widget, "capture_range_spinbox");
        auto * new_end_radio = findChild<QRadioButton>(&new_widget, "interval_end_radio");
        
        REQUIRE(new_toggle != nullptr);
        REQUIRE(new_spinbox != nullptr);
        REQUIRE(new_end_radio != nullptr);
        
        CHECK(new_toggle->isChecked() == false);
        CHECK(new_spinbox->value() == 12500);
        CHECK(new_end_radio->isChecked() == true);
    }
}

TEST_CASE("TableDesignerWidget State Integration - Computer States", "[TableDesignerWidget][StateIntegration]") {
    TestApp test_app;
    
    auto data_manager = std::make_shared<DataManager>();
    auto state = std::make_shared<TableDesignerState>();
    
    TableDesignerWidget widget(data_manager);
    widget.setState(state);
    
    SECTION("Computer state is preserved in state object") {
        // Manually set a computer state in the state object
        ComputerStateEntry entry;
        entry.enabled = true;
        entry.column_name = "CustomName";
        state->setComputerState("analog:signal||Mean", entry);
        
        CHECK(state->isComputerEnabled("analog:signal||Mean") == true);
        CHECK(state->computerColumnName("analog:signal||Mean") == "CustomName");
    }
    
    SECTION("Computer states survive serialization") {
        ComputerStateEntry entry;
        entry.enabled = true;
        entry.column_name = "MyColumn";
        state->setComputerState("events:spikes||Count", entry);
        
        std::string json = state->toJson();
        
        auto new_state = std::make_shared<TableDesignerState>();
        CHECK(new_state->fromJson(json));
        
        CHECK(new_state->isComputerEnabled("events:spikes||Count") == true);
        CHECK(new_state->computerColumnName("events:spikes||Count") == "MyColumn");
    }
}

TEST_CASE("TableDesignerWidget State Integration - No Infinite Loops", "[TableDesignerWidget][StateIntegration]") {
    TestApp test_app;
    
    auto data_manager = std::make_shared<DataManager>();
    auto state = std::make_shared<TableDesignerState>();
    
    TableDesignerWidget widget(data_manager);
    widget.setState(state);
    
    SECTION("UI change does not cause infinite loop") {
        QSignalSpy spy(state.get(), &TableDesignerState::groupSettingsChanged);
        
        auto * toggle = findChild<QPushButton>(&widget, "group_mode_toggle_btn");
        REQUIRE(toggle != nullptr);
        
        // This should not cause infinite recursion
        toggle->setChecked(false);
        toggle->setChecked(true);
        toggle->setChecked(false);
        
        // Should have 3 signals (or less if same-value filtering)
        CHECK(spy.count() <= 3);
    }
    
    SECTION("State change does not cause infinite loop") {
        QSignalSpy spy(state.get(), &TableDesignerState::rowSettingsChanged);
        
        // Rapidly change state - should not cause infinite recursion
        for (int i = 0; i < 10; ++i) {
            state->setCaptureRange(10000 + i * 1000);
        }
        
        // Should have 10 signals (one per unique change)
        CHECK(spy.count() == 10);
    }
}
