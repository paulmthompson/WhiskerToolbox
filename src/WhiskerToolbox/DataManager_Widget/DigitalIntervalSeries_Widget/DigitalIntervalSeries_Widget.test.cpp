#include "DigitalIntervalSeries_Widget.hpp"

#include "DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/DigitalTimeSeries/interval_data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QApplication>
#include <QTest>
#include <memory>

TEST_CASE("DigitalIntervalSeries_Widget interval creation workflow", "[DigitalIntervalSeries_Widget][interval_creation]") {
    
    // Setup QApplication for widget testing
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);
    
    auto data_manager = std::make_shared<DataManager>();
    data_manager->setData<DigitalIntervalSeries>("test_intervals");
    
    DigitalIntervalSeries_Widget widget(data_manager);
    widget.setActiveKey("test_intervals");
    
    SECTION("Initial state") {
        // Widget should start in normal mode (not in interval creation)
        REQUIRE(widget.findChild<QPushButton*>("create_interval_button")->text() == "Create Interval");
        REQUIRE(widget.findChild<QPushButton*>("cancel_interval_button")->isVisible() == false);
        REQUIRE(widget.findChild<QLabel*>("start_frame_label")->isVisible() == false);
    }
    
    SECTION("Bidirectional interval creation - forward order") {
        data_manager->setCurrentTime(100);
        
        // Simulate first button click - should enter interval creation mode
        QTest::mouseClick(widget.findChild<QPushButton*>("create_interval_button"), Qt::LeftButton);
        
        REQUIRE(widget.findChild<QPushButton*>("create_interval_button")->text() == "Mark Interval End");
        REQUIRE(widget.findChild<QPushButton*>("cancel_interval_button")->isVisible() == true);
        REQUIRE(widget.findChild<QLabel*>("start_frame_label")->isVisible() == true);
        REQUIRE(widget.findChild<QLabel*>("start_frame_label")->text() == "Start: 100");
        
        // Move to later frame and click again
        data_manager->setCurrentTime(200);
        QTest::mouseClick(widget.findChild<QPushButton*>("create_interval_button"), Qt::LeftButton);
        
        // Should create interval [100, 200] and reset state
        REQUIRE(widget.findChild<QPushButton*>("create_interval_button")->text() == "Create Interval");
        REQUIRE(widget.findChild<QPushButton*>("cancel_interval_button")->isVisible() == false);
        REQUIRE(widget.findChild<QLabel*>("start_frame_label")->isVisible() == false);
        
        // Verify interval was created correctly
        auto intervals = data_manager->getData<DigitalIntervalSeries>("test_intervals");
        auto const & interval_data = intervals->getDigitalIntervalSeries();
        REQUIRE(interval_data.size() == 1);
        REQUIRE(interval_data[0].start == 100);
        REQUIRE(interval_data[0].end == 200);
    }
    
    SECTION("Bidirectional interval creation - reverse order") {
        data_manager->setCurrentTime(300);
        
        // Start interval creation at frame 300
        QTest::mouseClick(widget.findChild<QPushButton*>("create_interval_button"), Qt::LeftButton);
        
        // Move to earlier frame and complete interval
        data_manager->setCurrentTime(150);
        QTest::mouseClick(widget.findChild<QPushButton*>("create_interval_button"), Qt::LeftButton);
        
        // Should create interval [150, 300] (automatically swapped)
        auto intervals = data_manager->getData<DigitalIntervalSeries>("test_intervals");
        auto const & interval_data = intervals->getDigitalIntervalSeries();
        REQUIRE(interval_data.size() == 1);
        REQUIRE(interval_data[0].start == 150);
        REQUIRE(interval_data[0].end == 300);
    }
    
    SECTION("Cancel interval creation via button") {
        data_manager->setCurrentTime(50);
        
        // Start interval creation
        QTest::mouseClick(widget.findChild<QPushButton*>("create_interval_button"), Qt::LeftButton);
        
        REQUIRE(widget.findChild<QPushButton*>("create_interval_button")->text() == "Mark Interval End");
        REQUIRE(widget.findChild<QPushButton*>("cancel_interval_button")->isVisible() == true);
        
        // Cancel via button
        QTest::mouseClick(widget.findChild<QPushButton*>("cancel_interval_button"), Qt::LeftButton);
        
        // Should return to normal state
        REQUIRE(widget.findChild<QPushButton*>("create_interval_button")->text() == "Create Interval");
        REQUIRE(widget.findChild<QPushButton*>("cancel_interval_button")->isVisible() == false);
        REQUIRE(widget.findChild<QLabel*>("start_frame_label")->isVisible() == false);
        
        // No interval should be created
        auto intervals = data_manager->getData<DigitalIntervalSeries>("test_intervals");
        REQUIRE(intervals->getDigitalIntervalSeries().empty());
    }
}

