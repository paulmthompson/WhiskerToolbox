/**
 * @file EventPlotWidget.test.cpp
 * @brief Unit tests for EventPlotWidget
 * 
 * Tests the EventPlotWidget view component, including:
 * - Time position signal connection to EditorRegistry
 * - Signal emission updates registry's current time
 * - EventPlotOpenGLWidget signal emission (selection, double-click)
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

// ==================== EventPlotWidget Signal Tests ====================
// Note: Full OpenGL click-to-selection testing requires OpenGL context and scene setup.
// The selection and double-click functionality is tested via:
// 1. SceneHitTester tests (in CorePlotting) - verify hit testing logic
// 2. SceneBuilder tests (in CorePlotting) - verify entity_to_series_key mapping
// 3. EventPlotWidget signal forwarding tests (below) - verify signal propagation from OpenGL widget

TEST_CASE("EventPlotWidget timePositionSelected signal", "[EventPlotWidget][Signals]") {
    // Create QApplication if it doesn't exist
    int argc = 0;
    QApplication app(argc, nullptr);
    
    auto data_manager = std::make_shared<DataManager>();
    
    SECTION("timePositionSelected signal can be connected and emitted") {
        // Create EventPlotWidget (which contains EventPlotOpenGLWidget internally)
        EventPlotWidget event_plot_widget(data_manager, nullptr);
        
        // Set up signal spy for timePositionSelected on the parent widget
        QSignalSpy time_position_spy(&event_plot_widget, &EventPlotWidget::timePositionSelected);
        REQUIRE(time_position_spy.isValid());
        
        // Create a test TimePosition
        auto time_frame = createTestTimeFrame();
        TimePosition test_pos(TimeFrameIndex(500), time_frame);
        
        // Emit the signal directly (simulating what the internal OpenGL widget would do)
        emit event_plot_widget.timePositionSelected(test_pos);
        QApplication::processEvents();
        
        REQUIRE(time_position_spy.count() == 1);
        
        // Verify the position
        TimePosition received_pos = time_position_spy.at(0).at(0).value<TimePosition>();
        REQUIRE(received_pos.index == TimeFrameIndex(500));
    }
    
    SECTION("multiple timePositionSelected emissions are tracked") {
        EventPlotWidget event_plot_widget(data_manager, nullptr);
        
        QSignalSpy time_position_spy(&event_plot_widget, &EventPlotWidget::timePositionSelected);
        
        auto time_frame = createTestTimeFrame();
        
        // Emit multiple time positions
        emit event_plot_widget.timePositionSelected(TimePosition(TimeFrameIndex(100), time_frame));
        emit event_plot_widget.timePositionSelected(TimePosition(TimeFrameIndex(200), time_frame));
        emit event_plot_widget.timePositionSelected(TimePosition(TimeFrameIndex(300), time_frame));
        QApplication::processEvents();
        
        REQUIRE(time_position_spy.count() == 3);
        
        // Verify last emission
        TimePosition last_pos = time_position_spy.at(2).at(0).value<TimePosition>();
        REQUIRE(last_pos.index == TimeFrameIndex(300));
    }
}

// ==================== Event Selection Signal Tests ====================

TEST_CASE("EventPlotWidget eventSelected signal", "[EventPlotWidget][Signals][ClickSelection]") {
    // Create QApplication if it doesn't exist
    int argc = 0;
    QApplication app(argc, nullptr);
    
    auto data_manager = std::make_shared<DataManager>();
    
    SECTION("eventSelected signal is forwarded from internal OpenGL widget") {
        // Create EventPlotWidget
        EventPlotWidget event_plot_widget(data_manager, nullptr);
        
        // Find the internal OpenGL widget via Qt's meta-object system
        // The internal widget is named "EventPlotOpenGLWidget" by Qt's default naming
        QObject * opengl_widget = nullptr;
        auto children = event_plot_widget.findChildren<QWidget *>();
        for (auto * child : children) {
            if (QString(child->metaObject()->className()).contains("EventPlotOpenGLWidget")) {
                opengl_widget = child;
                break;
            }
        }
        REQUIRE(opengl_widget != nullptr);
        
        // Set up signal spy on the parent widget's eventSelected signal
        QSignalSpy event_selected_spy(&event_plot_widget, &EventPlotWidget::eventSelected);
        REQUIRE(event_selected_spy.isValid());
        
        // Emit eventSelected from the internal OpenGL widget via Qt's signal system
        int const test_trial_index = 5;
        float const test_relative_time = 123.45f;
        QString const test_series_key = "test_spikes";
        
        // Use QMetaObject::invokeMethod to emit the signal
        bool invoke_result = QMetaObject::invokeMethod(
            opengl_widget,
            "eventSelected",
            Qt::DirectConnection,
            Q_ARG(int, test_trial_index),
            Q_ARG(float, test_relative_time),
            Q_ARG(QString const &, test_series_key)
        );
        REQUIRE(invoke_result);
        QApplication::processEvents();
        
        // Verify the signal was forwarded
        REQUIRE(event_selected_spy.count() == 1);
        
        // Verify the signal parameters were forwarded correctly
        REQUIRE(event_selected_spy.at(0).at(0).toInt() == test_trial_index);
        REQUIRE(event_selected_spy.at(0).at(1).toFloat() == test_relative_time);
        REQUIRE(event_selected_spy.at(0).at(2).toString() == test_series_key);
    }
    
    SECTION("multiple event selections are tracked") {
        EventPlotWidget event_plot_widget(data_manager, nullptr);
        
        // Find the internal OpenGL widget
        QObject * opengl_widget = nullptr;
        auto children = event_plot_widget.findChildren<QWidget *>();
        for (auto * child : children) {
            if (QString(child->metaObject()->className()).contains("EventPlotOpenGLWidget")) {
                opengl_widget = child;
                break;
            }
        }
        REQUIRE(opengl_widget != nullptr);
        
        QSignalSpy event_selected_spy(&event_plot_widget, &EventPlotWidget::eventSelected);
        
        // Emit multiple event selections via meta-object invoke
        QString key1 = "spikes_1";
        QString key2 = "spikes_2";
        QMetaObject::invokeMethod(opengl_widget, "eventSelected", Qt::DirectConnection,
                                  Q_ARG(int, 0), Q_ARG(float, -50.0f), Q_ARG(QString const &, key1));
        QMetaObject::invokeMethod(opengl_widget, "eventSelected", Qt::DirectConnection,
                                  Q_ARG(int, 3), Q_ARG(float, 0.0f), Q_ARG(QString const &, key1));
        QMetaObject::invokeMethod(opengl_widget, "eventSelected", Qt::DirectConnection,
                                  Q_ARG(int, 7), Q_ARG(float, 150.0f), Q_ARG(QString const &, key2));
        QApplication::processEvents();
        
        // Verify all signals were forwarded
        REQUIRE(event_selected_spy.count() == 3);
        
        // Verify last selection
        REQUIRE(event_selected_spy.at(2).at(0).toInt() == 7);
        REQUIRE(event_selected_spy.at(2).at(1).toFloat() == 150.0f);
        REQUIRE(event_selected_spy.at(2).at(2).toString() == "spikes_2");
    }
    
    SECTION("eventSelected and timePositionSelected signals are independent") {
        EventPlotWidget event_plot_widget(data_manager, nullptr);
        
        // Find the internal OpenGL widget
        QObject * opengl_widget = nullptr;
        auto children = event_plot_widget.findChildren<QWidget *>();
        for (auto * child : children) {
            if (QString(child->metaObject()->className()).contains("EventPlotOpenGLWidget")) {
                opengl_widget = child;
                break;
            }
        }
        REQUIRE(opengl_widget != nullptr);
        
        QSignalSpy event_selected_spy(&event_plot_widget, &EventPlotWidget::eventSelected);
        QSignalSpy time_position_spy(&event_plot_widget, &EventPlotWidget::timePositionSelected);
        
        auto time_frame = createTestTimeFrame();
        
        // Emit eventSelected (single click)
        QString events_key = "events";
        QMetaObject::invokeMethod(opengl_widget, "eventSelected", Qt::DirectConnection,
                                  Q_ARG(int, 2), Q_ARG(float, 100.0f), Q_ARG(QString const &, events_key));
        
        // Emit timePositionSelected (double click navigates to time)
        emit event_plot_widget.timePositionSelected(TimePosition(TimeFrameIndex(500), time_frame));
        
        QApplication::processEvents();
        
        // Verify signals are independent
        REQUIRE(event_selected_spy.count() == 1);
        REQUIRE(time_position_spy.count() == 1);
        
        // eventSelected should have trial info
        REQUIRE(event_selected_spy.at(0).at(0).toInt() == 2);
        
        // timePositionSelected should have TimePosition
        TimePosition received_pos = time_position_spy.at(0).at(0).value<TimePosition>();
        REQUIRE(received_pos.index == TimeFrameIndex(500));
    }
}
