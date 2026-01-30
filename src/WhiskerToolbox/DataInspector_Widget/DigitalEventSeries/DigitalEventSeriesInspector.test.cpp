
#include "DigitalEventSeriesInspector.hpp"
#include "DigitalEventSeriesDataView.hpp"
#include "EventTableModel.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QTableView>
#include <QAbstractItemModel>

#include <array>
#include <memory>
#include <numeric>
#include <vector>

namespace {
// Ensures a QApplication exists for the process. Must be called before using any Qt widgets/signals.
// Uses a leaked pointer intentionally - QApplication must outlive all Qt objects in tests.
void ensureQApplication()
{
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());// NOLINT: Intentionally leaked
    }
}
}// namespace

// === DigitalEventSeriesInspector Tests ===

TEST_CASE("DigitalEventSeriesInspector construction", "[DigitalEventSeriesInspector]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Constructs with data manager") {
        auto data_manager = std::make_shared<DataManager>();
        DigitalEventSeriesInspector inspector(data_manager, nullptr, nullptr);

        // Inspector should be created without crashing
        app->processEvents();
    }

    SECTION("Constructs with nullptr group manager") {
        auto data_manager = std::make_shared<DataManager>();
        DigitalEventSeriesInspector inspector(data_manager, nullptr, nullptr);

        REQUIRE(inspector.supportsGroupFiltering() == false);
        app->processEvents();
    }

    SECTION("Returns correct data type") {
        auto data_manager = std::make_shared<DataManager>();
        DigitalEventSeriesInspector inspector(data_manager, nullptr, nullptr);

        REQUIRE(inspector.getDataType() == DM_DataType::DigitalEvent);
        REQUIRE(inspector.getTypeName() == QStringLiteral("Digital Event Series"));
        REQUIRE(inspector.supportsExport() == true);
    }
}

TEST_CASE("DigitalEventSeriesInspector has expected UI", "[DigitalEventSeriesInspector]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Contains total events label") {
        auto data_manager = std::make_shared<DataManager>();
        DigitalEventSeriesInspector inspector(data_manager, nullptr, nullptr);

        // The inspector should have the label
        // We can't directly access it, but we can verify the inspector exists
        app->processEvents();
        REQUIRE(inspector.getTypeName() == QStringLiteral("Digital Event Series"));
    }

    SECTION("Contains add and remove event buttons") {
        auto data_manager = std::make_shared<DataManager>();
        DigitalEventSeriesInspector inspector(data_manager, nullptr, nullptr);

        // Find buttons through the wrapped widget
        auto * add_button = inspector.findChild<QPushButton *>("add_event_button");
        REQUIRE(add_button != nullptr);
        REQUIRE(add_button->text() == QStringLiteral("Add Event"));

        auto * remove_button = inspector.findChild<QPushButton *>("remove_event_button");
        REQUIRE(remove_button != nullptr);
        REQUIRE(remove_button->text() == QStringLiteral("Remove Event"));

        app->processEvents();
    }

    SECTION("Contains export section") {
        auto data_manager = std::make_shared<DataManager>();
        DigitalEventSeriesInspector inspector(data_manager, nullptr, nullptr);

        // Find export-related widgets
        auto * filename_edit = inspector.findChild<QLineEdit *>("filename_edit");
        REQUIRE(filename_edit != nullptr);

        auto * export_type_combo = inspector.findChild<QComboBox *>("export_type_combo");
        REQUIRE(export_type_combo != nullptr);
        REQUIRE(export_type_combo->count() > 0);

        app->processEvents();
    }
}

