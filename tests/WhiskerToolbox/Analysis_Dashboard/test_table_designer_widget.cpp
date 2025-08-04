#include <catch2/catch_test_macros.hpp>

#include "Analysis_Dashboard/Tables/TableDesignerWidget.hpp"
#include "Analysis_Dashboard/Tables/TableManager.hpp"
#include "DataManager/DataManager.hpp"

#include <QApplication>
#include <QWidget>
#include <memory>

#include "../fixtures/qt_test_fixtures.hpp"
#include "../fixtures/data_manager_test_fixtures.hpp"

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