/**
 * @file EventPlotPropertiesWidget.test.cpp
 * @brief Unit tests for EventPlotPropertiesWidget
 * 
 * Tests the properties widget for EventPlotWidget, including:
 * - Combo box population with DigitalEventSeries and DigitalIntervalSeries
 * - DataManager observer callback functionality
 * - Combo box refresh when data is added/removed
 */

#include "Plots/EventPlotWidget/UI/EventPlotPropertiesWidget.hpp"
#include "Plots/EventPlotWidget/Core/EventPlotState.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
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
std::shared_ptr<TimeFrame> createTestTimeFrame()
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

/**
 * @brief Create a test DigitalIntervalSeries with some intervals
 */
std::shared_ptr<DigitalIntervalSeries> createTestIntervalSeries(std::string const & key)
{
    std::vector<Interval> intervals = {
        Interval{100, 200},
        Interval{300, 400},
        Interval{500, 600}
    };
    return std::make_shared<DigitalIntervalSeries>(intervals);
}

// ==================== Combo Box Population Tests ====================

TEST_CASE("EventPlotPropertiesWidget combo box population", "[EventPlotPropertiesWidget]")
{
    // Create QApplication if it doesn't exist (required for Qt widgets)
    int argc = 0;
    QApplication app(argc, nullptr);
    
    SECTION("empty combo box when no data available") {
        auto data_manager = std::make_shared<DataManager>();
        auto state = std::make_shared<EventPlotState>();
        
        EventPlotPropertiesWidget widget(state, data_manager);
        
        auto * combo = widget.findChild<QComboBox *>("event_series_combo");
        REQUIRE(combo != nullptr);
        REQUIRE(combo->count() == 1);  // "No event data available" message
        REQUIRE_FALSE(combo->isEnabled());
    }
    
    SECTION("combo box populated with DigitalEventSeries keys") {
        auto data_manager = std::make_shared<DataManager>();
        auto state = std::make_shared<EventPlotState>();
        
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
        
        EventPlotPropertiesWidget widget(state, data_manager);
        
        // Process events again to ensure combo box is populated
        QApplication::processEvents();
        
        auto * combo = widget.findChild<QComboBox *>("event_series_combo");
        REQUIRE(combo != nullptr);
        REQUIRE(combo->isEnabled());
        REQUIRE(combo->count() == 2);
        REQUIRE(combo->itemText(0).toStdString() == "events_1");
        REQUIRE(combo->itemText(1).toStdString() == "events_2");
    }
    
    SECTION("combo box populated with DigitalIntervalSeries keys") {
        auto data_manager = std::make_shared<DataManager>();
        auto state = std::make_shared<EventPlotState>();
        
        // Create and set TimeFrame (remove existing if present)
        data_manager->removeTime(TimeKey("time"));
        auto time_frame = createTestTimeFrame();
        data_manager->setTime(TimeKey("time"), time_frame);
        
        // Add some interval series
        auto interval_series_1 = createTestIntervalSeries("intervals_1");
        auto interval_series_2 = createTestIntervalSeries("intervals_2");
        interval_series_1->setTimeFrame(time_frame);
        interval_series_2->setTimeFrame(time_frame);
        
        data_manager->setData<DigitalIntervalSeries>("intervals_1", interval_series_1, TimeKey("time"));
        data_manager->setData<DigitalIntervalSeries>("intervals_2", interval_series_2, TimeKey("time"));
        
        // Process events to allow observer callback to run
        QApplication::processEvents();
        
        EventPlotPropertiesWidget widget(state, data_manager);
        
        // Process events again to ensure combo box is populated
        QApplication::processEvents();
        
        auto * combo = widget.findChild<QComboBox *>("event_series_combo");
        REQUIRE(combo != nullptr);
        REQUIRE(combo->isEnabled());
        REQUIRE(combo->count() == 2);
        REQUIRE(combo->itemText(0).toStdString() == "intervals_1");
        REQUIRE(combo->itemText(1).toStdString() == "intervals_2");
    }
    
    SECTION("combo box populated with both event and interval series") {
        auto data_manager = std::make_shared<DataManager>();
        auto state = std::make_shared<EventPlotState>();
        
        // Create and set TimeFrame (remove existing if present)
        data_manager->removeTime(TimeKey("time"));
        auto time_frame = createTestTimeFrame();
        data_manager->setTime(TimeKey("time"), time_frame);
        
        // Add both types
        auto event_series = createTestEventSeries("events_1");
        auto interval_series = createTestIntervalSeries("intervals_1");
        event_series->setTimeFrame(time_frame);
        interval_series->setTimeFrame(time_frame);
        
        data_manager->setData<DigitalEventSeries>("events_1", event_series, TimeKey("time"));
        data_manager->setData<DigitalIntervalSeries>("intervals_1", interval_series, TimeKey("time"));
        
        // Process events to allow observer callback to run
        QApplication::processEvents();
        
        EventPlotPropertiesWidget widget(state, data_manager);
        
        // Process events again to ensure combo box is populated
        QApplication::processEvents();
        
        auto * combo = widget.findChild<QComboBox *>("event_series_combo");
        REQUIRE(combo != nullptr);
        REQUIRE(combo->isEnabled());
        REQUIRE(combo->count() == 2);
        // Order: events first, then intervals
        REQUIRE(combo->itemText(0).toStdString() == "events_1");
        REQUIRE(combo->itemText(1).toStdString() == "intervals_1");
    }
}

