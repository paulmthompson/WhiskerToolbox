/**
 * @file test_tensor_design_builder.cpp
 * @brief Unit tests for the Qt-free TensorDesign library.
 */

#include "TensorDesign/ColumnRecipePresetRegistry.hpp"
#include "TensorDesign/TensorDesignBuilder.hpp"

#include "../fixtures/GatherAlignmentFixtures.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Points/Point_Data.hpp"
#include "Tensors/TensorData.hpp"

#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using DesignRowType = Neuralyzer::TensorDesign::RowType;
using Catch::Matchers::WithinAbs;
using Neuralyzer::TensorBuilders::IntervalProperty;
using Neuralyzer::TensorDesign::buildTensor;
using Neuralyzer::TensorDesign::buildTensorFromDesignJson;
using Neuralyzer::TensorDesign::ColumnRecipePresetArgs;
using Neuralyzer::TensorDesign::createBuiltInColumnRecipePresetRegistry;
using Neuralyzer::TensorDesign::parseDesignJson;
using Neuralyzer::TensorDesign::populateDataManager;
using Neuralyzer::TensorDesign::serializeDesignJson;
using Neuralyzer::TensorDesign::TensorDesignSpec;

namespace {

using Neuralyzer::Test::GatherFixtures::createIdentityTimeFrameForMax;
using Neuralyzer::Test::GatherFixtures::createTimeFrameForRate;

constexpr char const * kMeanValuePipelineJson =
        R"({"steps": [], "range_reduction": {"reduction_name": "MeanValue"}})";
constexpr char const * kIdentityRowPipelineJson = R"({"steps": []})";

template<typename T>
T requireValue(std::optional<T> const & opt) {
    if (!opt.has_value()) {
        throw std::runtime_error("expected optional value");
    }
    return opt.value();
}

std::shared_ptr<AnalogTimeSeries> createLinearAnalog(std::size_t num_samples) {
    std::vector<float> data;
    std::vector<TimeFrameIndex> times;
    data.reserve(num_samples);
    times.reserve(num_samples);
    for (std::size_t i = 0; i < num_samples; ++i) {
        data.push_back(static_cast<float>(i));
        times.emplace_back(static_cast<int64_t>(i));
    }
    return std::make_shared<AnalogTimeSeries>(std::move(data), std::move(times));
}

/**
 * @brief Replace DataManager's default clock with an identity TimeFrame.
 * @pre max_time must cover all source and row indices inserted under TimeKey("time").
 * @post Data registered with TimeKey("time") receives a non-empty identity TimeFrame.
 */
void setDefaultIdentityTimeFrame(DataManager & dm, int64_t max_time) {
    REQUIRE(dm.setTime(TimeKey("time"), createIdentityTimeFrameForMax(max_time), true));
}

std::shared_ptr<DigitalIntervalSeries> createIntervalSeries(
        std::vector<std::pair<int64_t, int64_t>> const & intervals) {
    std::vector<TimeFrameInterval> vec;
    vec.reserve(intervals.size());
    for (auto const & [start, end]: intervals) {
        vec.emplace_back(TimeFrameInterval(TimeFrameIndex(start), TimeFrameIndex(end)));
    }
    return std::make_shared<DigitalIntervalSeries>(vec);
}

std::shared_ptr<DigitalEventSeries> createEventSeries(std::vector<int64_t> const & times) {
    auto series = std::make_shared<DigitalEventSeries>();
    for (auto const time: times) {
        series->addEvent(TimeFrameIndex(time));
    }
    return series;
}

std::shared_ptr<TimeFrame> createTimeFrameFromTimes(std::vector<int> const & times) {
    return std::make_shared<TimeFrame>(times);
}

std::shared_ptr<PointData> createPointData(
        std::vector<std::pair<int64_t, Point2D<float>>> const & points) {
    auto data = std::make_shared<PointData>();
    for (auto const & [time, point]: points) {
        data->addAtTime(TimeFrameIndex(time), point, NotifyObservers::No);
    }
    return data;
}

}// namespace

TEST_CASE("parseDesignJson parses valid interval design", "[TensorDesign]") {
    std::string const json = R"({
        "tensor_key": "features",
        "row_source": {
            "data_key": "intervals",
            "row_type": "interval"
        },
        "columns": [
            {
                "name": "mean_signal",
                "source_key": "signal",
                "pipeline_json": "{\"steps\": [], \"range_reduction\": {\"reduction_name\": \"MeanValue\"}}"
            }
        ]
    })";

    auto const parsed = requireValue(parseDesignJson(json));
    REQUIRE(parsed.tensor_key == "features");
    REQUIRE(parsed.row_source_key == "intervals");
    REQUIRE(parsed.row_type == DesignRowType::Interval);
    REQUIRE(parsed.columns.size() == 1);
    REQUIRE(parsed.columns[0].column_name == "mean_signal");
    REQUIRE(parsed.columns[0].row_pipeline_json.empty());
}

TEST_CASE("parseDesignJson parses row_pipeline_json", "[TensorDesign][Phase2]") {
    std::string const expected_row_pipeline =
            R"({"steps": [{"transform_name": "IntervalToEvent", "parameters": {"point": "start"}}]})";
    std::string const json = R"({
        "tensor_key": "features",
        "row_source": {
            "data_key": "intervals",
            "row_type": "interval"
        },
        "columns": [
            {
                "name": "onset_signal",
                "source_key": "signal",
                "row_pipeline_json": "{\"steps\": [{\"transform_name\": \"IntervalToEvent\", \"parameters\": {\"point\": \"start\"}}]}",
                "pipeline_json": "{\"steps\": [], \"range_reduction\": {\"reduction_name\": \"MeanValue\"}}"
            }
        ]
    })";

    auto const parsed = requireValue(parseDesignJson(json));
    REQUIRE(parsed.columns.size() == 1);
    REQUIRE(parsed.columns[0].column_name == "onset_signal");
    REQUIRE(parsed.columns[0].pipeline_json == kMeanValuePipelineJson);
    REQUIRE(parsed.columns[0].row_pipeline_json == expected_row_pipeline);
}

TEST_CASE("parseDesignJson parses TimeFrame row source", "[TensorDesign][Phase9a]") {
    std::string const json = R"({
        "tensor_key": "frame_features",
        "row_source": {
            "time_key": "frame",
            "row_type": "timeframe"
        },
        "columns": [
            {
                "name": "signal_value",
                "source_key": "signal",
                "pipeline_json": "{\"steps\": []}"
            }
        ]
    })";

    auto const parsed = requireValue(parseDesignJson(json));
    REQUIRE(parsed.row_type == DesignRowType::TimeFrame);
    REQUIRE(parsed.row_time_key == "frame");
    REQUIRE(parsed.row_source_key.empty());
}

TEST_CASE("parseDesignJson rejects unknown row_type", "[TensorDesign]") {
    std::string const json = R"({
        "row_source": {
            "data_key": "intervals",
            "row_type": "unknown"
        },
        "columns": []
    })";

    REQUIRE_FALSE(parseDesignJson(json).has_value());
}

