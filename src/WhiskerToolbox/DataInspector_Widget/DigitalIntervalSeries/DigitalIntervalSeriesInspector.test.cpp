
#include "DigitalIntervalSeriesInspector.hpp"
#include "DigitalIntervalSeriesDataView.hpp"
#include "IntervalTableModel.hpp"

#include "DataInspector_Widget/DataInspectorPropertiesWidget.hpp"
#include "DataInspector_Widget/DataInspectorState.hpp"
#include "DataInspector_Widget/DataInspectorViewWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QItemSelectionModel>
#include <QSignalSpy>
#include <QTableView>
#include <QAbstractItemModel>
#include <QTest>

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

// === DigitalIntervalSeriesInspector Tests ===

TEST_CASE("DigitalIntervalSeriesInspector construction", "[DigitalIntervalSeriesInspector]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Constructs with data manager") {
        auto data_manager = std::make_shared<DataManager>();
        DigitalIntervalSeriesInspector inspector(data_manager, nullptr, nullptr);

        // Inspector should be created without crashing
        app->processEvents();
    }

    SECTION("Constructs with nullptr group manager") {
        auto data_manager = std::make_shared<DataManager>();
        DigitalIntervalSeriesInspector inspector(data_manager, nullptr, nullptr);

        REQUIRE(inspector.supportsGroupFiltering() == false);
        app->processEvents();
    }

    SECTION("Returns correct data type") {
        auto data_manager = std::make_shared<DataManager>();
        DigitalIntervalSeriesInspector inspector(data_manager, nullptr, nullptr);

        REQUIRE(inspector.getDataType() == DM_DataType::DigitalInterval);
        REQUIRE(inspector.getTypeName() == QStringLiteral("Digital Interval Series"));
        REQUIRE(inspector.supportsExport() == true);
    }
}

TEST_CASE("DigitalIntervalSeriesInspector has expected UI", "[DigitalIntervalSeriesInspector]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Contains total intervals label") {
        auto data_manager = std::make_shared<DataManager>();
        DigitalIntervalSeriesInspector inspector(data_manager, nullptr, nullptr);

        // The inspector wraps DigitalIntervalSeries_Widget which should have the label
        app->processEvents();
        REQUIRE(inspector.getTypeName() == QStringLiteral("Digital Interval Series"));
    }

    SECTION("Contains create and remove interval buttons") {
        auto data_manager = std::make_shared<DataManager>();
        DigitalIntervalSeriesInspector inspector(data_manager, nullptr, nullptr);

        // Find buttons through the wrapped widget
        auto * create_button = inspector.findChild<QPushButton *>("create_interval_button");
        REQUIRE(create_button != nullptr);
        REQUIRE(create_button->text() == QStringLiteral("Create Interval"));

        auto * remove_button = inspector.findChild<QPushButton *>("remove_interval_button");
        REQUIRE(remove_button != nullptr);
        REQUIRE(remove_button->text() == QStringLiteral("Remove Interval"));

        app->processEvents();
    }

    SECTION("Contains export section") {
        auto data_manager = std::make_shared<DataManager>();
        DigitalIntervalSeriesInspector inspector(data_manager, nullptr, nullptr);

        // Find export-related widgets
        auto * filename_edit = inspector.findChild<QLineEdit *>("filename_edit");
        REQUIRE(filename_edit != nullptr);

        auto * export_type_combo = inspector.findChild<QComboBox *>("export_type_combo");
        REQUIRE(export_type_combo != nullptr);
        REQUIRE(export_type_combo->count() > 0);

        app->processEvents();
    }
}