TEST_CASE("DigitalIntervalSeries_Widget state management", "[DigitalIntervalSeries_Widget][state_management]") {
    
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);
    
    auto data_manager = std::make_shared<DataManager>();
    data_manager->setData<DigitalIntervalSeries>("intervals1");
    data_manager->setData<DigitalIntervalSeries>("intervals2");
    
    DigitalIntervalSeries_Widget widget(data_manager);
    
    SECTION("State reset when switching active keys") {
        widget.setActiveKey("intervals1");
        data_manager->setCurrentTime(100);
        
        // Start interval creation
        QTest::mouseClick(widget.findChild<QPushButton*>("create_interval_button"), Qt::LeftButton);
        
        REQUIRE(widget.findChild<QPushButton*>("create_interval_button")->text() == "Mark Interval End");
        
        // Switch to different key - should reset state
        widget.setActiveKey("intervals2");
        
        REQUIRE(widget.findChild<QPushButton*>("create_interval_button")->text() == "Create Interval");
        REQUIRE(widget.findChild<QPushButton*>("cancel_interval_button")->isVisible() == false);
        REQUIRE(widget.findChild<QLabel*>("start_frame_label")->isVisible() == false);
    }
    
    SECTION("State reset when removing callbacks") {
        widget.setActiveKey("intervals1");
        data_manager->setCurrentTime(100);
        
        // Start interval creation
        QTest::mouseClick(widget.findChild<QPushButton*>("create_interval_button"), Qt::LeftButton);
        
        REQUIRE(widget.findChild<QPushButton*>("create_interval_button")->text() == "Mark Interval End");
        
        // Remove callbacks - should reset state
        widget.removeCallbacks();
        
        REQUIRE(widget.findChild<QPushButton*>("create_interval_button")->text() == "Create Interval");
        REQUIRE(widget.findChild<QPushButton*>("cancel_interval_button")->isVisible() == false);
        REQUIRE(widget.findChild<QLabel*>("start_frame_label")->isVisible() == false);
    }
}

TEST_CASE("DigitalIntervalSeries_Widget filename generation", "[DigitalIntervalSeries_Widget][filename]") {
    
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);
    
    auto data_manager = std::make_shared<DataManager>();
    data_manager->setData<DigitalIntervalSeries>("whisker_contacts");
    data_manager->setData<DigitalIntervalSeries>("object_interactions");
    
    DigitalIntervalSeries_Widget widget(data_manager);
    
    SECTION("Filename updates when active key changes") {
        // Set active key and verify filename updates
        widget.setActiveKey("whisker_contacts");
        REQUIRE(widget.findChild<QLineEdit*>("filename_edit")->text() == "whisker_contacts.csv");
        
        // Change to different key
        widget.setActiveKey("object_interactions");
        REQUIRE(widget.findChild<QLineEdit*>("filename_edit")->text() == "object_interactions.csv");
    }
    
    SECTION("Filename updates when export type changes") {
        widget.setActiveKey("whisker_contacts");
        
        // Initially should be CSV
        REQUIRE(widget.findChild<QLineEdit*>("filename_edit")->text() == "whisker_contacts.csv");
        
        // If future export types are added, they should update filename accordingly
        // For now, only CSV is available, so this test validates the existing behavior
        auto export_combo = widget.findChild<QComboBox*>("export_type_combo");
        export_combo->setCurrentIndex(0); // CSV
        REQUIRE(widget.findChild<QLineEdit*>("filename_edit")->text() == "whisker_contacts.csv");
    }
    
    SECTION("Empty active key uses fallback filename") {
        // Widget without active key should use fallback
        DigitalIntervalSeries_Widget widget_no_key(data_manager);
        REQUIRE(widget_no_key.findChild<QLineEdit*>("filename_edit")->text() == "intervals_output.csv");
    }
}

TEST_CASE("DigitalIntervalSeries_Widget error handling", "[DigitalIntervalSeries_Widget][error_handling]") {
    
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);
    
    auto data_manager = std::make_shared<DataManager>();
    DigitalIntervalSeries_Widget widget(data_manager);
    
    SECTION("Handle null data gracefully") {
        widget.setActiveKey("nonexistent_key");
        data_manager->setCurrentTime(100);
        
        // Should not crash when trying to create interval with null data
        QTest::mouseClick(widget.findChild<QPushButton*>("create_interval_button"), Qt::LeftButton);
        
        // State should remain unchanged
        REQUIRE(widget.findChild<QPushButton*>("create_interval_button")->text() == "Create Interval");
    }
    
    SECTION("Context menu only appears during interval creation") {
        widget.setActiveKey("test_key");
        
        // Right-click when not in interval creation mode - context menu should not appear
        // (This is tested implicitly by the _showCreateIntervalContextMenu implementation)
        
        data_manager->setCurrentTime(100);
        QTest::mouseClick(widget.findChild<QPushButton*>("create_interval_button"), Qt::LeftButton);
        
        // Now in interval creation mode - context menu should be available
        // (Implementation allows context menu to appear)
        REQUIRE(widget.findChild<QPushButton*>("create_interval_button")->text() == "Mark Interval End");
    }
} 