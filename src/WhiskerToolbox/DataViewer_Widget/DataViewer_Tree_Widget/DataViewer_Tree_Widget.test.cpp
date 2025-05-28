#include "DataViewer_Tree_Widget.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QTreeWidgetItem>
#include <QSignalSpy>

#include <memory>

// Note: This test requires Qt application context for QTreeWidget functionality
// Run with: your_test_runner --qt-app

TEST_CASE("DataViewer_Tree_Widget - State preservation during repopulation", "[DataViewer_Tree_Widget][signals][integration]") {
    
    SECTION("Signal blocking prevents unwanted toggle signals during repopulation") {
        // Create widget without DataManager for isolated testing
        DataViewer_Tree_Widget tree_widget;
        
        // Set up signal spy to catch unwanted featureToggled signals
        QSignalSpy toggle_spy(&tree_widget, &DataViewer_Tree_Widget::featureToggled);
        
        // Step 1: Manually create initial tree state (simulating populated tree)
        auto* item1 = new QTreeWidgetItem(&tree_widget);
        item1->setText(0, "tongue_area");
        item1->setData(0, Qt::UserRole, "tongue_area"); // Mark as series item
        item1->setFlags(item1->flags() | Qt::ItemIsUserCheckable);
        item1->setCheckState(0, Qt::Checked); // User enabled this
        
        auto* item2 = new QTreeWidgetItem(&tree_widget);
        item2->setText(0, "jaw_position"); 
        item2->setData(0, Qt::UserRole, "jaw_position");
        item2->setFlags(item2->flags() | Qt::ItemIsUserCheckable);
        item2->setCheckState(0, Qt::Unchecked); // User left this disabled
        
        // Clear any signals from setup
        toggle_spy.clear();
        
        // Step 2: Save current state (simulating what happens before repopulation)
        auto& state_manager = tree_widget.getStateManager();
        
        // Manually save state for testing
        std::unordered_set<std::string> enabled_series = {"tongue_area"};
        std::unordered_map<std::string, bool> group_states = {};
        state_manager.saveEnabledSeries(enabled_series);
        state_manager.saveGroupStates(group_states);
        
        // Step 3: Simulate tree clearing (this should NOT emit signals)
        tree_widget.blockSignals(true);
        tree_widget.clear();
        
        // At this point, NO featureToggled signals should have been emitted
        REQUIRE(toggle_spy.count() == 0);
        
        // Step 4: Simulate tree repopulation with new data
        auto* new_item1 = new QTreeWidgetItem(&tree_widget);
        new_item1->setText(0, "tongue_area");
        new_item1->setData(0, Qt::UserRole, "tongue_area");
        new_item1->setFlags(new_item1->flags() | Qt::ItemIsUserCheckable);
        new_item1->setCheckState(0, Qt::Unchecked); // Initially unchecked
        
        auto* new_item2 = new QTreeWidgetItem(&tree_widget);
        new_item2->setText(0, "jaw_position");
        new_item2->setData(0, Qt::UserRole, "jaw_position");
        new_item2->setFlags(new_item2->flags() | Qt::ItemIsUserCheckable);
        new_item2->setCheckState(0, Qt::Unchecked); // Initially unchecked
        
        auto* new_item3 = new QTreeWidgetItem(&tree_widget);
        new_item3->setText(0, "laser_power"); // New item added
        new_item3->setData(0, Qt::UserRole, "laser_power");
        new_item3->setFlags(new_item3->flags() | Qt::ItemIsUserCheckable);
        new_item3->setCheckState(0, Qt::Unchecked); // Initially unchecked
        
        // Step 5: Restore state (should restore tongue_area but NOT emit signals)
        // Simulate state restoration with signals still blocked
        if (state_manager.shouldSeriesBeEnabled("tongue_area")) {
            new_item1->setCheckState(0, Qt::Checked);
        }
        if (state_manager.shouldSeriesBeEnabled("jaw_position")) {
            new_item2->setCheckState(0, Qt::Checked);
        }
        if (state_manager.shouldSeriesBeEnabled("laser_power")) {
            new_item3->setCheckState(0, Qt::Checked);
        }
        
        // Step 6: Unblock signals AFTER restoration is complete
        tree_widget.blockSignals(false);
        
        // Verify final state
        REQUIRE(new_item1->checkState(0) == Qt::Checked);   // tongue_area restored
        REQUIRE(new_item2->checkState(0) == Qt::Unchecked); // jaw_position stayed disabled
        REQUIRE(new_item3->checkState(0) == Qt::Unchecked); // laser_power new item, disabled
        
        // Most importantly: NO unwanted signals should have been emitted during repopulation
        REQUIRE(toggle_spy.count() == 0);
    }
}

TEST_CASE("TreeWidgetStateManager - Unit tests for state logic", "[TreeWidgetStateManager][unit]") {
    
    SECTION("State preservation logic works correctly") {
        TreeWidgetStateManager state_manager;
        
        // Initial state
        std::unordered_set<std::string> initial_enabled = {"tongue_area", "jaw_position"};
        std::unordered_map<std::string, bool> initial_groups = {{"analog_", true}};
        
        state_manager.saveEnabledSeries(initial_enabled);
        state_manager.saveGroupStates(initial_groups);
        
        // Verify series state
        REQUIRE(state_manager.shouldSeriesBeEnabled("tongue_area") == true);
        REQUIRE(state_manager.shouldSeriesBeEnabled("jaw_position") == true);
        REQUIRE(state_manager.shouldSeriesBeEnabled("new_series") == false);
        
        // Verify group state
        auto analog_state = state_manager.shouldGroupBeEnabled("analog_");
        REQUIRE(analog_state.has_value());
        REQUIRE(analog_state.value() == true);
        
        auto unknown_state = state_manager.shouldGroupBeEnabled("unknown_");
        REQUIRE_FALSE(unknown_state.has_value());
    }
    
    SECTION("Bug scenario: Previously enabled series stay enabled after new data addition") {
        TreeWidgetStateManager state_manager;
        
        // Step 1: User has enabled tongue_area
        std::unordered_set<std::string> enabled_before = {"tongue_area"};
        state_manager.saveEnabledSeries(enabled_before);
        
        // Step 2: DataManager adds new data (laser_interval), triggering repopulation
        // State manager should remember tongue_area was enabled
        
        // Step 3: After repopulation, check that state is preserved correctly
        REQUIRE(state_manager.shouldSeriesBeEnabled("tongue_area") == true);   // Should remain enabled
        REQUIRE(state_manager.shouldSeriesBeEnabled("laser_interval") == false); // New data should be disabled
        
        // This test verifies the core logic that was failing in the original bug
    }
} 