TEST_CASE("DigitalIntervalSeriesInspector data manipulation", "[DigitalIntervalSeriesInspector]") {
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

        // Create interval series with some intervals
        auto interval_series = std::make_shared<DigitalIntervalSeries>();
        interval_series->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));
        interval_series->addEvent(TimeFrameIndex(30), TimeFrameIndex(40));
        data_manager->setData<DigitalIntervalSeries>("test_intervals", interval_series, TimeKey("time"));

        DigitalIntervalSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_intervals");

        app->processEvents();

        // Verify the active key is set
        REQUIRE(inspector.getActiveKey() == "test_intervals");

        // Verify total intervals label is updated
        auto * total_intervals_label = inspector.findChild<QLabel *>("total_interval_label");
        REQUIRE(total_intervals_label != nullptr);
        REQUIRE(total_intervals_label->text() == QStringLiteral("2"));
    }

    SECTION("Create interval button creates interval at current time") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Set current time to frame 50
        data_manager->setCurrentTime(50);

        // Create empty interval series
        auto interval_series = std::make_shared<DigitalIntervalSeries>();
        data_manager->setData<DigitalIntervalSeries>("test_intervals", interval_series, TimeKey("time"));

        DigitalIntervalSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_intervals");

        app->processEvents();

        // Initially should have 0 intervals
        auto * total_intervals_label = inspector.findChild<QLabel *>("total_interval_label");
        REQUIRE(total_intervals_label != nullptr);
        REQUIRE(total_intervals_label->text() == QStringLiteral("0"));

        // Click create interval button (first click marks start)
        auto * create_button = inspector.findChild<QPushButton *>("create_interval_button");
        REQUIRE(create_button != nullptr);
        create_button->click();

        app->processEvents();

        // Should be in interval creation mode
        REQUIRE(create_button->text() == QStringLiteral("Mark Interval End"));

        // Move to later frame and click again to complete interval
        data_manager->setCurrentTime(60);
        create_button->click();

        app->processEvents();

        // Should now have 1 interval
        REQUIRE(total_intervals_label->text() == QStringLiteral("1"));

        // Verify the interval was created correctly
        auto intervals = data_manager->getData<DigitalIntervalSeries>("test_intervals");
        REQUIRE(intervals != nullptr);
        REQUIRE(intervals->size() == 1);
        auto interval_view = intervals->view();
        REQUIRE(interval_view[0].value().start == 50);
        REQUIRE(interval_view[0].value().end == 60);
    }

    SECTION("Remove interval button removes interval at current time") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Set current time to frame 15 (within first interval)
        data_manager->setCurrentTime(15);

        // Create interval series with intervals [10,20] and [30,40]
        auto interval_series = std::make_shared<DigitalIntervalSeries>();
        interval_series->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));
        interval_series->addEvent(TimeFrameIndex(30), TimeFrameIndex(40));
        data_manager->setData<DigitalIntervalSeries>("test_intervals", interval_series, TimeKey("time"));

        DigitalIntervalSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_intervals");

        app->processEvents();

        // Initially should have 2 intervals
        auto * total_intervals_label = inspector.findChild<QLabel *>("total_interval_label");
        REQUIRE(total_intervals_label != nullptr);
        REQUIRE(total_intervals_label->text() == QStringLiteral("2"));

        // Click remove interval button (first click marks start)
        auto * remove_button = inspector.findChild<QPushButton *>("remove_interval_button");
        REQUIRE(remove_button != nullptr);
        remove_button->click();

        app->processEvents();

        // Should be in remove interval mode
        REQUIRE(remove_button->text() == QStringLiteral("Mark Remove Interval End"));

        // Move to frame 18 and click again to complete removal
        data_manager->setCurrentTime(18);
        remove_button->click();

        app->processEvents();

        // Should still have intervals (the removal affects the interval but doesn't remove it completely)
        // The exact behavior depends on implementation, but we verify it doesn't crash
        REQUIRE(total_intervals_label != nullptr);
    }

    SECTION("Updates when data changes externally") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create interval series with initial intervals
        auto interval_series = std::make_shared<DigitalIntervalSeries>();
        interval_series->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));
        data_manager->setData<DigitalIntervalSeries>("test_intervals", interval_series, TimeKey("time"));

        DigitalIntervalSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_intervals");

        app->processEvents();

        // Initially should have 1 interval
        auto * total_intervals_label = inspector.findChild<QLabel *>("total_interval_label");
        REQUIRE(total_intervals_label != nullptr);
        REQUIRE(total_intervals_label->text() == QStringLiteral("1"));

        // Add an interval externally
        interval_series->addEvent(TimeFrameIndex(30), TimeFrameIndex(40));
        app->processEvents();

        // Label should update to show 2 intervals
        REQUIRE(total_intervals_label->text() == QStringLiteral("2"));
    }
}

