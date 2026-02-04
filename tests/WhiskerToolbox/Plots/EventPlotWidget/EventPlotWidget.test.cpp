/**
 * @file EventPlotWidget.test.cpp
 * @brief Unit tests for EventPlotWidget
 * 
 * Tests the EventPlotWidget view component, including:
 * - Time position signal connection to EditorRegistry
 * - Signal emission updates registry's current time
 */

#include "Plots/EventPlotWidget/UI/EventPlotWidget.hpp"
#include "Plots/EventPlotWidget/EventPlotWidgetRegistration.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QApplication>
#include <QSignalSpy>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

// ==================== Helper Functions ====================

/**
 * @brief Create a test TimeFrame
 */
static std::shared_ptr<TimeFrame> createTestTimeFrame() {
    std::vector<int> times;
    times.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        times.push_back(i);
    }
    return std::make_shared<TimeFrame>(times);
}

// ==================== Time Position Signal Tests ====================

TEST_CASE("EventPlotWidget time position signal", "[EventPlotWidget]") {
    // Create QApplication if it doesn't exist (required for Qt widgets)
    int argc = 0;
    QApplication app(argc, nullptr);

    SECTION("timePositionSelected signal updates EditorRegistry current time") {
        auto data_manager = std::make_shared<DataManager>();
        auto registry = std::make_unique<EditorRegistry>(nullptr);

        // Register the EventPlotWidget type
        EventPlotWidgetModule::registerTypes(registry.get(), data_manager);

        // Get type info and use create_editor_custom to ensure signal connection is set up
        auto type_info = registry->typeInfo(EditorLib::EditorTypeId("EventPlotWidget"));
        REQUIRE_FALSE(type_info.type_id.isEmpty());
        auto instance = type_info.create_editor_custom(registry.get());
        REQUIRE(instance.view != nullptr);

        // Get the EventPlotWidget from the instance
        auto * event_plot_widget = qobject_cast<EventPlotWidget *>(instance.view);
        REQUIRE(event_plot_widget != nullptr);

        // Create a test TimeFrame and TimePosition
        auto time_frame = createTestTimeFrame();
        TimePosition test_position(TimeFrameIndex(500), time_frame);

        // Set up signal spy to monitor EditorRegistry timeChanged signal
        QSignalSpy time_changed_spy(registry.get(), &EditorRegistry::timeChanged);

        // Verify initial state - registry should have default position
        TimePosition initial_position = registry->currentPosition();
        REQUIRE(initial_position.index.getValue() == 0);

        // Emit the timePositionSelected signal from the widget
        emit event_plot_widget->timePositionSelected(test_position);

        // Process Qt events to allow signal/slot connections to execute
        QApplication::processEvents();

        // Verify that EditorRegistry's timeChanged signal was emitted
        REQUIRE(time_changed_spy.count() == 1);

        // Verify that the registry's current time was updated
        TimePosition updated_position = registry->currentPosition();
        REQUIRE(updated_position.index == test_position.index);
        REQUIRE(updated_position.sameClock(test_position));

        // Verify the signal was emitted with the correct position
        REQUIRE(time_changed_spy.at(0).at(0).value<TimePosition>().index == test_position.index);
    }

    SECTION("multiple time position selections update registry correctly") {
        auto data_manager = std::make_shared<DataManager>();
        auto registry = std::make_unique<EditorRegistry>(nullptr);

        // Register and create widget using create_editor_custom
        EventPlotWidgetModule::registerTypes(registry.get(), data_manager);
        auto type_info = registry->typeInfo(EditorLib::EditorTypeId("EventPlotWidget"));
        REQUIRE_FALSE(type_info.type_id.isEmpty());
        auto instance = type_info.create_editor_custom(registry.get());
        auto * event_plot_widget = qobject_cast<EventPlotWidget *>(instance.view);
        REQUIRE(event_plot_widget != nullptr);

        auto time_frame = createTestTimeFrame();
        QSignalSpy time_changed_spy(registry.get(), &EditorRegistry::timeChanged);

        // Emit multiple time positions
        TimePosition pos1(TimeFrameIndex(100), time_frame);
        TimePosition pos2(TimeFrameIndex(200), time_frame);
        TimePosition pos3(TimeFrameIndex(300), time_frame);

        emit event_plot_widget->timePositionSelected(pos1);
        QApplication::processEvents();

        emit event_plot_widget->timePositionSelected(pos2);
        QApplication::processEvents();

        emit event_plot_widget->timePositionSelected(pos3);
        QApplication::processEvents();

        // Verify all signals were emitted
        REQUIRE(time_changed_spy.count() == 3);

        // Verify final position
        TimePosition final_position = registry->currentPosition();
        REQUIRE(final_position.index == pos3.index);
    }

    SECTION("time position signal connection works through registration") {
        auto data_manager = std::make_shared<DataManager>();
        auto registry = std::make_unique<EditorRegistry>(nullptr);

        // Register the type
        EventPlotWidgetModule::registerTypes(registry.get(), data_manager);

        // Create editor using create_editor_custom (which sets up the connection)
        auto type_info = registry->typeInfo(EditorLib::EditorTypeId("EventPlotWidget"));
        REQUIRE_FALSE(type_info.type_id.isEmpty());

        // Use create_editor_custom to ensure the signal connection is set up
        auto instance = type_info.create_editor_custom(registry.get());
        REQUIRE(instance.view != nullptr);

        auto * event_plot_widget = qobject_cast<EventPlotWidget *>(instance.view);
        REQUIRE(event_plot_widget != nullptr);

        auto time_frame = createTestTimeFrame();
        TimePosition test_position(TimeFrameIndex(750), time_frame);

        QSignalSpy time_changed_spy(registry.get(), &EditorRegistry::timeChanged);

        // Emit signal - should trigger registry update through the connection
        emit event_plot_widget->timePositionSelected(test_position);
        QApplication::processEvents();

        // Verify connection worked
        REQUIRE(time_changed_spy.count() == 1);
        REQUIRE(registry->currentPosition().index == test_position.index);
    }
}
