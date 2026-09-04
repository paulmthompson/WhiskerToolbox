/**
 * @file DataInspectorTimeSync.test.cpp
 * @brief Integration tests for DataInspector interval double-click time sync with TimeScrollBar
 */

#include "DataInspector_Widget/DataInspectorState.hpp"
#include "DataInspector_Widget/DataInspectorViewWidget.hpp"
#include "DataInspector_Widget/DataInspectorWidgetRegistration.hpp"
#include "DataInspector_Widget/DigitalIntervalSeries/DigitalIntervalSeriesDataView.hpp"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include <QApplication>
#include <QSpinBox>
#include <QTableView>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <numeric>
#include <vector>

namespace {

void ensureQApplication() {
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static char * argv[] = {app_name};
        new QApplication(argc, argv);// NOLINT: Intentionally leaked
    }
}

[[nodiscard]] std::shared_ptr<TimeFrame> makeDenseTimeFrame(int count) {
    std::vector<int> times(static_cast<size_t>(count));
    std::iota(times.begin(), times.end(), 0);
    return std::make_shared<TimeFrame>(times);
}

struct SparseMasterTimeFrameConfig {
    int count;
    int tick_step;
};

[[nodiscard]] std::shared_ptr<TimeFrame> makeSparseMasterTimeFrame(SparseMasterTimeFrameConfig config) {
    std::vector<int> times;
    times.reserve(static_cast<size_t>(config.count));
    for (int i = 0; i < config.count; ++i) {
        times.push_back(i * config.tick_step);
    }
    return std::make_shared<TimeFrame>(times);
}

struct InspectorTimeSyncFixture {
    std::shared_ptr<DataManager> data_manager;
    std::unique_ptr<EditorRegistry> registry;
    TimeScrollBar * time_scrollbar{nullptr};
    std::shared_ptr<DataInspectorState> inspector_state;
    DigitalIntervalSeriesDataView * interval_view{nullptr};
    std::shared_ptr<TimeFrame> time_tf;
    std::shared_ptr<TimeFrame> master_tf;

    InspectorTimeSyncFixture(TimeKey const & interval_time_key,
                             TimeFrameIndex interval_start,
                             TimeFrameIndex interval_end) {
        qRegisterMetaType<TimePosition>("TimePosition");
        ensureQApplication();

        data_manager = std::make_shared<DataManager>();
        time_tf = makeDenseTimeFrame(100);
        data_manager->setTime(TimeKey("time"), time_tf, true);

        if (interval_time_key == TimeKey("master")) {
            master_tf = makeSparseMasterTimeFrame({.count = 100, .tick_step = 10});
            data_manager->setTime(TimeKey("master"), master_tf, true);
        }

        auto interval_series = std::make_shared<DigitalIntervalSeries>();
        interval_series->addEvent(interval_start, interval_end);
        data_manager->setData<DigitalIntervalSeries>(
                "test_intervals", interval_series, interval_time_key);

        registry = std::make_unique<EditorRegistry>(nullptr);
        DataInspectorModule::registerTypes(registry.get(), data_manager);

        time_scrollbar = new TimeScrollBar(data_manager, nullptr, nullptr);
        time_scrollbar->setEditorRegistry(registry.get());
        time_scrollbar->setTimeFrame(data_manager->getTime(TimeKey("time")), TimeKey("time"));

        auto const instance = registry->createEditor(EditorLib::EditorTypeId("DataInspector"));
        REQUIRE(instance.state != nullptr);
        REQUIRE(instance.view != nullptr);

        inspector_state = std::dynamic_pointer_cast<DataInspectorState>(instance.state);
        REQUIRE(inspector_state != nullptr);

        inspector_state->setInspectedDataKey(QStringLiteral("test_intervals"));
        QApplication::processEvents();

        auto * view_widget = qobject_cast<DataInspectorViewWidget *>(instance.view);
        REQUIRE(view_widget != nullptr);
        interval_view = dynamic_cast<DigitalIntervalSeriesDataView *>(view_widget->currentView());
        REQUIRE(interval_view != nullptr);
    }

    [[nodiscard]] int frameSpinboxValue() const {
        auto * frame_spinbox = time_scrollbar->findChild<QSpinBox *>("frame_spinbox");
        REQUIRE(frame_spinbox != nullptr);
        return frame_spinbox->value();
    }

    void doubleClickIntervalStart() const {
        auto * table_view = interval_view->tableView();
        REQUIRE(table_view != nullptr);
        REQUIRE(table_view->model() != nullptr);
        REQUIRE(table_view->model()->rowCount() >= 1);

        QModelIndex const idx_start = table_view->model()->index(0, 0);
        REQUIRE(idx_start.isValid());

        bool const ok = QMetaObject::invokeMethod(interval_view,
                                                  "_handleTableViewDoubleClicked",
                                                  Qt::DirectConnection,
                                                  Q_ARG(QModelIndex, idx_start));
        REQUIRE(ok);
        QApplication::processEvents();
    }

    void pressNextFrame() const {
        time_scrollbar->changeScrollBarValue(1, true);
        QApplication::processEvents();
    }
};

}// namespace

TEST_CASE("DataInspector interval double-click syncs same-clock arrow navigation",
          "[DataInspectorWidget][DigitalIntervalSeries][time_sync][integration]") {
    InspectorTimeSyncFixture fixture(
            TimeKey("time"), TimeFrameIndex(10), TimeFrameIndex(20));

    fixture.registry->setCurrentTime(TimePosition(TimeFrameIndex(50), fixture.time_tf));
    fixture.doubleClickIntervalStart();

    REQUIRE(fixture.registry->currentPosition().index.getValue() == 10);
    REQUIRE(fixture.frameSpinboxValue() == 10);

    fixture.pressNextFrame();

    REQUIRE(fixture.registry->currentPosition().index.getValue() == 11);
    REQUIRE(fixture.registry->currentPosition().sameClock(fixture.time_tf));
    REQUIRE(fixture.frameSpinboxValue() == 11);
}

TEST_CASE("DataInspector interval double-click syncs cross-clock arrow navigation",
          "[DataInspectorWidget][DigitalIntervalSeries][time_sync][integration]") {
    constexpr int64_t kMasterStartFrame = 5;
    constexpr int64_t kExpectedTimeFrame = 50;

    InspectorTimeSyncFixture fixture(TimeKey("master"),
                                     TimeFrameIndex(kMasterStartFrame),
                                     TimeFrameIndex(kMasterStartFrame + 1));

    fixture.registry->setCurrentTime(TimePosition(TimeFrameIndex(30), fixture.time_tf));
    fixture.doubleClickIntervalStart();

    REQUIRE(fixture.registry->currentPosition().index.getValue() == kExpectedTimeFrame);
    REQUIRE(fixture.registry->currentPosition().sameClock(fixture.time_tf));
    REQUIRE(fixture.frameSpinboxValue() == kExpectedTimeFrame);

    fixture.pressNextFrame();

    REQUIRE(fixture.registry->currentPosition().index.getValue() == kExpectedTimeFrame + 1);
    REQUIRE(fixture.registry->currentPosition().sameClock(fixture.time_tf));
    REQUIRE(fixture.frameSpinboxValue() == kExpectedTimeFrame + 1);
}
