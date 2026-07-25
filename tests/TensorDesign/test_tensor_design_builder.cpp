/**
 * @file test_tensor_design_builder.cpp
 * @brief Unit tests for the Qt-free TensorDesign library.
 */

#include "TensorDesign/TensorDesignBuilder.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Tensors/TensorData.hpp"

#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

using DesignRowType = Neuralyzer::TensorDesign::RowType;
using Catch::Matchers::WithinAbs;
using Neuralyzer::TensorDesign::buildTensor;
using Neuralyzer::TensorDesign::parseDesignJson;
using Neuralyzer::TensorDesign::populateDataManager;
using Neuralyzer::TensorDesign::serializeDesignJson;
using Neuralyzer::TensorDesign::TensorDesignSpec;

namespace {

constexpr char const * kMeanValuePipelineJson =
        R"({"steps": [], "range_reduction": {"reduction_name": "MeanValue"}})";

template <typename T>
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

std::shared_ptr<DigitalIntervalSeries> createIntervalSeries(
        std::vector<std::pair<int64_t, int64_t>> const & intervals) {
    std::vector<Interval> vec;
    vec.reserve(intervals.size());
    for (auto const & [start, end]: intervals) {
        vec.emplace_back(Interval{start, end});
    }
    return std::make_shared<DigitalIntervalSeries>(vec);
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

TEST_CASE("buildTensor fails when columns are empty", "[TensorDesign]") {
    DataManager dm;
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
    TensorDesignSpec original;
    original.tensor_key = "features";
    original.output_time_key = "time";
    original.row_source_key = "intervals";
    original.row_type = DesignRowType::Interval;
    original.columns.push_back({
            .column_name = "mean_signal",
            .source_key = "signal",
            .pipeline_json = kMeanValuePipelineJson,
    });

    auto const json = serializeDesignJson(original);
    auto const roundtrip = requireValue(parseDesignJson(json));
    REQUIRE(roundtrip.tensor_key == original.tensor_key);
    REQUIRE(roundtrip.output_time_key == original.output_time_key);
    REQUIRE(roundtrip.row_source_key == original.row_source_key);
    REQUIRE(roundtrip.row_type == original.row_type);
    REQUIRE(roundtrip.columns.size() == 1);
    REQUIRE(roundtrip.columns[0].column_name == original.columns[0].column_name);
}
