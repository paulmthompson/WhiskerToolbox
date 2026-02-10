/**
 * @file ACFPropertiesWidget.test.cpp
 * @brief Unit tests for ACFPropertiesWidget
 * 
 * Tests the properties widget for ACFWidget, including:
 * - Combo box population with DigitalEventSeries keys
 * - DataManager observer callback functionality
 * - Combo box refresh when data is added/removed
 */

#include "Plots/ACFWidget/UI/ACFPropertiesWidget.hpp"
#include "Plots/ACFWidget/Core/ACFState.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <catch2/catch_test_macros.hpp>
#include <QApplication>
#include <QComboBox>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

// ==================== Helper Functions ====================

/**
 * @brief Create a test TimeFrame
 */
static std::shared_ptr<TimeFrame> createTestTimeFrame()
{
    std::vector<int> times;
    times.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        times.push_back(i);
    }
    return std::make_shared<TimeFrame>(times);
}

/**
 * @brief Create a test DigitalEventSeries with some events
 */
std::shared_ptr<DigitalEventSeries> createTestEventSeries(std::string const & key)
{
    std::vector<TimeFrameIndex> events = {
        TimeFrameIndex(100),
        TimeFrameIndex(200),
        TimeFrameIndex(300),
        TimeFrameIndex(400),
        TimeFrameIndex(500)
    };
    return std::make_shared<DigitalEventSeries>(events);
}

// ==================== Combo Box Population Tests ====================

TEST_CASE("ACFPropertiesWidget combo box population", "[ACFPropertiesWidget]")
{
    // Create QApplication if it doesn't exist (required for Qt widgets)
    int argc = 0;
    QApplication app(argc, nullptr);
    
    SECTION("empty combo box when no data available") {
        auto data_manager = std::make_shared<DataManager>();
        auto state = std::make_shared<ACFState>();
        
        ACFPropertiesWidget widget(state, data_manager);
        
        auto * combo = widget.findChild<QComboBox *>("event_key_combo");
        REQUIRE(combo != nullptr);
        REQUIRE(combo->count() == 0);  // Empty when no data available
    }
    
    SECTION("combo box populated with DigitalEventSeries keys") {
        auto data_manager = std::make_shared<DataManager>();
        auto state = std::make_shared<ACFState>();
        
        // Create and set TimeFrame (remove existing if present)
        data_manager->removeTime(TimeKey("time"));
        auto time_frame = createTestTimeFrame();
        data_manager->setTime(TimeKey("time"), time_frame);
        
        // Add some event series
        auto event_series_1 = createTestEventSeries("events_1");
        auto event_series_2 = createTestEventSeries("events_2");
        event_series_1->setTimeFrame(time_frame);
        event_series_2->setTimeFrame(time_frame);
        
        data_manager->setData<DigitalEventSeries>("events_1", event_series_1, TimeKey("time"));
        data_manager->setData<DigitalEventSeries>("events_2", event_series_2, TimeKey("time"));
        
        // Process events to allow observer callback to run
        QApplication::processEvents();
        
        ACFPropertiesWidget widget(state, data_manager);
        
        // Process events again to ensure combo box is populated
        QApplication::processEvents();
        
        auto * combo = widget.findChild<QComboBox *>("event_key_combo");
        REQUIRE(combo != nullptr);
        REQUIRE(combo->isEnabled());
        REQUIRE(combo->count() == 2);
        REQUIRE(combo->itemText(0).toStdString() == "events_1");
        REQUIRE(combo->itemText(1).toStdString() == "events_2");
    }
}

// ==================== Observer Callback Tests ====================