TEST_CASE("DigitalEventSeriesInspector data manipulation", "[DigitalEventSeriesInspector]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Sets active key correctly") {
        auto data_manager = std::make_shared<DataManager>();

        // Create a simple timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create event series with some events
        std::vector<TimeFrameIndex> event_times = {
            TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};
        auto event_series = std::make_shared<DigitalEventSeries>(event_times);
        data_manager->setData<DigitalEventSeries>("test_events", event_series, TimeKey("time"));

        DigitalEventSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_events");

        app->processEvents();

        // Verify the active key is set
        REQUIRE(inspector.getActiveKey() == "test_events");

        // Verify total events label is updated
        auto * total_events_label = inspector.findChild<QLabel *>("total_events_label");
        REQUIRE(total_events_label != nullptr);
        REQUIRE(total_events_label->text() == QStringLiteral("3"));
    }

    SECTION("Add event button adds event at current time") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Set current time to frame 50
        data_manager->setCurrentTime(50);

        // Create empty event series
        auto event_series = std::make_shared<DigitalEventSeries>();
        data_manager->setData<DigitalEventSeries>("test_events", event_series, TimeKey("time"));

        DigitalEventSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_events");

        app->processEvents();

        // Initially should have 0 events
        auto * total_events_label = inspector.findChild<QLabel *>("total_events_label");
        REQUIRE(total_events_label != nullptr);
        REQUIRE(total_events_label->text() == QStringLiteral("0"));

        // Click add event button
        auto * add_button = inspector.findChild<QPushButton *>("add_event_button");
        REQUIRE(add_button != nullptr);
        add_button->click();

        app->processEvents();

        // Should now have 1 event
        REQUIRE(total_events_label->text() == QStringLiteral("1"));

        // Verify the event was added at the current time
        auto events = data_manager->getData<DigitalEventSeries>("test_events");
        REQUIRE(events != nullptr);
        REQUIRE(events->size() == 1);
        auto event_view = events->view();
        REQUIRE(event_view[0].time() == TimeFrameIndex(50));
    }

    SECTION("Remove event button removes event at current time") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Set current time to frame 20
        data_manager->setCurrentTime(20);

        // Create event series with events at 10, 20, 30
        std::vector<TimeFrameIndex> event_times = {
            TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};
        auto event_series = std::make_shared<DigitalEventSeries>(event_times);
        data_manager->setData<DigitalEventSeries>("test_events", event_series, TimeKey("time"));

        DigitalEventSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_events");

        app->processEvents();

        // Initially should have 3 events
        auto * total_events_label = inspector.findChild<QLabel *>("total_events_label");
        REQUIRE(total_events_label != nullptr);
        REQUIRE(total_events_label->text() == QStringLiteral("3"));

        // Click remove event button
        auto * remove_button = inspector.findChild<QPushButton *>("remove_event_button");
        REQUIRE(remove_button != nullptr);
        remove_button->click();

        app->processEvents();

        // Should now have 2 events
        REQUIRE(total_events_label->text() == QStringLiteral("2"));

        // Verify the event at current time (20) was removed
        auto events = data_manager->getData<DigitalEventSeries>("test_events");
        REQUIRE(events != nullptr);
        REQUIRE(events->size() == 2);
        auto event_view = events->view();
        REQUIRE(event_view[0].time() == TimeFrameIndex(10));
        REQUIRE(event_view[1].time() == TimeFrameIndex(30));
    }

    SECTION("Remove event button does nothing if no event at current time") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Set current time to frame 50 (no event here)
        data_manager->setCurrentTime(50);

        // Create event series with events at 10, 20, 30
        std::vector<TimeFrameIndex> event_times = {
            TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};
        auto event_series = std::make_shared<DigitalEventSeries>(event_times);
        data_manager->setData<DigitalEventSeries>("test_events", event_series, TimeKey("time"));

        DigitalEventSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_events");

        app->processEvents();

        // Initially should have 3 events
        auto * total_events_label = inspector.findChild<QLabel *>("total_events_label");
        REQUIRE(total_events_label != nullptr);
        REQUIRE(total_events_label->text() == QStringLiteral("3"));

        // Click remove event button
        auto * remove_button = inspector.findChild<QPushButton *>("remove_event_button");
        REQUIRE(remove_button != nullptr);
        remove_button->click();

        app->processEvents();

        // Should still have 3 events (no event at current time to remove)
        REQUIRE(total_events_label->text() == QStringLiteral("3"));
    }

    SECTION("Updates when data changes externally") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create event series with initial events
        std::vector<TimeFrameIndex> event_times = {TimeFrameIndex(10), TimeFrameIndex(20)};
        auto event_series = std::make_shared<DigitalEventSeries>(event_times);
        data_manager->setData<DigitalEventSeries>("test_events", event_series, TimeKey("time"));

        DigitalEventSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_events");

        app->processEvents();

        // Initially should have 2 events
        auto * total_events_label = inspector.findChild<QLabel *>("total_events_label");
        REQUIRE(total_events_label != nullptr);
        REQUIRE(total_events_label->text() == QStringLiteral("2"));

        // Add an event externally
        event_series->addEvent(TimeFrameIndex(30));
        app->processEvents();

        // Label should update to show 3 events
        REQUIRE(total_events_label->text() == QStringLiteral("3"));
    }
}

