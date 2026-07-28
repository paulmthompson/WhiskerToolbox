/**
 * @file test_tensor_design_builder.cpp
 * @brief Unit tests for the Qt-free TensorDesign library.
 */

#include "TensorDesign/TensorDesignBuilder.hpp"

#include "../fixtures/GatherAlignmentFixtures.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
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
    std::vector<Interval> vec;
    vec.reserve(intervals.size());
    for (auto const & [start, end]: intervals) {
        vec.emplace_back(Interval{start, end});
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

TEST_CASE("buildTensorFromDesignJson characterizes timestamp offset sampling",
          "[TensorDesign][offset][characterization][Phase1]") {
    DataManager dm;
    setDefaultIdentityTimeFrame(dm, 12);
    auto analog = createLinearAnalog(12);
    dm.setData<AnalogTimeSeries>("signal", analog, TimeKey("time"));
    auto row_events = createEventSeries({2, 7, 9});
    dm.setData<DigitalEventSeries>("row_events", row_events, TimeKey("time"));

    std::string const json = R"({
        "tensor_key": "offset_features",
        "row_source": {
            "data_key": "row_events",
            "row_type": "timestamp"
        },
        "columns": [
            {
                "name": "signal_plus_two",
                "source_key": "signal",
                "pipeline_json": "{\"offset\": 2}"
            },
            {
                "name": "signal_no_offset",
                "source_key": "signal",
                "pipeline_json": "{\"offset\": 0}"
            },
            {
                "name": "signal_out_of_range",
                "source_key": "signal",
                "pipeline_json": "{\"offset\": 20}"
            }
        ]
    })";

    auto const built = requireValue(buildTensorFromDesignJson(dm, json));
    REQUIRE(built.numRows() == row_events->size());
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