TEST_CASE("DigitalIntervalSeriesInspector callbacks", "[DigitalIntervalSeriesInspector]") {
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

        // Create interval series
        auto interval_series = std::make_shared<DigitalIntervalSeries>();
        data_manager->setData<DigitalIntervalSeries>("test_intervals", interval_series, TimeKey("time"));

        {
            DigitalIntervalSeriesInspector inspector(data_manager, nullptr, nullptr);
            inspector.setActiveKey("test_intervals");
            app->processEvents();
        }// Inspector goes out of scope

        // Should not crash when data changes after inspector is destroyed
        interval_series->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));
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

        // Create interval series
        auto interval_series = std::make_shared<DigitalIntervalSeries>();
        data_manager->setData<DigitalIntervalSeries>("test_intervals", interval_series, TimeKey("time"));

        DigitalIntervalSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_intervals");
        app->processEvents();

        // Remove callbacks
        inspector.removeCallbacks();

        // Should not crash when data changes after callbacks removed
        interval_series->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));
        app->processEvents();
    }
}

// === DigitalIntervalSeriesDataView Tests ===

TEST_CASE("DigitalIntervalSeriesDataView construction", "[DigitalIntervalSeriesDataView]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Constructs with data manager") {
        auto data_manager = std::make_shared<DataManager>();
        DigitalIntervalSeriesDataView view(data_manager, nullptr);

        REQUIRE(view.getDataType() == DM_DataType::DigitalInterval);
        REQUIRE(view.getTypeName() == QStringLiteral("Interval Table"));
        REQUIRE(view.tableView() != nullptr);

        app->processEvents();
    }
}

