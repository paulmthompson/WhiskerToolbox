/**
 * @file ColumnConfigDialog.test.cpp
 * @brief Tests for ColumnConfigDialog pipeline library integration
 */

#include "TensorData/ColumnConfigDialog.hpp"
#include "TensorData/TensorDesigner.hpp"
#include "TensorData/TensorInspector.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataInspector_Widget/DataInspectorPropertiesWidget.hpp"
#include "DataInspector_Widget/DataInspectorState.hpp"
#include "DataInspector_Widget/DataInspectorViewWidget.hpp"
#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Tensors/TensorData.hpp"
#include "Tensors/storage/LazyColumnTensorStorage.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TransformsV2/io/PipelineLibrary.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QPushButton>
#include <QSignalSpy>
#include <QTimer>

#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

using Catch::Matchers::WithinAbs;
using namespace Neuralyzer::Transforms::V2::Examples;

namespace {

class QtAppFixture {
public:
    QtAppFixture() {
        if (!QApplication::instance()) {
            int argc = 0;
            char * argv[] = {nullptr};
            _app = std::make_unique<QApplication>(argc, argv);
        }
    }

    std::unique_ptr<QApplication> _app;
};

std::shared_ptr<TimeFrame> makeIdentityTimeFrame(int64_t max_index) {
    std::vector<int> times;
    times.reserve(static_cast<std::size_t>(max_index + 1));
    for (int64_t i = 0; i <= max_index; ++i) {
        times.push_back(static_cast<int>(i));
    }
    return std::make_shared<TimeFrame>(times);
}

std::shared_ptr<AnalogTimeSeries> makeLinearAnalog(std::size_t count) {
    std::vector<float> values;
    std::vector<TimeFrameIndex> times;
    values.reserve(count);
    times.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        values.push_back(static_cast<float>(i));
        times.emplace_back(static_cast<int64_t>(i));
    }
    return std::make_shared<AnalogTimeSeries>(std::move(values), std::move(times));
}

std::shared_ptr<DigitalIntervalSeries> makeIntervals(
        std::vector<std::pair<int64_t, int64_t>> const & intervals) {
    std::vector<TimeFrameInterval> values;
    values.reserve(intervals.size());
    for (auto const & [start, end]: intervals) {
        values.emplace_back(TimeFrameIndex(start), TimeFrameIndex(end));
    }
    return std::make_shared<DigitalIntervalSeries>(std::move(values));
}

std::shared_ptr<DigitalEventSeries> makeEventSeries(std::vector<int64_t> const & times) {
    std::vector<TimeFrameIndex> event_times;
    event_times.reserve(times.size());
    for (auto const time: times) {
        event_times.emplace_back(time);
    }
    return std::make_shared<DigitalEventSeries>(std::move(event_times));
}

[[nodiscard]] QPushButton * findBuildTensorButton(TensorDesigner & designer) {
    for (auto * button: designer.findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("Build Tensor")) {
            return button;
        }
    }
    return nullptr;
}

void drainDeferredQtEvents() {
    for (int i = 0; i < 4; ++i) {
        QApplication::processEvents();
    }
}

std::shared_ptr<TensorData> makePlaceholderTensor() {
    std::vector<ColumnSource> cols;
    cols.push_back(ColumnSource{
            "value",
            []() { return std::vector<float>{0.0F}; },
            {}});
    return std::make_shared<TensorData>(
            TensorData::createFromLazyColumns(1, std::move(cols), RowDescriptor::ordinal(1)));
}

class CoutCapture {
public:
    CoutCapture() {
        _old_buf = std::cout.rdbuf(_buffer.rdbuf());
    }

    ~CoutCapture() {
        std::cout.rdbuf(_old_buf);
    }

    [[nodiscard]] std::string str() const {
        return _buffer.str();
    }

private:
    std::stringstream _buffer;
    std::streambuf * _old_buf{nullptr};
};

}// namespace

TEST_CASE("ColumnConfigDialog exposes Load from Library when library dir is set",
          "[DataInspector][ColumnConfigDialog][library]") {

    QtAppFixture const qt;

    auto const config_dir =
            QDir::temp().filePath(QStringLiteral("wt_column_config_%1").arg(QDateTime::currentMSecsSinceEpoch()));
    auto const dir_result = ensureUserPipelineDirectory(config_dir.toStdString());
    REQUIRE(dir_result);

    auto data_manager = std::make_shared<DataManager>();
    ColumnConfigDialog dialog(data_manager,
                              DesignerRowType::Ordinal,
                              nullptr,
                              QString::fromStdString(dir_result.value().string()));
    dialog.show();
    QApplication::processEvents();

    bool found = false;
    for (auto * button: dialog.findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("Load from Library...")) {
            found = true;
            CHECK(button->isEnabled());
        }
    }
    CHECK(found);
}