TEST_CASE("parseDesignJson expands design authoring preset to raw spec", "[TensorDesign][design-presets]") {
    std::string const json = R"({
        "tensor_key": "features",
        "preset": "whisker_contact_feature_table",
        "parameters": {
            "row_source_key": "contacts",
            "curvature_source_key": "curvature",
            "spike_source_key": "spikes",
            "angle_source_key": "angle",
            "keypoint_source_keys": ["tip"],
            "onset_pre": 2,
            "onset_post": 3
        }
    })";

    auto const parsed = requireValue(parseDesignJson(json));
    REQUIRE(parsed.tensor_key == "features");
    REQUIRE(parsed.row_type == DesignRowType::Interval);
    REQUIRE(parsed.row_source_key == "contacts");
    REQUIRE(parsed.columns.size() == 6);
    CHECK(parsed.columns[0].column_name == "mean_curvature");
    CHECK(parsed.columns[1].column_name == "spike_count");
    CHECK(parsed.columns[2].column_name == "spike_presence_at_onset");
    CHECK(parsed.columns[3].column_name == "angle_at_onset");
    CHECK(parsed.columns[4].column_name == "tip_x");
    CHECK(parsed.columns[5].column_name == "tip_y");

    auto const serialized = serializeDesignJson(parsed);
    CHECK(serialized.find("whisker_contact_feature_table") == std::string::npos);
    CHECK(serialized.find("\"row_source\"") != std::string::npos);
    CHECK(serialized.find("\"columns\"") != std::string::npos);
}

