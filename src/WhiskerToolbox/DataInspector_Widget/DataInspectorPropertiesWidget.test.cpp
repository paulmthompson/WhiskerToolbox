/**
 * @file DataInspectorPropertiesWidget.test.cpp
 * @brief Tests for DataInspectorPropertiesWidget, focusing on data deletion handling.
 *
 * Verifies that when DataManager::deleteData() removes the currently inspected key,
 * the inspector and view widgets safely clear their state without crashes or
 * dangling references.
 */

#include "DataInspectorPropertiesWidget.hpp"
#include "DataInspectorState.hpp"
#include "DataInspectorViewWidget.hpp"
#include "Inspectors/BaseDataView.hpp"
#include "Inspectors/BaseInspector.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QSignalSpy>

#include <array>
#include <memory>
#include <numeric>
#include <vector>

namespace {

void ensureQApplication() {
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());// NOLINT: Intentionally leaked
    }
}

/// Helper that creates a DataManager with a default time frame and one data key.
struct TestFixture {
    std::shared_ptr<DataManager> dm;
    std::shared_ptr<DataInspectorState> state;
    std::unique_ptr<DataInspectorPropertiesWidget> props;
    std::unique_ptr<DataInspectorViewWidget> view;

    TestFixture() {
        dm = std::make_shared<DataManager>();

        constexpr int kFrames = 100;
        std::vector<int> times(kFrames);
        std::iota(times.begin(), times.end(), 0);
        dm->setTime(TimeKey("time"), std::make_shared<TimeFrame>(times));

        state = std::make_shared<DataInspectorState>();

        view = std::make_unique<DataInspectorViewWidget>(dm, nullptr);
        view->setState(state);

        props = std::make_unique<DataInspectorPropertiesWidget>(dm, nullptr, nullptr);
        props->setState(state);
        props->setViewWidget(view.get());
    }

    void inspectKey(std::string const & key) {
        state->setInspectedDataKey(QString::fromStdString(key));
        QApplication::processEvents();
    }
};

}// namespace

// =============================================================================
// Deletion Detection Tests
// =============================================================================

TEST_CASE("DataInspectorPropertiesWidget clears on data deletion", "[DataInspector][deletion]") {
    ensureQApplication();
    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    TestFixture f;

    SECTION("MaskData deletion clears inspector") {
        auto mask = std::make_shared<MaskData>();
        f.dm->setData<MaskData>("test_mask", mask, TimeKey("time"));
        f.inspectKey("test_mask");

        REQUIRE(f.state->inspectedDataKey() == QStringLiteral("test_mask"));

        f.dm->deleteData("test_mask");
        app->processEvents();

        CHECK(f.state->inspectedDataKey().isEmpty());
    }

    SECTION("PointData deletion clears inspector") {
        auto points = std::make_shared<PointData>();
        f.dm->setData<PointData>("test_points", points, TimeKey("time"));
        f.inspectKey("test_points");

        REQUIRE(f.state->inspectedDataKey() == QStringLiteral("test_points"));

        f.dm->deleteData("test_points");
        app->processEvents();

        CHECK(f.state->inspectedDataKey().isEmpty());
    }

    SECTION("LineData deletion clears inspector") {
        auto lines = std::make_shared<LineData>();
        f.dm->setData<LineData>("test_lines", lines, TimeKey("time"));
        f.inspectKey("test_lines");

        REQUIRE(f.state->inspectedDataKey() == QStringLiteral("test_lines"));

        f.dm->deleteData("test_lines");
        app->processEvents();

        CHECK(f.state->inspectedDataKey().isEmpty());
    }

    SECTION("DigitalEventSeries deletion clears inspector") {
        auto events = std::make_shared<DigitalEventSeries>();
        events->addEvent(TimeFrameIndex(5));
        f.dm->setData<DigitalEventSeries>("test_events", events, TimeKey("time"));
        f.inspectKey("test_events");

        REQUIRE(f.state->inspectedDataKey() == QStringLiteral("test_events"));

        f.dm->deleteData("test_events");
        app->processEvents();

        CHECK(f.state->inspectedDataKey().isEmpty());
    }

    SECTION("DigitalIntervalSeries deletion clears inspector") {
        auto intervals = std::make_shared<DigitalIntervalSeries>();
        intervals->addEvent(TimeFrameIndex(10), TimeFrameIndex(20));
        f.dm->setData<DigitalIntervalSeries>("test_intervals", intervals, TimeKey("time"));
        f.inspectKey("test_intervals");

        REQUIRE(f.state->inspectedDataKey() == QStringLiteral("test_intervals"));

        f.dm->deleteData("test_intervals");
        app->processEvents();

        CHECK(f.state->inspectedDataKey().isEmpty());
    }

    SECTION("AnalogTimeSeries deletion clears inspector") {
        auto analog = std::make_shared<AnalogTimeSeries>();
        f.dm->setData<AnalogTimeSeries>("test_analog", analog, TimeKey("time"));
        f.inspectKey("test_analog");

        REQUIRE(f.state->inspectedDataKey() == QStringLiteral("test_analog"));

        f.dm->deleteData("test_analog");
        app->processEvents();

        CHECK(f.state->inspectedDataKey().isEmpty());
    }

    SECTION("TensorData deletion clears inspector") {
        auto tensor = std::make_shared<TensorData>();
        f.dm->setData<TensorData>("test_tensor", tensor, TimeKey("time"));
        f.inspectKey("test_tensor");

        REQUIRE(f.state->inspectedDataKey() == QStringLiteral("test_tensor"));

        f.dm->deleteData("test_tensor");
        app->processEvents();

        CHECK(f.state->inspectedDataKey().isEmpty());
    }
}