TEST_CASE("ACFPropertiesWidget observer callback", "[ACFPropertiesWidget]")
{
    // Create QApplication if it doesn't exist (required for Qt widgets)
    int argc = 0;
    QApplication app(argc, nullptr);
    
    SECTION("combo box refreshes when data is added") {
        auto data_manager = std::make_shared<DataManager>();
        auto state = std::make_shared<ACFState>();
        
        ACFPropertiesWidget widget(state, data_manager);
        
        auto * combo = widget.findChild<QComboBox *>("event_key_combo");
        REQUIRE(combo != nullptr);
        
        // Initially empty
        REQUIRE(combo->count() == 0);  // Empty when no data available
        
        // Create and set TimeFrame (remove existing if present)
        data_manager->removeTime(TimeKey("time"));
        auto time_frame = createTestTimeFrame();
        data_manager->setTime(TimeKey("time"), time_frame);
        
        // Add an event series
        auto event_series = createTestEventSeries("new_events");
        event_series->setTimeFrame(time_frame);
        data_manager->setData<DigitalEventSeries>("new_events", event_series, TimeKey("time"));
        
        // Process events to allow Qt signals/slots to process
        QApplication::processEvents();
        
        // Combo box should now be populated
        REQUIRE(combo->isEnabled());
        REQUIRE(combo->count() >= 1);
        bool found = false;
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemText(i).toStdString() == "new_events") {
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }
    
    SECTION("combo box refreshes when multiple series are added") {
        auto data_manager = std::make_shared<DataManager>();
        auto state = std::make_shared<ACFState>();
        
        ACFPropertiesWidget widget(state, data_manager);
        
        auto * combo = widget.findChild<QComboBox *>("event_key_combo");
        REQUIRE(combo != nullptr);
        
        // Create and set TimeFrame (remove existing if present)
        data_manager->removeTime(TimeKey("time"));
        auto time_frame = createTestTimeFrame();
        data_manager->setTime(TimeKey("time"), time_frame);
        
        // Add multiple event series
        auto event_series_1 = createTestEventSeries("events_1");
        auto event_series_2 = createTestEventSeries("events_2");
        event_series_1->setTimeFrame(time_frame);
        event_series_2->setTimeFrame(time_frame);
        
        data_manager->setData<DigitalEventSeries>("events_1", event_series_1, TimeKey("time"));
        QApplication::processEvents();
        
        data_manager->setData<DigitalEventSeries>("events_2", event_series_2, TimeKey("time"));
        QApplication::processEvents();
        
        // Combo box should contain both events
        REQUIRE(combo->isEnabled());
        REQUIRE(combo->count() == 2);
        
        // Verify all keys are present
        std::vector<std::string> items;
        for (int i = 0; i < combo->count(); ++i) {
            items.push_back(combo->itemText(i).toStdString());
        }
        REQUIRE(std::find(items.begin(), items.end(), "events_1") != items.end());
        REQUIRE(std::find(items.begin(), items.end(), "events_2") != items.end());
    }
    
    SECTION("combo box refreshes when data is removed") {
        auto data_manager = std::make_shared<DataManager>();
        auto state = std::make_shared<ACFState>();
        
        // Create and set TimeFrame (remove existing if present)
        data_manager->removeTime(TimeKey("time"));
        auto time_frame = createTestTimeFrame();
        data_manager->setTime(TimeKey("time"), time_frame);
        
        // Add some event series
        auto event_series_1 = createTestEventSeries("events_1");
        auto event_series_2 = createTestEventSeries("events_2");
        event_series_1->setTimeFrame(time_frame);
        event_series_2->setTimeFrame(time_frame);
        
        data_manager->setData<DigitalEventSeries>("events_1", event_series_1, TimeKey("time"));
        data_manager->setData<DigitalEventSeries>("events_2", event_series_2, TimeKey("time"));
        QApplication::processEvents();
        
        ACFPropertiesWidget widget(state, data_manager);
        QApplication::processEvents();
        
        auto * combo = widget.findChild<QComboBox *>("event_key_combo");
        REQUIRE(combo != nullptr);
        
        // Initially should have 2 items
        REQUIRE(combo->count() == 2);
        
        // Remove one event series
        data_manager->deleteData("events_1");
        QApplication::processEvents();
        
        // Combo box should now have 1 item
        REQUIRE(combo->count() == 1);
        REQUIRE(combo->itemText(0).toStdString() == "events_2");
    }
    
    SECTION("combo box preserves selection when repopulated") {
        auto data_manager = std::make_shared<DataManager>();
        auto state = std::make_shared<ACFState>();
        
        // Create and set TimeFrame (remove existing if present)
        data_manager->removeTime(TimeKey("time"));
        auto time_frame = createTestTimeFrame();
        data_manager->setTime(TimeKey("time"), time_frame);
        
        // Add an event series
        auto event_series_1 = createTestEventSeries("events_1");
        event_series_1->setTimeFrame(time_frame);
        data_manager->setData<DigitalEventSeries>("events_1", event_series_1, TimeKey("time"));
        QApplication::processEvents();
        
        ACFPropertiesWidget widget(state, data_manager);
        QApplication::processEvents();
        
        auto * combo = widget.findChild<QComboBox *>("event_key_combo");
        REQUIRE(combo != nullptr);
        
        // Set selection in state
        state->setEventKey(QStringLiteral("events_1"));
        QApplication::processEvents();
        
        // Verify combo box has selection
        REQUIRE(combo->currentIndex() >= 0);
        REQUIRE(combo->itemData(combo->currentIndex()).toString().toStdString() == "events_1");
        
        // Add another event series (should preserve selection)
        auto event_series_2 = createTestEventSeries("events_2");
        event_series_2->setTimeFrame(time_frame);
        data_manager->setData<DigitalEventSeries>("events_2", event_series_2, TimeKey("time"));
        QApplication::processEvents();
        
        // Selection should still be "events_1"
        REQUIRE(combo->currentIndex() >= 0);
        REQUIRE(combo->itemData(combo->currentIndex()).toString().toStdString() == "events_1");
        REQUIRE(combo->count() == 2);
    }
}

// ==================== Widget Destruction Tests ====================

TEST_CASE("ACFPropertiesWidget cleanup", "[ACFPropertiesWidget]")
{
    // Create QApplication if it doesn't exist (required for Qt widgets)
    int argc = 0;
    QApplication app(argc, nullptr);
    
    SECTION("observer callback removed on destruction") {
        auto data_manager = std::make_shared<DataManager>();
        auto state = std::make_shared<ACFState>();
        
        {
            // Create and set TimeFrame (remove existing if present)
            data_manager->removeTime(TimeKey("time"));
            auto time_frame = createTestTimeFrame();
            data_manager->setTime(TimeKey("time"), time_frame);
            
            ACFPropertiesWidget widget(state, data_manager);
            
            // Add some data to trigger observer
            auto event_series = createTestEventSeries("test_events");
            event_series->setTimeFrame(time_frame);
            data_manager->setData<DigitalEventSeries>("test_events", event_series, TimeKey("time"));
            QApplication::processEvents();
        }
        
        // Widget is destroyed, observer should be removed
        // Add more data - should not crash (observer was properly removed)
        data_manager->removeTime(TimeKey("time"));
        auto time_frame = createTestTimeFrame();
        data_manager->setTime(TimeKey("time"), time_frame);
        auto event_series_2 = createTestEventSeries("test_events_2");
        event_series_2->setTimeFrame(time_frame);
        data_manager->setData<DigitalEventSeries>("test_events_2", event_series_2, TimeKey("time"));
        QApplication::processEvents();
        
        // Test passes if no crash occurs
        REQUIRE(true);
    }
}
