#include <catch2/catch_test_macros.hpp>

#include "Analysis_Dashboard/Tables/TableDesignerWidget.hpp"
#include "Analysis_Dashboard/Tables/TableManager.hpp"
#include "DataManager/DataManager.hpp"

#include <QApplication>
#include <QWidget>
#include <memory>

#include "../fixtures/qt_test_fixtures.hpp"
#include "../fixtures/data_manager_test_fixtures.hpp"
#include "../fixtures/signal_probe.hpp"

/**
 * @brief Combined test fixture for table designer widget testing
 * 
 * This fixture combines Qt widget testing capabilities with a populated DataManager
 * for testing table designer functionality with real data.
 */
class TableDesignerTestFixture : public QtWidgetTestFixture, public DataManagerTestFixture {
protected:
    TableDesignerTestFixture() : QtWidgetTestFixture(), DataManagerTestFixture() {
        // Additional setup if needed
    }
    
    /**
     * @brief Get the DataManager as a shared pointer for widget testing
     * @return Shared pointer to the DataManager
     */
    std::shared_ptr<DataManager> getDataManagerShared() {
        return std::shared_ptr<DataManager>(getDataManagerPtr(), [](DataManager*) {
            // Custom deleter - don't delete since DataManagerTestFixture owns it
        });
    }
};

TEST_CASE_METHOD(TableDesignerTestFixture, 
    "Analysis Dashboard - TableDesignerWidget - Widget can be created and added to application",
     "[widget][table_designer]") {
    // Use the populated DataManager from the fixture
    auto data_manager = getDataManagerShared();
    REQUIRE(data_manager != nullptr);
    
    // Verify that the DataManager has test data
    auto all_keys = data_manager->getAllKeys();
    REQUIRE(all_keys.size() > 0);
    
    // Create a table manager
    auto table_manager = std::make_unique<TableManager>(data_manager);
    
    // Create the table designer widget
    auto table_designer = std::make_unique<TableDesignerWidget>(table_manager.get(), data_manager);
    
    // Verify the widget was created successfully
    REQUIRE(table_designer != nullptr);
    
    // Verify the widget is not visible by default and has a valid size
    REQUIRE(table_designer->isVisible() == false); // Widgets are not visible by default
    REQUIRE(table_designer->size().isValid() == true);
    
    // Show the widget (this would normally be done by a parent widget)
    table_designer->show();
    
    // Process events to ensure the widget is properly initialized
    processEvents();
    
    // Verify the widget is now visible
    REQUIRE(table_designer->isVisible() == true);
    
    // Verify the widget has a reasonable size after showing
    REQUIRE(table_designer->width() > 0);
    REQUIRE(table_designer->height() > 0);
    
    // Test that the widget can be hidden and shown again
    table_designer->hide();
    processEvents();
    REQUIRE(table_designer->isVisible() == false);
    
    table_designer->show();
    processEvents();
    REQUIRE(table_designer->isVisible() == true);
    
    // Test that the widget can be resized
    table_designer->resize(400, 300);
    processEvents();
    REQUIRE(table_designer->width() == 400);
    REQUIRE(table_designer->height() == 300);
    
    // Widget will be automatically cleaned up when unique_ptr goes out of scope
}

TEST_CASE_METHOD(TableDesignerTestFixture, "TableManager can create and manage tables", "[table_manager]") {
    // Use the populated DataManager from the fixture
    auto data_manager = getDataManagerShared();
    REQUIRE(data_manager != nullptr);
    
    // Verify that the DataManager has test data
    auto all_keys = data_manager->getAllKeys();
    REQUIRE(all_keys.size() > 0);
    
    // Verify specific test data is present
    REQUIRE(data_manager->getData<PointData>("test_points") != nullptr);
    REQUIRE(data_manager->getData<LineData>("test_lines") != nullptr);
    REQUIRE(data_manager->getData<AnalogTimeSeries>("test_analog") != nullptr);
    
    // Create a table manager
    auto table_manager = std::make_unique<TableManager>(data_manager);
    
    // Test creating a new table
    QString table_id = "test_table_1";
    QString table_name = "Test Table";
    QString table_description = "A test table for unit testing";
    
    bool created = table_manager->createTable(table_id, table_name, table_description);
    REQUIRE(created == true);
    
    // Verify the table exists
    REQUIRE(table_manager->hasTable(table_id) == true);
    
    // Get table info
    auto table_info = table_manager->getTableInfo(table_id);
    REQUIRE(table_info.id == table_id);
    REQUIRE(table_info.name == table_name);
    REQUIRE(table_info.description == table_description);
    
    // Test updating table info
    QString new_name = "Updated Test Table";
    QString new_description = "Updated description";
    bool updated = table_manager->updateTableInfo(table_id, new_name, new_description);
    REQUIRE(updated == true);
    
    // Verify the update
    auto updated_info = table_manager->getTableInfo(table_id);
    REQUIRE(updated_info.name == new_name);
    REQUIRE(updated_info.description == new_description);
    
    // Test removing the table
    bool removed = table_manager->removeTable(table_id);
    REQUIRE(removed == true);
    REQUIRE(table_manager->hasTable(table_id) == false);
}