TEST_CASE("ColumnConfigDialog disables Load from Library without library dir",
          "[DataInspector][ColumnConfigDialog][library]") {

    QtAppFixture const qt;

    auto data_manager = std::make_shared<DataManager>();
    ColumnConfigDialog dialog(data_manager, DesignerRowType::Ordinal);
    dialog.show();
    QApplication::processEvents();

    for (auto * button: dialog.findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("Load from Library...")) {
            CHECK_FALSE(button->isEnabled());
        }
    }
}

TEST_CASE("TensorDesigner loads Phase6 config and builds expected tensor",
          "[DataInspector][TensorDesigner][Phase6]") {
    QtAppFixture const qt;

    auto data_manager = std::make_shared<DataManager>();
    REQUIRE(data_manager->setTime(TimeKey("time"), makeIdentityTimeFrame(100), true));
    data_manager->setData<AnalogTimeSeries>("signal", makeLinearAnalog(100), TimeKey("time"));
    data_manager->setData<DigitalIntervalSeries>(
            "intervals",
            makeIntervals({{10, 20}, {50, 60}}),
            TimeKey("time"));

    TensorDesigner designer(data_manager);
    designer.show();
    QApplication::processEvents();

    std::string const json = R"({
        "tensor_key": "ui_phase6_tensor",
        "row_source": {"data_key": "intervals", "row_type": "interval"},
        "columns": [
            {
                "name": "signal_at_shifted_start",
                "source_key": "signal",
                "row_pipeline_json": "{\"steps\": [{\"step_id\": \"start\", \"transform_name\": \"IntervalToEvent\", \"parameters\": {\"point\": \"start\"}}, {\"step_id\": \"shift\", \"transform_name\": \"ShiftDigitalEventSeries\", \"parameters\": {\"offset\": 2}}]}",
                "pipeline_json": "{\"steps\": []}"
            }
        ]
    })";

    REQUIRE(designer.fromJson(json));
    REQUIRE(designer.tensorKey() == "ui_phase6_tensor");

    QSignalSpy spy(&designer, &TensorDesigner::tensorCreated);
    auto * build_button = [&designer]() -> QPushButton * {
        for (auto * button: designer.findChildren<QPushButton *>()) {
            if (button->text() == QStringLiteral("Build Tensor")) {
                return button;
            }
        }
        return nullptr;
    }();
    REQUIRE(build_button != nullptr);

    build_button->click();
    drainDeferredQtEvents();

    REQUIRE(spy.count() == 1);
    auto tensor = data_manager->getData<TensorData>("ui_phase6_tensor");
    REQUIRE(tensor != nullptr);
    REQUIRE(tensor->numRows() == 2);
    REQUIRE(tensor->numColumns() == 1);

    auto const values = tensor->getColumn(0);
    REQUIRE(values.size() == 2);
    CHECK_THAT(values[0], WithinAbs(12.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(52.0, 0.01));
}

TEST_CASE("TensorDesigner loads TimeFrame row config and builds expected tensor",
          "[DataInspector][TensorDesigner][Phase9a]") {
    QtAppFixture const qt;

    auto data_manager = std::make_shared<DataManager>();
    REQUIRE(data_manager->setTime(TimeKey("frame"), makeIdentityTimeFrame(4), true));
    data_manager->setData<AnalogTimeSeries>("signal", makeLinearAnalog(5), TimeKey("frame"));

    TensorDesigner designer(data_manager);
    designer.show();
    QApplication::processEvents();

    std::string const json = R"({
        "tensor_key": "ui_timeframe_tensor",
        "row_source": {"time_key": "frame", "row_type": "timeframe"},
        "columns": [
            {
                "name": "signal_value",
                "source_key": "signal",
                "pipeline_json": "{\"steps\": []}"
            }
        ]
    })";

    REQUIRE(designer.fromJson(json));
    REQUIRE(designer.tensorKey() == "ui_timeframe_tensor");

    QSignalSpy spy(&designer, &TensorDesigner::tensorCreated);
    auto * build_button = [&designer]() -> QPushButton * {
        for (auto * button: designer.findChildren<QPushButton *>()) {
            if (button->text() == QStringLiteral("Build Tensor")) {
                return button;
            }
        }
        return nullptr;
    }();
    REQUIRE(build_button != nullptr);

    build_button->click();
    drainDeferredQtEvents();

    REQUIRE(spy.count() == 1);
    auto tensor = data_manager->getData<TensorData>("ui_timeframe_tensor");
    REQUIRE(tensor != nullptr);
    REQUIRE(tensor->numRows() == 5);
    REQUIRE(tensor->numColumns() == 1);

    auto const values = tensor->getColumn(0);
    REQUIRE(values.size() == 5);
    CHECK_THAT(values[0], WithinAbs(0.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(1.0, 0.01));
    CHECK_THAT(values[2], WithinAbs(2.0, 0.01));
    CHECK_THAT(values[3], WithinAbs(3.0, 0.01));
    CHECK_THAT(values[4], WithinAbs(4.0, 0.01));
}