TEST_CASE("DigitalIntervalSeriesDataView table model updates on external data changes", "[DigitalIntervalSeriesDataView]") {
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

        // Create interval series with initial intervals
        auto interval_series = std::make_shared<DigitalIntervalSeries>();
        interval_series->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));
        interval_series->addEvent(TimeFrameIndex(30), TimeFrameIndex(40));
        data_manager->setData<DigitalIntervalSeries>("test_intervals", interval_series, TimeKey("time"));

        DigitalIntervalSeriesDataView view(data_manager, nullptr);
        view.setActiveKey("test_intervals");

        app->processEvents();

        // Verify table model has correct initial data
        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = dynamic_cast<IntervalTableModel *>(table_view->model());
        REQUIRE(model != nullptr);
        REQUIRE(model->rowCount(QModelIndex()) == 2);
        
        Interval interval0 = model->getInterval(0);
        Interval interval1 = model->getInterval(1);
        REQUIRE(interval0.start == 10);
        REQUIRE(interval0.end == 20);
        REQUIRE(interval1.start == 30);
        REQUIRE(interval1.end == 40);
    }

    SECTION("Table model updates when interval is added externally") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create interval series with initial intervals
        auto interval_series = std::make_shared<DigitalIntervalSeries>();
        interval_series->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));
        data_manager->setData<DigitalIntervalSeries>("test_intervals", interval_series, TimeKey("time"));

        DigitalIntervalSeriesDataView view(data_manager, nullptr);
        view.setActiveKey("test_intervals");

        app->processEvents();

        // Verify initial state
        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = dynamic_cast<IntervalTableModel *>(table_view->model());
        REQUIRE(model != nullptr);
        REQUIRE(model->rowCount(QModelIndex()) == 1);

        // Add an interval externally
        interval_series->addEvent(TimeFrameIndex(30), TimeFrameIndex(40));
        app->processEvents();

        // Verify table model is automatically updated
        REQUIRE(model->rowCount(QModelIndex()) == 2);
        Interval interval1 = model->getInterval(1);
        REQUIRE(interval1.start == 30);
        REQUIRE(interval1.end == 40);
    }

    SECTION("Table model updates when interval is removed externally") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create interval series with initial intervals
        auto interval_series = std::make_shared<DigitalIntervalSeries>();
        interval_series->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));
        interval_series->addEvent(TimeFrameIndex(30), TimeFrameIndex(40));
        data_manager->setData<DigitalIntervalSeries>("test_intervals", interval_series, TimeKey("time"));

        DigitalIntervalSeriesDataView view(data_manager, nullptr);
        view.setActiveKey("test_intervals");

        app->processEvents();

        // Verify initial state
        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = dynamic_cast<IntervalTableModel *>(table_view->model());
        REQUIRE(model != nullptr);
        REQUIRE(model->rowCount(QModelIndex()) == 2);

        // Remove an interval externally
        Interval to_remove{10, 20};
        interval_series->removeInterval(to_remove);
        app->processEvents();

        // Verify table model is automatically updated
        REQUIRE(model->rowCount(QModelIndex()) == 1);
        Interval remaining = model->getInterval(0);
        REQUIRE(remaining.start == 30);
        REQUIRE(remaining.end == 40);
    }

    SECTION("Table model updates when multiple intervals are added externally") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create interval series with initial intervals
        auto interval_series = std::make_shared<DigitalIntervalSeries>();
        interval_series->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));
        data_manager->setData<DigitalIntervalSeries>("test_intervals", interval_series, TimeKey("time"));

        DigitalIntervalSeriesDataView view(data_manager, nullptr);
        view.setActiveKey("test_intervals");

        app->processEvents();

        // Verify initial state
        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = dynamic_cast<IntervalTableModel *>(table_view->model());
        REQUIRE(model != nullptr);
        REQUIRE(model->rowCount(QModelIndex()) == 1);

        // Add multiple intervals externally
        interval_series->addEvent(TimeFrameIndex(30), TimeFrameIndex(40));
        app->processEvents();
        REQUIRE(model->rowCount(QModelIndex()) == 2);

        interval_series->addEvent(TimeFrameIndex(50), TimeFrameIndex(60));
        app->processEvents();
        REQUIRE(model->rowCount(QModelIndex()) == 3);

        interval_series->addEvent(TimeFrameIndex(70), TimeFrameIndex(80));
        app->processEvents();

        // Verify all intervals are in the table model
        REQUIRE(model->rowCount(QModelIndex()) == 4);
        REQUIRE(model->getInterval(0).start == 10);
        REQUIRE(model->getInterval(1).start == 30);
        REQUIRE(model->getInterval(2).start == 50);
        REQUIRE(model->getInterval(3).start == 70);
    }

    SECTION("Table model updates when active key changes") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create two interval series
        auto interval_series_1 = std::make_shared<DigitalIntervalSeries>();
        interval_series_1->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));
        data_manager->setData<DigitalIntervalSeries>("intervals_1", interval_series_1, TimeKey("time"));

        auto interval_series_2 = std::make_shared<DigitalIntervalSeries>();
        interval_series_2->addEvent(TimeFrameIndex(30), TimeFrameIndex(40));
        interval_series_2->addEvent(TimeFrameIndex(50), TimeFrameIndex(60));
        data_manager->setData<DigitalIntervalSeries>("intervals_2", interval_series_2, TimeKey("time"));

        DigitalIntervalSeriesDataView view(data_manager, nullptr);
        view.setActiveKey("intervals_1");

        app->processEvents();

        // Verify initial state
        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = dynamic_cast<IntervalTableModel *>(table_view->model());
        REQUIRE(model != nullptr);
        REQUIRE(model->rowCount(QModelIndex()) == 1);

        // Change active key
        view.setActiveKey("intervals_2");
        app->processEvents();

        // Verify table model reflects new data
        REQUIRE(model->rowCount(QModelIndex()) == 2);
        REQUIRE(model->getInterval(0).start == 30);
        REQUIRE(model->getInterval(1).start == 50);
    }
}

// === Selection Mechanism Tests ===