TEST_CASE_METHOD(TableDesignerTestFixture, "TableDesignerWidget works with populated DataManager", "[widget][table_designer][data]") {
    // Use the populated DataManager from the fixture
    auto data_manager = getDataManagerShared();
    REQUIRE(data_manager != nullptr);
    
    // Create a table manager
    auto table_manager = std::make_unique<TableManager>(data_manager);
    
    // Create the table designer widget
    auto table_designer = std::make_unique<TableDesignerWidget>(table_manager.get(), data_manager);
    
    // Show the widget
    table_designer->show();
    processEvents();
    
    // Test that the widget can access the DataManager's data
    auto all_keys = data_manager->getAllKeys();
    REQUIRE(all_keys.size() > 0);
    
    // Verify that the widget can handle the populated DataManager
    REQUIRE(table_designer->isVisible() == true);
    REQUIRE(table_designer->width() > 0);
    REQUIRE(table_designer->height() > 0);
    
    // Test that the widget remains stable with the test data
    table_designer->resize(600, 400);
    processEvents();
    REQUIRE(table_designer->width() == 600);
    REQUIRE(table_designer->height() == 400);
}

TEST_CASE_METHOD(TableDesignerTestFixture, "TableDesignerWidget signal monitoring", "[widget][table_designer][signals]") {
    // Use the populated DataManager from the fixture
    auto data_manager = getDataManagerShared();
    REQUIRE(data_manager != nullptr);
    
    // Create a table manager
    auto table_manager = std::make_unique<TableManager>(data_manager);
    
    // Create signal probes for TableDesignerWidget signals
    SignalProbe tableCreatedProbe;
    SignalProbe tableDeletedProbe;
    
    // Create the table designer widget
    auto table_designer = std::make_unique<TableDesignerWidget>(table_manager.get(), data_manager);
    
    // Connect probes to widget signals
    tableCreatedProbe.connectTo(table_designer.get(), &TableDesignerWidget::tableCreated);
    tableDeletedProbe.connectTo(table_designer.get(), &TableDesignerWidget::tableDeleted);
    
    // Show the widget
    table_designer->show();
    processEvents();
    
    // Initially, no signals should be emitted
    REQUIRE(!tableCreatedProbe.wasTriggered());
    REQUIRE(!tableDeletedProbe.wasTriggered());
    REQUIRE(tableCreatedProbe.getCallCount() == 0);
    REQUIRE(tableDeletedProbe.getCallCount() == 0);
    
    // Test that the widget is properly initialized
    REQUIRE(table_designer->isVisible() == true);
}