// ==================== Observer Callback Tests ====================

TEST_CASE("EventPlotPropertiesWidget observer callback", "[EventPlotPropertiesWidget]")
{
    // Create QApplication if it doesn't exist (required for Qt widgets)
    int argc = 0;
    QApplication app(argc, nullptr);
    
    SECTION("combo box refreshes when data is added") {
        auto data_manager = std::make_shared<DataManager>();
        auto state = std::make_shared<EventPlotState>();
        
        EventPlotPropertiesWidget widget(state, data_manager);
        
        auto * combo = widget.findChild<QComboBox *>("event_series_combo");
        REQUIRE(combo != nullptr);
        
        // Initially empty
        REQUIRE(combo->count() == 1);  // "No event data available"
        REQUIRE_FALSE(combo->isEnabled());
        
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
        auto state = std::make_shared<EventPlotState>();
        
        EventPlotPropertiesWidget widget(state, data_manager);
        
        auto * combo = widget.findChild<QComboBox *>("event_series_combo");
        REQUIRE(combo != nullptr);
        
        // Create and set TimeFrame (remove existing if present)
        data_manager->removeTime(TimeKey("time"));
        auto time_frame = createTestTimeFrame();
        data_manager->setTime(TimeKey("time"), time_frame);
        
        // Add multiple event series
        auto event_series_1 = createTestEventSeries("events_1");
        auto event_series_2 = createTestEventSeries("events_2");
        auto interval_series = createTestIntervalSeries("intervals_1");
        event_series_1->setTimeFrame(time_frame);
        event_series_2->setTimeFrame(time_frame);
        interval_series->setTimeFrame(time_frame);
        
        data_manager->setData<DigitalEventSeries>("events_1", event_series_1, TimeKey("time"));
        QApplication::processEvents();
        
        data_manager->setData<DigitalEventSeries>("events_2", event_series_2, TimeKey("time"));
        QApplication::processEvents();
        
        data_manager->setData<DigitalIntervalSeries>("intervals_1", interval_series, TimeKey("time"));
        QApplication::processEvents();
        
        // Combo box should contain all three
        REQUIRE(combo->isEnabled());
        REQUIRE(combo->count() == 3);
        
        // Verify all keys are present
        std::vector<std::string> items;
        for (int i = 0; i < combo->count(); ++i) {
            items.push_back(combo->itemText(i).toStdString());
        }
        REQUIRE(std::find(items.begin(), items.end(), "events_1") != items.end());
        REQUIRE(std::find(items.begin(), items.end(), "events_2") != items.end());
        REQUIRE(std::find(items.begin(), items.end(), "intervals_1") != items.end());
    }
}

// ==================== Widget Destruction Tests ====================

TEST_CASE("EventPlotPropertiesWidget cleanup", "[EventPlotPropertiesWidget]")
{
    // Create QApplication if it doesn't exist (required for Qt widgets)
    int argc = 0;
    QApplication app(argc, nullptr);
    
    SECTION("observer callback removed on destruction") {
        auto data_manager = std::make_shared<DataManager>();
        auto state = std::make_shared<EventPlotState>();
        
        {
            // Create and set TimeFrame (remove existing if present)
            data_manager->removeTime(TimeKey("time"));
            auto time_frame = createTestTimeFrame();
            data_manager->setTime(TimeKey("time"), time_frame);
            
            EventPlotPropertiesWidget widget(state, data_manager);
            
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