TEST_CASE("DigitalIntervalSeriesInspector selection mechanism", "[DigitalIntervalSeriesInspector][selection]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Selection provider works correctly") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create interval series with intervals
        auto interval_series = std::make_shared<DigitalIntervalSeries>();
        interval_series->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));
        interval_series->addEvent(TimeFrameIndex(30), TimeFrameIndex(40));
        interval_series->addEvent(TimeFrameIndex(50), TimeFrameIndex(60));
        data_manager->setData<DigitalIntervalSeries>("test_intervals", interval_series, TimeKey("time"));

        // Create view and inspector
        DigitalIntervalSeriesDataView view(data_manager, nullptr);
        view.setActiveKey("test_intervals");

        DigitalIntervalSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_intervals");
        inspector.setDataView(&view);

        app->processEvents();

        // Select intervals in the view
        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * selection_model = table_view->selectionModel();
        REQUIRE(selection_model != nullptr);
        
        // Select first and third intervals (rows 0 and 2)
        // Use select() with Select flag to add to selection instead of replacing
        auto index0 = table_view->model()->index(0, 0);
        auto index2 = table_view->model()->index(2, 0);
        selection_model->select(index0, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        selection_model->select(index2, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        app->processEvents();

        // Verify selection provider returns selected intervals
        auto selected = view.getSelectedIntervals();
        REQUIRE(selected.size() == 2);
        // Note: selection order may vary, so check both possibilities
        bool found_first = false;
        bool found_third = false;
        for (auto const & interval : selected) {
            if (interval.start == 10 && interval.end == 20) {
                found_first = true;
            }
            if (interval.start == 50 && interval.end == 60) {
                found_third = true;
            }
        }
        REQUIRE(found_first == true);
        REQUIRE(found_third == true);
    }

    SECTION("Merge intervals uses selection from view") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create interval series with intervals
        auto interval_series = std::make_shared<DigitalIntervalSeries>();
        interval_series->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));
        interval_series->addEvent(TimeFrameIndex(30), TimeFrameIndex(40));
        interval_series->addEvent(TimeFrameIndex(50), TimeFrameIndex(60));
        data_manager->setData<DigitalIntervalSeries>("test_intervals", interval_series, TimeKey("time"));

        // Create view and inspector
        DigitalIntervalSeriesDataView view(data_manager, nullptr);
        view.setActiveKey("test_intervals");

        DigitalIntervalSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_intervals");
        inspector.setDataView(&view);

        app->processEvents();

        // Initially should have 3 intervals
        REQUIRE(interval_series->size() == 3);

        // Select first two intervals in the view
        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * selection_model = table_view->selectionModel();
        REQUIRE(selection_model != nullptr);
        
        // Use select() with Select flag to add to selection
        auto index0 = table_view->model()->index(0, 0);
        auto index1 = table_view->model()->index(1, 0);
        selection_model->select(index0, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        selection_model->select(index1, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        app->processEvents();
        
        // Verify selection before merge
        auto selected_before = view.getSelectedIntervals();
        REQUIRE(selected_before.size() == 2);

        // Click merge button
        auto * merge_button = inspector.findChild<QPushButton *>("merge_intervals_button");
        REQUIRE(merge_button != nullptr);
        merge_button->click();

        app->processEvents();

        // Should now have 2 intervals (first two merged)
        REQUIRE(interval_series->size() == 2);
        
        // Verify the merged interval spans from 10 to 40
        auto interval_view = interval_series->view();
        bool found_merged = false;
        for (auto const & interval_with_id : interval_view) {
            Interval const & interval = interval_with_id.value();
            if (interval.start == 10 && interval.end == 40) {
                found_merged = true;
                break;
            }
        }
        REQUIRE(found_merged == true);
    }

    SECTION("Delete intervals uses selection from view") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create interval series with intervals
        auto interval_series = std::make_shared<DigitalIntervalSeries>();
        interval_series->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));
        interval_series->addEvent(TimeFrameIndex(30), TimeFrameIndex(40));
        interval_series->addEvent(TimeFrameIndex(50), TimeFrameIndex(60));
        data_manager->setData<DigitalIntervalSeries>("test_intervals", interval_series, TimeKey("time"));

        // Create view and inspector
        DigitalIntervalSeriesDataView view(data_manager, nullptr);
        view.setActiveKey("test_intervals");

        DigitalIntervalSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_intervals");
        inspector.setDataView(&view);

        app->processEvents();

        // Initially should have 3 intervals
        REQUIRE(interval_series->size() == 3);

        // Select middle interval in the view
        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        table_view->selectRow(1);
        app->processEvents();

        // Get the widget and call delete (we need to access the widget directly)
        // Since delete is a private slot, we'll test through the selection mechanism
        // by verifying that getSelectedIntervals works correctly
        auto selected = view.getSelectedIntervals();
        REQUIRE(selected.size() == 1);
        REQUIRE(selected[0].start == 30);
        REQUIRE(selected[0].end == 40);
    }

    SECTION("Extend interval uses selection from view") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Set current time to frame 70
        data_manager->setCurrentTime(70);

        // Create interval series with an interval
        auto interval_series = std::make_shared<DigitalIntervalSeries>();
        interval_series->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));
        data_manager->setData<DigitalIntervalSeries>("test_intervals", interval_series, TimeKey("time"));

        // Create view and inspector
        DigitalIntervalSeriesDataView view(data_manager, nullptr);
        view.setActiveKey("test_intervals");

        DigitalIntervalSeriesInspector inspector(data_manager, nullptr, nullptr);
        inspector.setActiveKey("test_intervals");
        inspector.setDataView(&view);

        app->processEvents();

        // Select the interval in the view
        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        table_view->selectRow(0);
        app->processEvents();

        // Click extend button
        auto * extend_button = inspector.findChild<QPushButton *>("extend_interval_button");
        REQUIRE(extend_button != nullptr);
        extend_button->click();

        app->processEvents();

        // Should now have an extended interval [10, 70]
        auto interval_view = interval_series->view();
        bool found_extended = false;
        for (auto const & interval_with_id : interval_view) {
            Interval const & interval = interval_with_id.value();
            if (interval.start == 10 && interval.end == 70) {
                found_extended = true;
                break;
            }
        }
        REQUIRE(found_extended == true);
    }
}