TEST_CASE_METHOD(TableDesignerTestFixture, "TableManager signal monitoring", "[table_manager][signals]") {
    // Use the populated DataManager from the fixture
    auto data_manager = getDataManagerShared();
    REQUIRE(data_manager != nullptr);
    
    // Create signal probes for TableManager signals
    SignalProbe tableCreatedProbe;
    SignalProbe tableRemovedProbe;
    SignalProbe tableInfoUpdatedProbe;
    SignalProbe tableDataChangedProbe;
    
    // Create a table manager
    auto table_manager = std::make_unique<TableManager>(data_manager);
    
    // Connect probes to table manager signals
    tableCreatedProbe.connectTo(table_manager.get(), &TableManager::tableCreated);
    tableRemovedProbe.connectTo(table_manager.get(), &TableManager::tableRemoved);
    tableInfoUpdatedProbe.connectTo(table_manager.get(), &TableManager::tableInfoUpdated);
    tableDataChangedProbe.connectTo(table_manager.get(), &TableManager::tableDataChanged);
    
    // Initially, no signals should be emitted
    REQUIRE(!tableCreatedProbe.wasTriggered());
    REQUIRE(!tableRemovedProbe.wasTriggered());
    REQUIRE(!tableInfoUpdatedProbe.wasTriggered());
    REQUIRE(!tableDataChangedProbe.wasTriggered());
    
    // Test creating a table
    QString table_id = "test_table_1";
    QString table_name = "Test Table";
    QString table_description = "A test table for signal testing";
    
    bool created = table_manager->createTable(table_id, table_name, table_description);
    REQUIRE(created == true);
    
    // Verify the tableCreated signal was emitted
    REQUIRE(tableCreatedProbe.wasTriggered());
    REQUIRE(tableCreatedProbe.getCallCount() == 1);
    REQUIRE(tableCreatedProbe.getLastArg() == table_id);
    
    // Verify other signals were not emitted
    REQUIRE(!tableRemovedProbe.wasTriggered());
    REQUIRE(!tableInfoUpdatedProbe.wasTriggered());
    REQUIRE(!tableDataChangedProbe.wasTriggered());
    
    // Test updating table info
    QString new_name = "Updated Test Table";
    QString new_description = "Updated description";
    bool updated = table_manager->updateTableInfo(table_id, new_name, new_description);
    REQUIRE(updated == true);
    
    // Verify the tableInfoUpdated signal was emitted
    REQUIRE(tableInfoUpdatedProbe.wasTriggered());
    REQUIRE(tableInfoUpdatedProbe.getCallCount() == 1);
    REQUIRE(tableInfoUpdatedProbe.getLastArg() == table_id);
    
    // Verify the tableCreated signal count is still 1
    REQUIRE(tableCreatedProbe.getCallCount() == 1);
    
    // Test removing the table
    bool removed = table_manager->removeTable(table_id);
    REQUIRE(removed == true);
    
    // Verify the tableRemoved signal was emitted
    REQUIRE(tableRemovedProbe.wasTriggered());
    REQUIRE(tableRemovedProbe.getCallCount() == 1);
    REQUIRE(tableRemovedProbe.getLastArg() == table_id);
    
    // Verify signal counts
    REQUIRE(tableCreatedProbe.getCallCount() == 1);
    REQUIRE(tableInfoUpdatedProbe.getCallCount() == 1);
    REQUIRE(tableRemovedProbe.getCallCount() == 1);
    REQUIRE(!tableDataChangedProbe.wasTriggered());
}

TEST_CASE_METHOD(TableDesignerTestFixture, "TableDesignerWidget and TableManager signal integration", "[widget][table_designer][table_manager][signals]") {
    // Use the populated DataManager from the fixture
    auto data_manager = getDataManagerShared();
    REQUIRE(data_manager != nullptr);
    
    // Create signal probes for both widget and manager
    SignalProbe widgetTableCreatedProbe;
    SignalProbe managerTableCreatedProbe;
    SignalProbe managerTableRemovedProbe;
    
    // Create a table manager
    auto table_manager = std::make_unique<TableManager>(data_manager);
    
    // Create the table designer widget
    auto table_designer = std::make_unique<TableDesignerWidget>(table_manager.get(), data_manager);
    
    // Connect probes to both widget and manager signals
    widgetTableCreatedProbe.connectTo(table_designer.get(), &TableDesignerWidget::tableCreated);
    managerTableCreatedProbe.connectTo(table_manager.get(), &TableManager::tableCreated);
    managerTableRemovedProbe.connectTo(table_manager.get(), &TableManager::tableRemoved);
    
    // Show the widget
    table_designer->show();
    processEvents();
    
    // Initially, no signals should be emitted
    REQUIRE(!widgetTableCreatedProbe.wasTriggered());
    REQUIRE(!managerTableCreatedProbe.wasTriggered());
    REQUIRE(!managerTableRemovedProbe.wasTriggered());
    
    // Test creating a table through the manager
    QString table_id = "integration_test_table";
    QString table_name = "Integration Test Table";
    QString table_description = "Testing signal integration";
    
    bool created = table_manager->createTable(table_id, table_name, table_description);
    REQUIRE(created == true);
    
    // Verify manager signal was emitted
    REQUIRE(managerTableCreatedProbe.wasTriggered());
    REQUIRE(managerTableCreatedProbe.getCallCount() == 1);
    REQUIRE(managerTableCreatedProbe.getLastArg() == table_id);
    
    // The widget should not emit tableCreated signal for manager-created tables
    // (it only emits when it creates tables through its UI)
    REQUIRE(!widgetTableCreatedProbe.wasTriggered());
    
    // Test removing the table
    bool removed = table_manager->removeTable(table_id);
    REQUIRE(removed == true);
    
    // Verify manager signal was emitted
    REQUIRE(managerTableRemovedProbe.wasTriggered());
    REQUIRE(managerTableRemovedProbe.getCallCount() == 1);
    REQUIRE(managerTableRemovedProbe.getLastArg() == table_id);
    
    // Verify final signal counts
    REQUIRE(managerTableCreatedProbe.getCallCount() == 1);
    REQUIRE(managerTableRemovedProbe.getCallCount() == 1);
    REQUIRE(!widgetTableCreatedProbe.wasTriggered());
} 