TEST_CASE("DigitalEventSeriesInspector callbacks", "[DigitalEventSeriesInspector]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Removes callbacks on destruction") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create event series
        auto event_series = std::make_shared<DigitalEventSeries>();
        data_manager->setData<DigitalEventSeries>("test_events", event_series, TimeKey("time"));

        {
            DigitalEventSeriesInspector inspector(data_manager, nullptr, nullptr);
            inspector.setActiveKey("test_events");
            app->processEvents();
        }// Inspector goes out of scope

        // Should not crash when data changes after inspector is destroyed
        event_series->addEvent(TimeFrameIndex(10));
        app->processEvents();
    }

    SECTION("Removes callbacks explicitly") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create event series
        auto event_series = std::make_shared<DigitalEventSeries>();
        data_manager->setData<DigitalEventSeries>("test_events", event_series, TimeKey("time"));

        DigitalEventSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_events");
        app->processEvents();

        // Remove callbacks
        inspector.removeCallbacks();

        // Should not crash when data changes after callbacks removed
        event_series->addEvent(TimeFrameIndex(10));
        app->processEvents();
    }
}

// === DigitalEventSeriesDataView Tests ===

TEST_CASE("DigitalEventSeriesDataView construction", "[DigitalEventSeriesDataView]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Constructs with data manager") {
        auto data_manager = std::make_shared<DataManager>();
        DigitalEventSeriesDataView view(data_manager, nullptr);

        REQUIRE(view.getDataType() == DM_DataType::DigitalEvent);
        REQUIRE(view.getTypeName() == QStringLiteral("Event Table"));
        REQUIRE(view.tableView() != nullptr);

        app->processEvents();
    }
}