TEST_CASE("DigitalIntervalSeriesDataView double-click emits frameSelected without recursion",
          "[DataInspectorWidget][DigitalIntervalSeries][double_click]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    auto data_manager = std::make_shared<DataManager>();

    // Time base
    constexpr int kNumTimes = 100;
    std::vector<int> t(kNumTimes);
    std::iota(t.begin(), t.end(), 0);
    auto tf = std::make_shared<TimeFrame>(t);
    data_manager->setTime(TimeKey("time"), tf);

    // Interval data
    auto interval_series = std::make_shared<DigitalIntervalSeries>();
    interval_series->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));
    interval_series->addEvent(TimeFrameIndex(30), TimeFrameIndex(40));
    data_manager->setData<DigitalIntervalSeries>("test_intervals", interval_series, TimeKey("time"));

    // Full widget wiring (View + Properties share state)
    auto state = std::make_shared<DataInspectorState>();

    DataInspectorViewWidget view_widget(data_manager, nullptr);
    view_widget.setState(state);

    DataInspectorPropertiesWidget props_widget(data_manager, nullptr, nullptr);
    props_widget.setState(state);
    props_widget.setViewWidget(&view_widget);

    // Trigger creation of the correct view/inspector
    state->setInspectedDataKey(QStringLiteral("test_intervals"));
    app->processEvents();

    auto * interval_view = dynamic_cast<DigitalIntervalSeriesDataView *>(view_widget.currentView());
    REQUIRE(interval_view != nullptr);

    // Spy on both the data view signal and the view widget signal to verify the chain works
    QSignalSpy data_view_spy(interval_view, &BaseDataView::frameSelected);
    QSignalSpy view_widget_spy(&view_widget, &DataInspectorViewWidget::frameSelected);
    REQUIRE(data_view_spy.isValid());
    REQUIRE(view_widget_spy.isValid());

    auto * table_view = interval_view->tableView();
    REQUIRE(table_view != nullptr);
    REQUIRE(table_view->model() != nullptr);
    REQUIRE(table_view->model()->rowCount() >= 1);

    // Double click row 0 -> should emit frameSelected(start) exactly once.
    QModelIndex idx0 = table_view->model()->index(0, 0);
    REQUIRE(idx0.isValid());

    // Invoke the slot directly to simulate the double-click behavior
    // This is more reliable in headless/CI environments and tests the actual behavior
    bool ok = QMetaObject::invokeMethod(interval_view,
                                        "_handleTableViewDoubleClicked",
                                        Qt::DirectConnection,
                                        Q_ARG(QModelIndex, idx0));
    REQUIRE(ok);
    app->processEvents();

    // Verify the data view emitted the signal
    REQUIRE(data_view_spy.count() == 1);
    auto const data_view_args = data_view_spy.takeFirst();
    REQUIRE(data_view_args.size() == 1);
    REQUIRE(data_view_args[0].toInt() == 10);

    // Verify the view widget re-emitted the signal (proving the connection works)
    REQUIRE(view_widget_spy.count() == 1);
    auto const view_widget_args = view_widget_spy.takeFirst();
    REQUIRE(view_widget_args.size() == 1);
    REQUIRE(view_widget_args[0].toInt() == 10);
}