TEST_CASE("TensorInspector builds contact_0 spikes_1 cross-timeframe validation scenario",
          "[DataInspector][TensorDesigner][validation][contact_0]") {
    QtAppFixture const qt;

    auto data_manager = std::make_shared<DataManager>();
    REQUIRE(data_manager->setTime(TimeKey("time"), makeIdentityTimeFrame(1000), true));
    REQUIRE(data_manager->setTime(TimeKey("master"), makeIdentityTimeFrame(1000), true));

    auto const contact_0 = makeIntervals({{100, 120}, {200, 230}});
    auto const spikes_1 = makeEventSeries({95, 105, 112, 190, 205, 220});
    data_manager->setData<DigitalIntervalSeries>("contact_0", contact_0, TimeKey("time"));
    data_manager->setData<DigitalEventSeries>("spikes_1", spikes_1, TimeKey("master"));

    auto state = std::make_shared<DataInspectorState>();
    auto view = std::make_unique<DataInspectorViewWidget>(data_manager, nullptr);
    view->setState(state);

    auto props = std::make_unique<DataInspectorPropertiesWidget>(data_manager, nullptr, nullptr);
    props->setState(state);
    props->setViewWidget(view.get());

    // Bootstrap the real UI path: TensorInspector embedded in the properties panel.
    data_manager->setData<TensorData>("placeholder", makePlaceholderTensor(), TimeKey("time"));
    props->inspectData(QStringLiteral("placeholder"));
    drainDeferredQtEvents();

    auto * inspector = props->findChild<TensorInspector *>();
    REQUIRE(inspector != nullptr);
    auto * designer = inspector->designer();
    REQUIRE(designer != nullptr);

    std::string const json = R"({
        "tensor_key": "test",
        "row_source": {"data_key": "contact_0", "row_type": "interval"},
        "columns": [
            {
                "preset": "trial_relative_event_count",
                "row_modifier": "bind_interval_start",
                "parameters": {
                    "output_name": "spike_presence",
                    "source_key": "spikes_1",
                    "binding_source_key": "contact_0",
                    "store_key": "row_alignment_time",
                    "window_start": 0.0,
                    "window_end": 15.0
                }
            }
        ]
    })";

    REQUIRE(designer->fromJson(json));
    REQUIRE(designer->tensorKey() == "test");

    CoutCapture cout_capture;
    QSignalSpy created_spy(designer, &TensorDesigner::tensorCreated);
    auto * build_button = findBuildTensorButton(*designer);
    REQUIRE(build_button != nullptr);

    build_button->click();
    drainDeferredQtEvents();

    REQUIRE(created_spy.count() == 1);
    REQUIRE(data_manager->getType("test") == DM_DataType::Tensor);
    REQUIRE_FALSE(data_manager->getTimeKey("test").empty());
    REQUIRE(data_manager->getTimeKey("test").str() == "time");
    REQUIRE(state->inspectedDataKey() == QStringLiteral("test"));

    auto tensor = data_manager->getData<TensorData>("test");
    REQUIRE(tensor != nullptr);
    REQUIRE(tensor->numRows() == contact_0->size());
    REQUIRE(tensor->numColumns() == 1);

    auto const values = tensor->getColumn(0);
    REQUIRE(values.size() == contact_0->size());
    CHECK_THAT(values[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(1.0, 0.01));

    CHECK(cout_capture.str().find("Unsupported feature type") == std::string::npos);
    REQUIRE(data_manager->getType("test") != DM_DataType::Unknown);

    // Rebuild the same key while inspecting it — this used to segfault.
    created_spy.clear();
    build_button->click();
    drainDeferredQtEvents();

    REQUIRE(created_spy.count() == 1);
    REQUIRE(data_manager->getType("test") == DM_DataType::Tensor);
    REQUIRE(state->inspectedDataKey() == QStringLiteral("test"));
    REQUIRE(props->findChild<TensorInspector *>() != nullptr);

    auto rebuilt = data_manager->getData<TensorData>("test");
    REQUIRE(rebuilt != nullptr);
    auto const rebuilt_values = rebuilt->getColumn(0);
    REQUIRE(rebuilt_values.size() == contact_0->size());
    CHECK_THAT(rebuilt_values[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(rebuilt_values[1], WithinAbs(1.0, 0.01));
}