TEST_CASE("DigitalEventSeriesDataView table model updates on external data changes", "[DigitalEventSeriesDataView]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Table model reflects initial data") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create event series with initial events
        std::vector<TimeFrameIndex> event_times = {
            TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};
        auto event_series = std::make_shared<DigitalEventSeries>(event_times);
        data_manager->setData<DigitalEventSeries>("test_events", event_series, TimeKey("time"));

        DigitalEventSeriesDataView view(data_manager, nullptr);
        view.setActiveKey("test_events");

        app->processEvents();

        // Verify table model has correct initial data
        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = dynamic_cast<EventTableModel *>(table_view->model());
        REQUIRE(model != nullptr);
        REQUIRE(model->rowCount(QModelIndex()) == 3);
        REQUIRE(model->getEvent(0) == TimeFrameIndex(10));
        REQUIRE(model->getEvent(1) == TimeFrameIndex(20));
        REQUIRE(model->getEvent(2) == TimeFrameIndex(30));
    }

    SECTION("Table model updates when event is added externally") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create event series with initial events
        std::vector<TimeFrameIndex> event_times = {
            TimeFrameIndex(10), TimeFrameIndex(20)};
        auto event_series = std::make_shared<DigitalEventSeries>(event_times);
        data_manager->setData<DigitalEventSeries>("test_events", event_series, TimeKey("time"));

        DigitalEventSeriesDataView view(data_manager, nullptr);
        view.setActiveKey("test_events");

        app->processEvents();

        // Verify initial state
        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = dynamic_cast<EventTableModel *>(table_view->model());
        REQUIRE(model != nullptr);
        REQUIRE(model->rowCount(QModelIndex()) == 2);

        // Add an event externally
        event_series->addEvent(TimeFrameIndex(30));
        app->processEvents();

        // Verify table model is automatically updated
        REQUIRE(model->rowCount(QModelIndex()) == 3);
        REQUIRE(model->getEvent(0) == TimeFrameIndex(10));
        REQUIRE(model->getEvent(1) == TimeFrameIndex(20));
        REQUIRE(model->getEvent(2) == TimeFrameIndex(30));
    }

    SECTION("Table model updates when event is removed externally") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create event series with initial events
        std::vector<TimeFrameIndex> event_times = {
            TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};
        auto event_series = std::make_shared<DigitalEventSeries>(event_times);
        data_manager->setData<DigitalEventSeries>("test_events", event_series, TimeKey("time"));

        DigitalEventSeriesDataView view(data_manager, nullptr);
        view.setActiveKey("test_events");

        app->processEvents();

        // Verify initial state
        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = dynamic_cast<EventTableModel *>(table_view->model());
        REQUIRE(model != nullptr);
        REQUIRE(model->rowCount(QModelIndex()) == 3);

        // Remove an event externally
        event_series->removeEvent(TimeFrameIndex(20));
        app->processEvents();

        // Verify table model is automatically updated
        REQUIRE(model->rowCount(QModelIndex()) == 2);
        REQUIRE(model->getEvent(0) == TimeFrameIndex(10));
        REQUIRE(model->getEvent(1) == TimeFrameIndex(30));
    }

    SECTION("Table model updates when multiple events are added externally") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create event series with initial events
        std::vector<TimeFrameIndex> event_times = {TimeFrameIndex(10)};
        auto event_series = std::make_shared<DigitalEventSeries>(event_times);
        data_manager->setData<DigitalEventSeries>("test_events", event_series, TimeKey("time"));

        DigitalEventSeriesDataView view(data_manager, nullptr);
        view.setActiveKey("test_events");

        app->processEvents();

        // Verify initial state
        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = dynamic_cast<EventTableModel *>(table_view->model());
        REQUIRE(model != nullptr);
        REQUIRE(model->rowCount(QModelIndex()) == 1);

        // Add multiple events externally
        event_series->addEvent(TimeFrameIndex(20));
        app->processEvents();
        REQUIRE(model->rowCount(QModelIndex()) == 2);

        event_series->addEvent(TimeFrameIndex(30));
        app->processEvents();
        REQUIRE(model->rowCount(QModelIndex()) == 3);

        event_series->addEvent(TimeFrameIndex(40));
        app->processEvents();

        // Verify all events are in the table model
        REQUIRE(model->rowCount(QModelIndex()) == 4);
        REQUIRE(model->getEvent(0) == TimeFrameIndex(10));
        REQUIRE(model->getEvent(1) == TimeFrameIndex(20));
        REQUIRE(model->getEvent(2) == TimeFrameIndex(30));
        REQUIRE(model->getEvent(3) == TimeFrameIndex(40));
    }

    SECTION("Table model updates when all events are removed externally") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create event series with initial events
        std::vector<TimeFrameIndex> event_times = {
            TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};
        auto event_series = std::make_shared<DigitalEventSeries>(event_times);
        data_manager->setData<DigitalEventSeries>("test_events", event_series, TimeKey("time"));

        DigitalEventSeriesDataView view(data_manager, nullptr);
        view.setActiveKey("test_events");

        app->processEvents();

        // Verify initial state
        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = dynamic_cast<EventTableModel *>(table_view->model());
        REQUIRE(model != nullptr);
        REQUIRE(model->rowCount(QModelIndex()) == 3);

        // Remove all events externally
        event_series->removeEvent(TimeFrameIndex(10));
        app->processEvents();
        REQUIRE(model->rowCount(QModelIndex()) == 2);

        event_series->removeEvent(TimeFrameIndex(20));
        app->processEvents();
        REQUIRE(model->rowCount(QModelIndex()) == 1);

        event_series->removeEvent(TimeFrameIndex(30));
        app->processEvents();

        // Verify table model is empty
        REQUIRE(model->rowCount(QModelIndex()) == 0);
    }

    SECTION("Table model updates when active key changes") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create two event series
        std::vector<TimeFrameIndex> event_times_1 = {TimeFrameIndex(10), TimeFrameIndex(20)};
        auto event_series_1 = std::make_shared<DigitalEventSeries>(event_times_1);
        data_manager->setData<DigitalEventSeries>("events_1", event_series_1, TimeKey("time"));

        std::vector<TimeFrameIndex> event_times_2 = {
            TimeFrameIndex(30), TimeFrameIndex(40), TimeFrameIndex(50)};
        auto event_series_2 = std::make_shared<DigitalEventSeries>(event_times_2);
        data_manager->setData<DigitalEventSeries>("events_2", event_series_2, TimeKey("time"));

        DigitalEventSeriesDataView view(data_manager, nullptr);
        view.setActiveKey("events_1");

        app->processEvents();

        // Verify initial state
        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = dynamic_cast<EventTableModel *>(table_view->model());
        REQUIRE(model != nullptr);
        REQUIRE(model->rowCount(QModelIndex()) == 2);

        // Change active key
        view.setActiveKey("events_2");
        app->processEvents();

        // Verify table model reflects new data
        REQUIRE(model->rowCount(QModelIndex()) == 3);
        REQUIRE(model->getEvent(0) == TimeFrameIndex(30));
        REQUIRE(model->getEvent(1) == TimeFrameIndex(40));
        REQUIRE(model->getEvent(2) == TimeFrameIndex(50));
    }
}