TEST_CASE("DataInspectorViewWidget clears on data deletion", "[DataInspector][deletion]") {
    ensureQApplication();
    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    TestFixture f;

    auto mask = std::make_shared<MaskData>();
    f.dm->setData<MaskData>("view_mask", mask, TimeKey("time"));
    f.inspectKey("view_mask");

    REQUIRE(f.view->currentView() != nullptr);

    f.dm->deleteData("view_mask");
    app->processEvents();

    // View should be cleared (no current view)
    CHECK(f.view->currentView() == nullptr);
}

TEST_CASE("Deleting non-inspected key does not affect inspector", "[DataInspector][deletion]") {
    ensureQApplication();
    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    TestFixture f;

    auto mask1 = std::make_shared<MaskData>();
    auto mask2 = std::make_shared<MaskData>();
    f.dm->setData<MaskData>("mask_a", mask1, TimeKey("time"));
    f.dm->setData<MaskData>("mask_b", mask2, TimeKey("time"));
    f.inspectKey("mask_a");

    REQUIRE(f.state->inspectedDataKey() == QStringLiteral("mask_a"));

    // Delete a different key
    f.dm->deleteData("mask_b");
    app->processEvents();

    // Inspector should still show mask_a
    CHECK(f.state->inspectedDataKey() == QStringLiteral("mask_a"));
}

TEST_CASE("Inspector handles repeated deletion gracefully", "[DataInspector][deletion]") {
    ensureQApplication();
    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    TestFixture f;

    // Create, inspect, delete, repeat - should not crash
    for (int i = 0; i < 3; ++i) {
        std::string const key = "cycle_data_" + std::to_string(i);
        auto mask = std::make_shared<MaskData>();
        f.dm->setData<MaskData>(key, mask, TimeKey("time"));
        f.inspectKey(key);

        REQUIRE(f.state->inspectedDataKey() == QString::fromStdString(key));

        f.dm->deleteData(key);
        app->processEvents();

        CHECK(f.state->inspectedDataKey().isEmpty());
    }
}

TEST_CASE("inspectedDataKeyChanged signal fires on deletion", "[DataInspector][deletion]") {
    ensureQApplication();
    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    TestFixture f;

    auto mask = std::make_shared<MaskData>();
    f.dm->setData<MaskData>("signal_mask", mask, TimeKey("time"));
    f.inspectKey("signal_mask");

    QSignalSpy spy(f.state.get(), &DataInspectorState::inspectedDataKeyChanged);
    REQUIRE(spy.isValid());

    f.dm->deleteData("signal_mask");
    app->processEvents();

    // Should have received at least one signal with empty key
    REQUIRE(spy.count() >= 1);
    auto const last_args = spy.last();
    CHECK(last_args[0].toString().isEmpty());
}