TEST_CASE("buildTensor builds interval-row tensor from design spec", "[TensorDesign]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 100);
    auto analog = createLinearAnalog(100);
    dm.setData<AnalogTimeSeries>("signal", analog, TimeKey("time"));
    auto intervals = createIntervalSeries({{10, 20}, {50, 60}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("time"));

    TensorDesignSpec spec;
    spec.row_source_key = "intervals";
    spec.row_type = DesignRowType::Interval;
    spec.columns.push_back({
            .column_name = "mean_signal",
            .source_key = "signal",
            .pipeline_json = kMeanValuePipelineJson,
    });

    auto const built = requireValue(buildTensor(dm, spec));
    REQUIRE(built.numRows() == 2);
    REQUIRE(built.numColumns() == 1);

    auto const values = built.getColumn(0);
    CHECK_THAT(values[0], WithinAbs(15.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(55.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson counts events over full interval rows",
          "[TensorDesign][Phase1]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 100);
    auto events = createEventSeries({5, 15, 25, 35, 45, 55});
    dm.setData<DigitalEventSeries>("events", events, TimeKey("time"));
    auto intervals = createIntervalSeries({{0, 20}, {30, 50}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("time"));

    std::string const json = R"({
        "tensor_key": "event_features",
        "row_source": {
            "data_key": "intervals",
            "row_type": "interval"
        },
        "columns": [
            {
                "name": "event_count",
                "source_key": "events",
                "pipeline_json": "{\"steps\": [], \"range_reduction\": {\"reduction_name\": \"EventCount\"}}"
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == intervals->size());
    REQUIRE(built.numColumns() == 1);

    auto const values = built.getColumn(0);
    REQUIRE(values.size() == intervals->size());
    CHECK_THAT(values[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(2.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson empty row_pipeline_json preserves event count",
          "[TensorDesign][Phase3]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 100);
    auto events = createEventSeries({5, 15, 25, 35, 45, 55});
    dm.setData<DigitalEventSeries>("events", events, TimeKey("time"));
    auto intervals = createIntervalSeries({{0, 20}, {30, 50}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("time"));

    std::string const json = R"({
        "tensor_key": "event_features",
        "row_source": {
            "data_key": "intervals",
            "row_type": "interval"
        },
        "columns": [
            {
                "name": "event_count",
                "source_key": "events",
                "row_pipeline_json": "",
                "pipeline_json": "{\"steps\": [], \"range_reduction\": {\"reduction_name\": \"EventCount\"}}"
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == intervals->size());
    REQUIRE(built.numColumns() == 1);

    auto const values = built.getColumn(0);
    REQUIRE(values.size() == intervals->size());
    CHECK_THAT(values[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(2.0, 0.01));
}

TEST_CASE("buildTensor explicit identity row_pipeline_json preserves full-interval mean",
          "[TensorDesign][Phase3]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 100);
    auto analog = createLinearAnalog(100);
    dm.setData<AnalogTimeSeries>("signal", analog, TimeKey("time"));
    auto intervals = createIntervalSeries({{10, 20}, {50, 60}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("time"));

    TensorDesignSpec spec;
    spec.row_source_key = "intervals";
    spec.row_type = DesignRowType::Interval;
    spec.columns.push_back({
            .column_name = "mean_signal",
            .source_key = "signal",
            .pipeline_json = kMeanValuePipelineJson,
            .row_pipeline_json = kIdentityRowPipelineJson,
    });

    auto const built = requireValue(buildTensor(dm, spec));
    REQUIRE(built.numRows() == intervals->size());
    REQUIRE(built.numColumns() == 1);

    auto const values = built.getColumn(0);
    CHECK_THAT(values[0], WithinAbs(15.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(55.0, 0.01));
}

TEST_CASE("buildTensor converts interval-row windows across TimeFrames", "[TensorDesign][GatherResult]") {
    DataManager dm;
    REQUIRE(dm.setTime(TimeKey("source_time"), createIdentityTimeFrameForMax(100)));
    REQUIRE(dm.setTime(TimeKey("row_time"), createTimeFrameForRate(31, 2)));

    auto analog = createLinearAnalog(100);
    dm.setData<AnalogTimeSeries>("signal", analog, TimeKey("source_time"));
    auto intervals = createIntervalSeries({{5, 10}, {25, 30}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("row_time"));

    TensorDesignSpec spec;
    spec.row_source_key = "intervals";
    spec.row_type = DesignRowType::Interval;
    spec.columns.push_back({
            .column_name = "mean_signal",
            .source_key = "signal",
            .pipeline_json = kMeanValuePipelineJson,
    });

    auto const built = requireValue(buildTensor(dm, spec));
    REQUIRE(built.numRows() == 2);
    REQUIRE(built.numColumns() == 1);

    auto const values = built.getColumn(0);
    CHECK_THAT(values[0], WithinAbs(15.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(55.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson counts events across TimeFrames",
          "[TensorDesign][GatherResult][Phase1]") {
    DataManager dm;
    REQUIRE(dm.setTime(TimeKey("source_time"), createIdentityTimeFrameForMax(100)));
    REQUIRE(dm.setTime(TimeKey("row_time"), createTimeFrameForRate(31, 2)));

    auto events = createEventSeries({12, 18, 22, 55, 59, 61});
    dm.setData<DigitalEventSeries>("events", events, TimeKey("source_time"));
    auto intervals = createIntervalSeries({{5, 10}, {25, 30}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("row_time"));

    std::string const json = R"({
        "tensor_key": "cross_time_event_features",
        "row_source": {
            "data_key": "intervals",
            "row_type": "interval"
        },
        "columns": [
            {
                "name": "event_count",
                "source_key": "events",
                "pipeline_json": "{\"steps\": [], \"range_reduction\": {\"reduction_name\": \"EventCount\"}}"
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == intervals->size());
    REQUIRE(built.numColumns() == 1);

    auto const values = built.getColumn(0);
    REQUIRE(values.size() == intervals->size());
    CHECK_THAT(values[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(2.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson identity row_pipeline_json preserves cross-TimeFrame event count",
          "[TensorDesign][GatherResult][Phase3]") {
    DataManager dm;
    REQUIRE(dm.setTime(TimeKey("source_time"), createIdentityTimeFrameForMax(100)));
    REQUIRE(dm.setTime(TimeKey("row_time"), createTimeFrameForRate(31, 2)));

    auto events = createEventSeries({12, 18, 22, 55, 59, 61});
    dm.setData<DigitalEventSeries>("events", events, TimeKey("source_time"));
    auto intervals = createIntervalSeries({{5, 10}, {25, 30}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("row_time"));

    std::string const json = R"({
        "tensor_key": "cross_time_event_features",
        "row_source": {
            "data_key": "intervals",
            "row_type": "interval"
        },
        "columns": [
            {
                "name": "event_count",
                "source_key": "events",
                "row_pipeline_json": "{\"steps\": []}",
                "pipeline_json": "{\"steps\": [], \"range_reduction\": {\"reduction_name\": \"EventCount\"}}"
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == intervals->size());
    REQUIRE(built.numColumns() == 1);

    auto const values = built.getColumn(0);
    REQUIRE(values.size() == intervals->size());
    CHECK_THAT(values[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(2.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson counts events over row-pipeline onset windows",
          "[TensorDesign][Phase4]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 100);
    auto events = createEventSeries({9, 11, 12, 29, 31, 90});
    dm.setData<DigitalEventSeries>("events", events, TimeKey("time"));
    auto intervals = createIntervalSeries({{10, 20}, {30, 40}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("time"));

    std::string const json = R"({
        "tensor_key": "onset_event_features",
        "row_source": {
            "data_key": "intervals",
            "row_type": "interval"
        },
        "columns": [
            {
                "name": "onset_event_count",
                "source_key": "events",
                "row_pipeline_json": "{\"steps\": [{\"step_id\": \"interval_start\", \"transform_name\": \"IntervalToEvent\", \"parameters\": {\"point\": \"start\"}}, {\"step_id\": \"start_window\", \"transform_name\": \"EventToInterval\", \"parameters\": {\"pre_expansion\": 2, \"post_expansion\": 3}}]}",
                "pipeline_json": "{\"steps\": [], \"range_reduction\": {\"reduction_name\": \"EventCount\"}}"
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == intervals->size());
    REQUIRE(built.numColumns() == 1);

    auto const values = built.getColumn(0);
    REQUIRE(values.size() == intervals->size());
    CHECK_THAT(values[0], WithinAbs(3.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(2.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson mixes interval properties and row-pipeline windows",
          "[TensorDesign][Phase4]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 100);
    auto events = createEventSeries({9, 11, 12, 29, 31, 90});
    dm.setData<DigitalEventSeries>("events", events, TimeKey("time"));
    auto intervals = createIntervalSeries({{10, 20}, {30, 40}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("time"));

    std::string const json = R"({
        "tensor_key": "mixed_onset_features",
        "row_source": {
            "data_key": "intervals",
            "row_type": "interval"
        },
        "columns": [
            {
                "name": "contact_start",
                "interval_property": "start"
            },
            {
                "name": "onset_event_count",
                "source_key": "events",
                "row_pipeline_json": "{\"steps\": [{\"step_id\": \"interval_start\", \"transform_name\": \"IntervalToEvent\", \"parameters\": {\"point\": \"start\"}}, {\"step_id\": \"start_window\", \"transform_name\": \"EventToInterval\", \"parameters\": {\"pre_expansion\": 2, \"post_expansion\": 3}}]}",
                "pipeline_json": "{\"steps\": [], \"range_reduction\": {\"reduction_name\": \"EventCount\"}}"
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == intervals->size());
    REQUIRE(built.numColumns() == 2);

    auto const starts = built.getColumn(0);
    auto const event_counts = built.getColumn(1);
    CHECK_THAT(starts[0], WithinAbs(10.0, 0.01));
    CHECK_THAT(starts[1], WithinAbs(30.0, 0.01));
    CHECK_THAT(event_counts[0], WithinAbs(3.0, 0.01));
    CHECK_THAT(event_counts[1], WithinAbs(2.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson row-pipeline windows gather across TimeFrames",
          "[TensorDesign][GatherResult][Phase4]") {
    DataManager dm;
    REQUIRE(dm.setTime(TimeKey("source_time"), createIdentityTimeFrameForMax(100)));
    REQUIRE(dm.setTime(TimeKey("row_time"), createTimeFrameForRate(31, 2)));

    auto events = createEventSeries({8, 10, 12, 48, 50, 52, 90});
    dm.setData<DigitalEventSeries>("events", events, TimeKey("source_time"));
    auto intervals = createIntervalSeries({{5, 10}, {25, 30}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("row_time"));

    std::string const json = R"({
        "tensor_key": "cross_time_onset_event_features",
        "row_source": {
            "data_key": "intervals",
            "row_type": "interval"
        },
        "columns": [
            {
                "name": "onset_event_count",
                "source_key": "events",
                "row_pipeline_json": "{\"steps\": [{\"step_id\": \"interval_start\", \"transform_name\": \"IntervalToEvent\", \"parameters\": {\"point\": \"start\"}}, {\"step_id\": \"start_window\", \"transform_name\": \"EventToInterval\", \"parameters\": {\"pre_expansion\": 1, \"post_expansion\": 1}}]}",
                "pipeline_json": "{\"steps\": [], \"range_reduction\": {\"reduction_name\": \"EventCount\"}}"
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == intervals->size());
    REQUIRE(built.numColumns() == 1);

    auto const values = built.getColumn(0);
    REQUIRE(values.size() == intervals->size());
    CHECK_THAT(values[0], WithinAbs(3.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(3.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson builds interval_property columns",
          "[TensorDesign][Phase1]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 100);
    auto intervals = createIntervalSeries({{10, 30}, {50, 80}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("time"));

    std::string const json = R"({
        "tensor_key": "interval_properties",
        "row_source": {
            "data_key": "intervals",
            "row_type": "interval"
        },
        "columns": [
            {
                "name": "contact_start",
                "interval_property": "start"
            },
            {
                "name": "contact_duration",
                "interval_property": "duration"
            }
        ]
    })";

    auto const parsed = requireValue(parseDesignJson(json));
    REQUIRE(parsed.columns.size() == 2);
    REQUIRE(parsed.columns[0].interval_property == IntervalProperty::Start);
    REQUIRE(parsed.columns[1].interval_property == IntervalProperty::Duration);

    auto const built = requireValue(buildTensor(dm, parsed));
    REQUIRE(built.numRows() == intervals->size());
    REQUIRE(built.numColumns() == 2);

    auto const starts = built.getColumn(0);
    auto const durations = built.getColumn(1);
    CHECK_THAT(starts[0], WithinAbs(10.0, 0.01));
    CHECK_THAT(starts[1], WithinAbs(50.0, 0.01));
    CHECK_THAT(durations[0], WithinAbs(20.0, 0.01));
    CHECK_THAT(durations[1], WithinAbs(30.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson mixes interval properties and gathered columns",
          "[TensorDesign][Phase1]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 100);
    auto analog = createLinearAnalog(100);
    dm.setData<AnalogTimeSeries>("signal", analog, TimeKey("time"));
    auto events = createEventSeries({5, 15, 25, 35, 45, 55});
    dm.setData<DigitalEventSeries>("events", events, TimeKey("time"));
    auto intervals = createIntervalSeries({{0, 20}, {30, 50}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("time"));

    std::string const json = R"({
        "tensor_key": "mixed_interval_features",
        "row_source": {
            "data_key": "intervals",
            "row_type": "interval"
        },
        "columns": [
            {
                "name": "mean_signal",
                "source_key": "signal",
                "pipeline_json": "{\"steps\": [], \"range_reduction\": {\"reduction_name\": \"MeanValue\"}}"
            },
            {
                "name": "duration",
                "interval_property": "duration"
            },
            {
                "name": "event_count",
                "source_key": "events",
                "pipeline_json": "{\"steps\": [], \"range_reduction\": {\"reduction_name\": \"EventCount\"}}"
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == intervals->size());
    REQUIRE(built.numColumns() == 3);

    auto const mean_signal = built.getColumn(0);
    auto const duration = built.getColumn(1);
    auto const event_count = built.getColumn(2);
    CHECK_THAT(mean_signal[0], WithinAbs(10.0, 0.01));
    CHECK_THAT(mean_signal[1], WithinAbs(40.0, 0.01));
    CHECK_THAT(duration[0], WithinAbs(20.0, 0.01));
    CHECK_THAT(duration[1], WithinAbs(20.0, 0.01));
    CHECK_THAT(event_count[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(event_count[1], WithinAbs(2.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson identity row_pipeline_json preserves mixed interval columns",
          "[TensorDesign][Phase3]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 100);
    auto analog = createLinearAnalog(100);
    dm.setData<AnalogTimeSeries>("signal", analog, TimeKey("time"));
    auto events = createEventSeries({5, 15, 25, 35, 45, 55});
    dm.setData<DigitalEventSeries>("events", events, TimeKey("time"));
    auto intervals = createIntervalSeries({{0, 20}, {30, 50}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("time"));

    std::string const json = R"({
        "tensor_key": "mixed_interval_features",
        "row_source": {
            "data_key": "intervals",
            "row_type": "interval"
        },
        "columns": [
            {
                "name": "mean_signal",
                "source_key": "signal",
                "row_pipeline_json": "{\"steps\": []}",
                "pipeline_json": "{\"steps\": [], \"range_reduction\": {\"reduction_name\": \"MeanValue\"}}"
            },
            {
                "name": "duration",
                "interval_property": "duration"
            },
            {
                "name": "event_count",
                "source_key": "events",
                "row_pipeline_json": "",
                "pipeline_json": "{\"steps\": [], \"range_reduction\": {\"reduction_name\": \"EventCount\"}}"
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == intervals->size());
    REQUIRE(built.numColumns() == 3);

    auto const mean_signal = built.getColumn(0);
    auto const duration = built.getColumn(1);
    auto const event_count = built.getColumn(2);
    CHECK_THAT(mean_signal[0], WithinAbs(10.0, 0.01));
    CHECK_THAT(mean_signal[1], WithinAbs(40.0, 0.01));
    CHECK_THAT(duration[0], WithinAbs(20.0, 0.01));
    CHECK_THAT(duration[1], WithinAbs(20.0, 0.01));
    CHECK_THAT(event_count[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(event_count[1], WithinAbs(2.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson builds timestamp-row tensors",
          "[TensorDesign][Phase1]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 100);
    auto analog = createLinearAnalog(100);
    dm.setData<AnalogTimeSeries>("signal", analog, TimeKey("time"));
    auto row_events = createEventSeries({0, 10, 20, 50, 99});
    dm.setData<DigitalEventSeries>("row_events", row_events, TimeKey("time"));

    std::string const json = R"({
        "tensor_key": "timestamp_features",
        "row_source": {
            "data_key": "row_events",
            "row_type": "timestamp"
        },
        "columns": [
            {
                "name": "signal_at_event",
                "source_key": "signal"
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == row_events->size());
    REQUIRE(built.numColumns() == 1);

    auto const values = built.getColumn(0);
    REQUIRE(values.size() == row_events->size());
    CHECK_THAT(values[0], WithinAbs(0.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(10.0, 0.01));
    CHECK_THAT(values[2], WithinAbs(20.0, 0.01));
    CHECK_THAT(values[3], WithinAbs(50.0, 0.01));
    CHECK_THAT(values[4], WithinAbs(99.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson builds derived-from-source row tensors",
          "[TensorDesign][Phase1]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 10);
    auto analog = createLinearAnalog(6);
    dm.setData<AnalogTimeSeries>("signal", analog, TimeKey("time"));

    std::string const json = R"({
        "tensor_key": "derived_timestamp_features",
        "row_source": {
            "data_key": "signal",
            "row_type": "derived_from_source"
        },
        "columns": [
            {
                "name": "signal_at_own_times",
                "source_key": "signal"
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == 6);
    REQUIRE(built.numColumns() == 1);

    auto const values = built.getColumn(0);
    REQUIRE(values.size() == 6);
    CHECK_THAT(values[0], WithinAbs(0.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(1.0, 0.01));
    CHECK_THAT(values[2], WithinAbs(2.0, 0.01));
    CHECK_THAT(values[3], WithinAbs(3.0, 0.01));
    CHECK_THAT(values[4], WithinAbs(4.0, 0.01));
    CHECK_THAT(values[5], WithinAbs(5.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson builds TimeFrame row tensors",
          "[TensorDesign][Phase9a]") {
    DataManager dm;
    REQUIRE(dm.setTime(TimeKey("frame"), createTimeFrameFromTimes({0, 1, 2, 3, 4}), true));
    auto analog = createLinearAnalog(5);
    dm.setData<AnalogTimeSeries>("signal", analog, TimeKey("frame"));

    std::string const json = R"({
        "tensor_key": "frame_features",
        "row_source": {
            "time_key": "frame",
            "row_type": "timeframe"
        },
        "columns": [
            {
                "name": "signal_value",
                "source_key": "signal",
                "pipeline_json": "{\"steps\": []}"
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == 5);
    REQUIRE(built.numColumns() == 1);

    auto const values = built.getColumn(0);
    REQUIRE(values.size() == 5);
    CHECK_THAT(values[0], WithinAbs(0.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(1.0, 0.01));
    CHECK_THAT(values[2], WithinAbs(2.0, 0.01));
    CHECK_THAT(values[3], WithinAbs(3.0, 0.01));
    CHECK_THAT(values[4], WithinAbs(4.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson builds PointData x/y columns over TimeFrame rows",
          "[TensorDesign][Phase9b]") {
    DataManager dm;
    REQUIRE(dm.setTime(TimeKey("frame"), createTimeFrameFromTimes({0, 1, 2}), true));
    auto points = createPointData({
            {0, Point2D<float>{1.0f, 2.0f}},
            {1, Point2D<float>{3.0f, 4.0f}},
            {2, Point2D<float>{5.0f, 6.0f}},
    });
    dm.setData<PointData>("Nose", points, TimeKey("frame"));

    std::string const json = R"({
        "tensor_key": "point_features",
        "row_source": {
            "time_key": "frame",
            "row_type": "timeframe"
        },
        "columns": [
            {
                "name": "nose_x",
                "source_key": "Nose",
                "pipeline_json": "{\"steps\": [{\"step_id\": \"x\", \"transform_name\": \"PointCoordinate\", \"parameters\": {\"coordinate\": \"X\"}}]}"
            },
            {
                "name": "nose_y",
                "source_key": "Nose",
                "pipeline_json": "{\"steps\": [{\"step_id\": \"y\", \"transform_name\": \"PointCoordinate\", \"parameters\": {\"coordinate\": \"Y\"}}]}"
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == 3);
    REQUIRE(built.numColumns() == 2);

    auto const x = built.getColumn(0);
    auto const y = built.getColumn(1);
    REQUIRE(x.size() == 3);
    REQUIRE(y.size() == 3);
    CHECK_THAT(x[0], WithinAbs(1.0, 0.01));
    CHECK_THAT(x[1], WithinAbs(3.0, 0.01));
    CHECK_THAT(x[2], WithinAbs(5.0, 0.01));
    CHECK_THAT(y[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(y[1], WithinAbs(4.0, 0.01));
    CHECK_THAT(y[2], WithinAbs(6.0, 0.01));
}

TEST_CASE("parseDesignJson expands preset columns from JSON", "[TensorDesign][presets][Phase9c]") {
    std::string const json = R"({
        "tensor_key": "point_features",
        "row_source": {
            "time_key": "frame",
            "row_type": "timeframe"
        },
        "columns": [
            {
                "preset": "point_xy",
                "parameters": {
                    "source_key": "Nose",
                    "name_prefix": "nose"
                }
            },
            {
                "preset": "mean_value",
                "parameters": {
                    "output_name": "mean_signal",
                    "source_key": "signal"
                }
            }
        ]
    })";

    auto const parsed = requireValue(parseDesignJson(json));
    REQUIRE(parsed.columns.size() == 3);
    CHECK(parsed.columns[0].column_name == "nose_x");
    CHECK(parsed.columns[0].source_key == "Nose");
    CHECK(parsed.columns[0].pipeline_json.find("PointCoordinate") != std::string::npos);
    CHECK(parsed.columns[1].column_name == "nose_y");
    CHECK(parsed.columns[1].source_key == "Nose");
    CHECK(parsed.columns[1].pipeline_json.find("PointCoordinate") != std::string::npos);
    CHECK(parsed.columns[2].column_name == "mean_signal");
    CHECK(parsed.columns[2].source_key == "signal");
    CHECK(parsed.columns[2].pipeline_json == kMeanValuePipelineJson);
}

TEST_CASE("buildTensorFromDesignJson expands point_xy preset JSON",
          "[TensorDesign][presets][Phase9c]") {
    DataManager dm;
    REQUIRE(dm.setTime(TimeKey("frame"), createTimeFrameFromTimes({0, 1, 2}), true));
    auto points = createPointData({
            {0, Point2D<float>{1.0f, 2.0f}},
            {1, Point2D<float>{3.0f, 4.0f}},
            {2, Point2D<float>{5.0f, 6.0f}},
    });
    dm.setData<PointData>("Nose", points, TimeKey("frame"));

    std::string const json = R"({
        "tensor_key": "point_features",
        "row_source": {
            "time_key": "frame",
            "row_type": "timeframe"
        },
        "columns": [
            {
                "preset": "point_xy",
                "parameters": {
                    "source_key": "Nose",
                    "name_prefix": "nose"
                }
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == 3);
    REQUIRE(built.numColumns() == 2);

    auto const x = built.getColumn(0);
    auto const y = built.getColumn(1);
    REQUIRE(x.size() == 3);
    REQUIRE(y.size() == 3);
    CHECK_THAT(x[0], WithinAbs(1.0, 0.01));
    CHECK_THAT(x[1], WithinAbs(3.0, 0.01));
    CHECK_THAT(x[2], WithinAbs(5.0, 0.01));
    CHECK_THAT(y[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(y[1], WithinAbs(4.0, 0.01));
    CHECK_THAT(y[2], WithinAbs(6.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson expands multi_point_xy preset JSON",
          "[TensorDesign][presets][Phase9c]") {
    DataManager dm;
    REQUIRE(dm.setTime(TimeKey("frame"), createTimeFrameFromTimes({0, 1, 2}), true));
    auto nose = createPointData({
            {0, Point2D<float>{1.0f, 2.0f}},
            {1, Point2D<float>{3.0f, 4.0f}},
            {2, Point2D<float>{5.0f, 6.0f}},
    });
    auto paw = createPointData({
            {0, Point2D<float>{10.0f, 20.0f}},
            {1, Point2D<float>{30.0f, 40.0f}},
            {2, Point2D<float>{50.0f, 60.0f}},
    });
    dm.setData<PointData>("Nose", nose, TimeKey("frame"));
    dm.setData<PointData>("Paw", paw, TimeKey("frame"));

    std::string const json = R"({
        "tensor_key": "point_features",
        "row_source": {
            "time_key": "frame",
            "row_type": "timeframe"
        },
        "columns": [
            {
                "preset": "multi_point_xy",
                "parameters": {
                    "source_keys": ["Nose", "Paw"]
                }
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == 3);
    REQUIRE(built.numColumns() == 4);

    auto const nose_x = built.getColumn(0);
    auto const nose_y = built.getColumn(1);
    auto const paw_x = built.getColumn(2);
    auto const paw_y = built.getColumn(3);
    CHECK_THAT(nose_x[0], WithinAbs(1.0, 0.01));
    CHECK_THAT(nose_x[1], WithinAbs(3.0, 0.01));
    CHECK_THAT(nose_x[2], WithinAbs(5.0, 0.01));
    CHECK_THAT(nose_y[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(nose_y[1], WithinAbs(4.0, 0.01));
    CHECK_THAT(nose_y[2], WithinAbs(6.0, 0.01));
    CHECK_THAT(paw_x[0], WithinAbs(10.0, 0.01));
    CHECK_THAT(paw_x[1], WithinAbs(30.0, 0.01));
    CHECK_THAT(paw_x[2], WithinAbs(50.0, 0.01));
    CHECK_THAT(paw_y[0], WithinAbs(20.0, 0.01));
    CHECK_THAT(paw_y[1], WithinAbs(40.0, 0.01));
    CHECK_THAT(paw_y[2], WithinAbs(60.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson handles stitched point_xy at interval_start JSON",
          "[TensorDesign][presets][Phase9c]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 12);
    auto points = createPointData({
            {2, Point2D<float>{1.0f, 2.0f}},
            {7, Point2D<float>{3.0f, 4.0f}},
            {9, Point2D<float>{5.0f, 6.0f}},
    });
    dm.setData<PointData>("Nose", points, TimeKey("time"));
    auto managed_points = dm.getData<PointData>("Nose");
    REQUIRE(managed_points != nullptr);
    REQUIRE(managed_points->getTimeFrame() != nullptr);
    auto intervals = createIntervalSeries({{2, 4}, {7, 8}, {9, 10}});
    dm.setData<DigitalIntervalSeries>("Contact", intervals, TimeKey("time"));

    std::string const json = R"({
        "tensor_key": "contact_point_features",
        "row_source": {
            "data_key": "Contact",
            "row_type": "interval"
        },
        "columns": [
            {
                "preset": "point_xy",
                "row_modifier": "interval_start",
                "parameters": {
                    "source_key": "Nose",
                    "name_prefix": "nose"
                }
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == intervals->size());
    REQUIRE(built.numColumns() == 2);

    auto const x = built.getColumn(0);
    auto const y = built.getColumn(1);
    CHECK_THAT(x[0], WithinAbs(1.0, 0.01));
    CHECK_THAT(x[1], WithinAbs(3.0, 0.01));
    CHECK_THAT(x[2], WithinAbs(5.0, 0.01));
    CHECK_THAT(y[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(y[1], WithinAbs(4.0, 0.01));
    CHECK_THAT(y[2], WithinAbs(6.0, 0.01));
}

TEST_CASE("point_xy preset expansion builds PointData x/y columns over TimeFrame rows",
          "[TensorDesign][presets][Phase9c]") {
    DataManager dm;
    REQUIRE(dm.setTime(TimeKey("frame"), createTimeFrameFromTimes({0, 1, 2}), true));
    auto points = createPointData({
            {0, Point2D<float>{1.0f, 2.0f}},
            {1, Point2D<float>{3.0f, 4.0f}},
            {2, Point2D<float>{5.0f, 6.0f}},
    });
    dm.setData<PointData>("Nose", points, TimeKey("frame"));

    auto registry = createBuiltInColumnRecipePresetRegistry();
    auto expansion = requireValue(registry.expand(
            "point_xy",
            ColumnRecipePresetArgs{
                    .source_key = "Nose",
                    .name_prefix = "nose"}));

    TensorDesignSpec spec;
    spec.tensor_key = "point_features";
    spec.row_time_key = "frame";
    spec.row_type = DesignRowType::TimeFrame;
    spec.columns = std::move(expansion.columns);

    auto const built = requireValue(buildTensorFromDesignJson(dm, serializeDesignJson(spec)));
    REQUIRE(built.numRows() == 3);
    REQUIRE(built.numColumns() == 2);

    auto const x = built.getColumn(0);
    auto const y = built.getColumn(1);
    REQUIRE(x.size() == 3);
    REQUIRE(y.size() == 3);
    CHECK_THAT(x[0], WithinAbs(1.0, 0.01));
    CHECK_THAT(x[1], WithinAbs(3.0, 0.01));
    CHECK_THAT(x[2], WithinAbs(5.0, 0.01));
    CHECK_THAT(y[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(y[1], WithinAbs(4.0, 0.01));
    CHECK_THAT(y[2], WithinAbs(6.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson handles stitched analog_sample at interval_start JSON",
          "[TensorDesign][presets][Phase9c]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 12);
    auto analog = createLinearAnalog(12);
    dm.setData<AnalogTimeSeries>("signal", analog, TimeKey("time"));
    auto intervals = createIntervalSeries({{2, 4}, {7, 8}, {9, 10}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("time"));

    std::string const saved_design_json = R"({
        "tensor_key": "onset_features",
        "row_source": {
            "data_key": "intervals",
            "row_type": "interval"
        },
        "columns": [
            {
                "preset": "analog_sample",
                "row_modifier": "interval_start",
                "parameters": {
                    "output_name": "signal_at_onset",
                    "source_key": "signal"
                }
            }
        ]
    })";

    auto const parsed = requireValue(parseDesignJson(saved_design_json));
    REQUIRE(parsed.columns.size() == 1);
    auto const & saved = parsed.columns.front();
    CHECK(saved.column_name == "signal_at_onset");
    CHECK(saved.source_key == "signal");
    CHECK(saved.row_pipeline_json == "{\"steps\": [{\"step_id\": \"interval_start\", \"transform_name\": \"IntervalToEvent\", \"parameters\": {\"point\": \"start\"}}]}");
    CHECK(saved.pipeline_json == "{\"steps\": []}");

    auto const built = requireValue(buildTensorFromDesignJson(dm, saved_design_json));
    REQUIRE(built.numRows() == intervals->size());
    REQUIRE(built.numColumns() == 1);

    auto const values = built.getColumn(0);
    REQUIRE(values.size() == intervals->size());
    CHECK_THAT(values[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(7.0, 0.01));
    CHECK_THAT(values[2], WithinAbs(9.0, 0.01));
}

TEST_CASE("mean_value preset expansion builds tensor from expanded JSON",
          "[TensorDesign][presets][Phase9c]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 10);
    auto analog = createLinearAnalog(10);
    dm.setData<AnalogTimeSeries>("Curvature", analog, TimeKey("time"));
    auto intervals = createIntervalSeries({{0, 3}, {4, 6}});
    dm.setData<DigitalIntervalSeries>("Contact", intervals, TimeKey("time"));

    auto registry = createBuiltInColumnRecipePresetRegistry();
    auto expansion = requireValue(registry.expand(
            "mean_value",
            ColumnRecipePresetArgs{
                    .output_name = "mean_curvature",
                    .source_key = "Curvature"}));

    TensorDesignSpec spec;
    spec.tensor_key = "contact_features";
    spec.row_source_key = "Contact";
    spec.row_type = DesignRowType::Interval;
    spec.columns = std::move(expansion.columns);

    auto const built = requireValue(buildTensorFromDesignJson(dm, serializeDesignJson(spec)));
    REQUIRE(built.numRows() == 2);
    REQUIRE(built.numColumns() == 1);

    auto const mean = built.getColumn(0);
    REQUIRE(mean.size() == 2);
    CHECK_THAT(mean[0], WithinAbs(1.5, 0.01));
    CHECK_THAT(mean[1], WithinAbs(5.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson leaves NaN for missing TimeFrame samples",
          "[TensorDesign][Phase9a]") {
    DataManager dm;
    REQUIRE(dm.setTime(TimeKey("frame"), createTimeFrameFromTimes({0, 1, 2, 3, 4}), true));
    auto analog = std::make_shared<AnalogTimeSeries>(
            std::vector<float>{10.0f, 30.0f},
            std::vector<TimeFrameIndex>{TimeFrameIndex(1), TimeFrameIndex(3)});
    dm.setData<AnalogTimeSeries>("signal", analog, TimeKey("frame"));

    std::string const json = R"({
        "tensor_key": "sparse_features",
        "row_source": {
            "time_key": "frame",
            "row_type": "timeframe"
        },
        "columns": [
            {
                "name": "signal_value",
                "source_key": "signal",
                "pipeline_json": "{\"steps\": []}"
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    auto const values = built.getColumn(0);
    REQUIRE(values.size() == 5);
    CHECK(std::isnan(values[0]));
    CHECK_THAT(values[1], WithinAbs(10.0, 0.01));
    CHECK(std::isnan(values[2]));
    CHECK_THAT(values[3], WithinAbs(30.0, 0.01));
    CHECK(std::isnan(values[4]));
}

TEST_CASE("buildTensorFromDesignJson samples analog source from DigitalEventSeries row_pipeline_json",
          "[TensorDesign][Phase6]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 12);
    auto analog = createLinearAnalog(12);
    dm.setData<AnalogTimeSeries>("signal", analog, TimeKey("time"));
    auto intervals = createIntervalSeries({{2, 4}, {7, 8}, {9, 10}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("time"));

    std::string const json = R"({
        "tensor_key": "offset_features",
        "row_source": {
            "data_key": "intervals",
            "row_type": "interval"
        },
        "columns": [
            {
                "name": "signal_plus_two",
                "source_key": "signal",
                "row_pipeline_json": "{\"steps\": [{\"step_id\": \"start\", \"transform_name\": \"IntervalToEvent\", \"parameters\": {\"point\": \"start\"}}, {\"step_id\": \"shift\", \"transform_name\": \"ShiftDigitalEventSeries\", \"parameters\": {\"offset\": 2}}]}",
                "pipeline_json": "{\"steps\": []}"
            },
            {
                "name": "signal_no_offset",
                "source_key": "signal",
                "row_pipeline_json": "{\"steps\": [{\"step_id\": \"start\", \"transform_name\": \"IntervalToEvent\", \"parameters\": {\"point\": \"start\"}}]}",
                "pipeline_json": "{\"steps\": []}"
            },
            {
                "name": "signal_out_of_range",
                "source_key": "signal",
                "row_pipeline_json": "{\"steps\": [{\"step_id\": \"start\", \"transform_name\": \"IntervalToEvent\", \"parameters\": {\"point\": \"start\"}}, {\"step_id\": \"shift\", \"transform_name\": \"ShiftDigitalEventSeries\", \"parameters\": {\"offset\": 20}}]}",
                "pipeline_json": "{\"steps\": []}"
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == intervals->size());
    REQUIRE(built.numColumns() == 3);

    auto const plus_two = built.getColumn(0);
    auto const no_offset = built.getColumn(1);
    auto const out_of_range = built.getColumn(2);
    CHECK_THAT(plus_two[0], WithinAbs(4.0, 0.01));
    CHECK_THAT(plus_two[1], WithinAbs(9.0, 0.01));
    CHECK_THAT(plus_two[2], WithinAbs(11.0, 0.01));
    CHECK_THAT(no_offset[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(no_offset[1], WithinAbs(7.0, 0.01));
    CHECK_THAT(no_offset[2], WithinAbs(9.0, 0.01));
    CHECK(std::isnan(out_of_range[0]));
    CHECK(std::isnan(out_of_range[1]));
    CHECK(std::isnan(out_of_range[2]));
}

TEST_CASE("buildTensor fails when columns are empty", "[TensorDesign]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 5);
    auto intervals = createIntervalSeries({{0, 5}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("time"));

    TensorDesignSpec spec;
    spec.row_source_key = "intervals";
    spec.row_type = DesignRowType::Interval;

    REQUIRE_FALSE(buildTensor(dm, spec).has_value());
}

TEST_CASE("buildTensor rejects ordinal row type", "[TensorDesign]") {
    DataManager dm;

    TensorDesignSpec spec;
    spec.row_source_key = "signal";
    spec.row_type = DesignRowType::Ordinal;
    spec.columns.push_back({
            .column_name = "mean_signal",
            .source_key = "signal",
            .pipeline_json = kMeanValuePipelineJson,
    });

    REQUIRE_FALSE(buildTensor(dm, spec).has_value());
}

TEST_CASE("populateDataManager requires tensor_key", "[TensorDesign]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 20);
    auto analog = createLinearAnalog(20);
    dm.setData<AnalogTimeSeries>("signal", analog, TimeKey("time"));
    auto intervals = createIntervalSeries({{0, 5}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("time"));

    TensorDesignSpec spec;
    spec.row_source_key = "intervals";
    spec.row_type = DesignRowType::Interval;
    spec.columns.push_back({
            .column_name = "mean_signal",
            .source_key = "signal",
            .pipeline_json = kMeanValuePipelineJson,
    });

    REQUIRE_FALSE(populateDataManager(dm, spec));
}

TEST_CASE("populateDataManager registers tensor in DataManager", "[TensorDesign]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 20);
    auto analog = createLinearAnalog(20);
    dm.setData<AnalogTimeSeries>("signal", analog, TimeKey("time"));
    auto intervals = createIntervalSeries({{0, 5}});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("time"));

    TensorDesignSpec spec;
    spec.tensor_key = "designed";
    spec.row_source_key = "intervals";
    spec.row_type = DesignRowType::Interval;
    spec.columns.push_back({
            .column_name = "mean_signal",
            .source_key = "signal",
            .pipeline_json = kMeanValuePipelineJson,
    });

    REQUIRE(populateDataManager(dm, spec));
    auto const tensor = dm.getData<TensorData>("designed");
    REQUIRE(tensor != nullptr);
    REQUIRE(tensor->numRows() == 1);
}

TEST_CASE("serializeDesignJson round-trips through parseDesignJson", "[TensorDesign]") {
    std::string const row_pipeline_json =
            R"({"steps": [{"transform_name": "IntervalToEvent", "parameters": {"point": "start"}}]})";
    TensorDesignSpec original;
    original.tensor_key = "features";
    original.output_time_key = "time";
    original.row_source_key = "intervals";
    original.row_type = DesignRowType::Interval;
    original.columns.push_back({
            .column_name = "mean_signal",
            .source_key = "signal",
            .pipeline_json = kMeanValuePipelineJson,
            .row_pipeline_json = row_pipeline_json,
    });
    original.columns.push_back({
            .column_name = "contact_duration",
            .interval_property = IntervalProperty::Duration,
    });

    auto const json = serializeDesignJson(original);
    auto const roundtrip = requireValue(parseDesignJson(json));
    REQUIRE(roundtrip.tensor_key == original.tensor_key);
    REQUIRE(roundtrip.output_time_key == original.output_time_key);
    REQUIRE(roundtrip.row_source_key == original.row_source_key);
    REQUIRE(roundtrip.row_type == original.row_type);
    REQUIRE(roundtrip.columns.size() == 2);
    REQUIRE(roundtrip.columns[0].column_name == original.columns[0].column_name);
    REQUIRE(roundtrip.columns[0].row_pipeline_json == row_pipeline_json);
    REQUIRE(roundtrip.columns[1].column_name == original.columns[1].column_name);
    REQUIRE(roundtrip.columns[1].row_pipeline_json.empty());
    REQUIRE(roundtrip.columns[1].interval_property == IntervalProperty::Duration);
}

TEST_CASE("serializeDesignJson omits empty row_pipeline_json", "[TensorDesign][Phase2]") {
    TensorDesignSpec original;
    original.tensor_key = "features";
    original.row_source_key = "intervals";
    original.row_type = DesignRowType::Interval;
    original.columns.push_back({
            .column_name = "mean_signal",
            .source_key = "signal",
            .pipeline_json = kMeanValuePipelineJson,
    });

    auto const json = serializeDesignJson(original);
    REQUIRE(json.find("row_pipeline_json") == std::string::npos);

    auto const roundtrip = requireValue(parseDesignJson(json));
    REQUIRE(roundtrip.columns.size() == 1);
    REQUIRE(roundtrip.columns[0].row_pipeline_json.empty());
}

TEST_CASE("serializeDesignJson round-trips TimeFrame row source", "[TensorDesign][Phase9a]") {
    TensorDesignSpec original;
    original.tensor_key = "frame_features";
    original.row_time_key = "frame";
    original.row_type = DesignRowType::TimeFrame;
    original.columns.push_back({
            .column_name = "signal_value",
            .source_key = "signal",
            .pipeline_json = R"({"steps": []})",
    });

    auto const json = serializeDesignJson(original);
    REQUIRE(json.find("time_key") != std::string::npos);
    REQUIRE(json.find("data_key") == std::string::npos);

    auto const roundtrip = requireValue(parseDesignJson(json));
    REQUIRE(roundtrip.row_type == DesignRowType::TimeFrame);
    REQUIRE(roundtrip.row_time_key == "frame");
    REQUIRE(roundtrip.row_source_key.empty());
}

TEST_CASE("parseDesignJson parses pipeline_value_bindings", "[TensorDesign][Phase5]") {
    std::string const json = R"({
        "tensor_key": "features",
        "row_source": {"data_key": "intervals", "row_type": "interval"},
        "columns": [
            {
                "name": "relative_spikes",
                "source_key": "spikes",
                "pipeline_value_bindings": [
                    {
                        "source_key": "contacts",
                        "source_pipeline_json": "{\"steps\": [{\"step_id\": \"start\", \"transform_name\": \"IntervalToEvent\", \"parameters\": {\"point\": \"start\"}}]}",
                        "store_key": "row_alignment_time"
                    }
                ],
                "pipeline_json": "{\"steps\": []}"
            }
        ]
    })";

    auto const parsed = requireValue(parseDesignJson(json));
    REQUIRE(parsed.columns.size() == 1);
    REQUIRE(parsed.columns[0].pipeline_value_bindings.size() == 1);
    auto const & binding = parsed.columns[0].pipeline_value_bindings[0];
    REQUIRE(binding.source_key == "contacts");
    REQUIRE(binding.store_key == "row_alignment_time");
    REQUIRE(binding.source_pipeline_json.find("IntervalToEvent") != std::string::npos);
}

TEST_CASE("buildTensorFromDesignJson applies derived pipeline_value_bindings",
          "[TensorDesign][Phase5]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 1000);

    auto intervals = createIntervalSeries({{100, 120}, {200, 230}});
    auto contacts = createIntervalSeries({{100, 120}, {200, 230}});
    auto spikes = createEventSeries({95, 105, 112, 190, 205, 220});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("time"));
    dm.setData<DigitalIntervalSeries>("contacts", contacts, TimeKey("time"));
    dm.setData<DigitalEventSeries>("spikes", spikes, TimeKey("time"));

    std::string const json = R"({
        "tensor_key": "features",
        "row_source": {"data_key": "intervals", "row_type": "interval"},
        "columns": [
            {
                "name": "relative_spikes",
                "source_key": "spikes",
                "pipeline_value_bindings": [
                    {
                        "source_key": "contacts",
                        "source_pipeline_json": "{\"steps\": [{\"step_id\": \"start\", \"transform_name\": \"IntervalToEvent\", \"parameters\": {\"point\": \"start\"}}]}",
                        "store_key": "row_alignment_time"
                    }
                ],
                "pipeline_json": "{\"steps\": [{\"step_id\": \"normalize\", \"transform_name\": \"NormalizeDigitalEventSeriesRelative\", \"parameters\": {\"alignment_time\": 0}, \"param_bindings\": {\"alignment_time\": \"row_alignment_time\"}}], \"range_reduction\": {\"reduction_name\": \"EventCountInWindow\", \"parameters\": {\"window_start\": 0.0, \"window_end\": 15.0}}}"
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    auto const values = built.getColumn(0);

    REQUIRE(values.size() == intervals->size());
    CHECK_THAT(values[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(1.0, 0.01));
}

TEST_CASE("buildTensorFromDesignJson handles stitched trial_relative_event_count at interval_start JSON",
          "[TensorDesign][presets][Phase9c]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 1000);

    auto intervals = createIntervalSeries({{100, 120}, {200, 230}});
    auto contacts = createIntervalSeries({{100, 120}, {200, 230}});
    auto spikes = createEventSeries({95, 105, 112, 190, 205, 220});
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("time"));
    dm.setData<DigitalIntervalSeries>("contacts", contacts, TimeKey("time"));
    dm.setData<DigitalEventSeries>("spikes", spikes, TimeKey("time"));

    std::string const json = R"({
        "tensor_key": "features",
        "row_source": {"data_key": "intervals", "row_type": "interval"},
        "columns": [
            {
                "preset": "trial_relative_event_count",
                "row_modifier": "bind_interval_start",
                "parameters": {
                    "output_name": "relative_spikes",
                    "source_key": "spikes",
                    "binding_source_key": "contacts",
                    "store_key": "row_alignment_time",
                    "window_start": 0.0,
                    "window_end": 15.0
                }
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    auto const values = built.getColumn(0);

    REQUIRE(values.size() == intervals->size());
    CHECK_THAT(values[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(1.0, 0.01));
}

TEST_CASE("serializeDesignJson round-trips pipeline_value_bindings", "[TensorDesign][Phase5]") {
    TensorDesignSpec original;
    original.tensor_key = "features";
    original.row_source_key = "intervals";
    original.row_type = DesignRowType::Interval;
    Neuralyzer::TensorBuilders::ColumnRecipe recipe;
    recipe.column_name = "relative_spikes";
    recipe.source_key = "spikes";
    recipe.pipeline_json = R"({"steps": []})";
    recipe.pipeline_value_bindings.push_back(Neuralyzer::TensorBuilders::PipelineValueBindingRecipe{
            .source_key = "contacts",
            .source_pipeline_json = R"({"steps": [{"step_id": "start", "transform_name": "IntervalToEvent", "parameters": {"point": "start"}}]})",
            .store_key = "row_alignment_time"});
    original.columns.push_back(std::move(recipe));

    auto const json = serializeDesignJson(original);
    REQUIRE(json.find("pipeline_value_bindings") != std::string::npos);

    auto const roundtrip = requireValue(parseDesignJson(json));
    REQUIRE(roundtrip.columns.size() == 1);
    REQUIRE(roundtrip.columns[0].pipeline_value_bindings.size() == 1);
    REQUIRE(roundtrip.columns[0].pipeline_value_bindings[0].source_key == "contacts");
    REQUIRE(roundtrip.columns[0].pipeline_value_bindings[0].store_key == "row_alignment_time");
    REQUIRE(roundtrip.columns[0].pipeline_value_bindings[0].source_pipeline_json.find("IntervalToEvent") != std::string::npos);
}
