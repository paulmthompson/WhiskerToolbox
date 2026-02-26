/**
 * @file BuildScatterPoints.test.cpp
 * @brief Tests for scatter point pair computation
 *
 * Verifies that buildScatterPoints correctly pairs data from
 * AnalogTimeSeries and TensorData sources with various row types
 * and temporal offsets.
 *
 * @see BuildScatterPoints.hpp for the factory function
 * @see ScatterPointData for the result structure
 */

#include "Core/BuildScatterPoints.hpp"
#include "Core/ScatterAxisSource.hpp"
#include "Core/ScatterPointData.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/interval_data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstddef>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

// =============================================================================
// Helpers
// =============================================================================

namespace {

std::shared_ptr<TimeFrame> makeTimeFrame(std::size_t size)
{
    std::vector<int> ts(size);
    for (std::size_t i = 0; i < size; ++i) {
        ts[i] = static_cast<int>(i);
    }
    return std::make_shared<TimeFrame>(ts);
}

std::shared_ptr<TimeIndexStorage> makeDenseTimeStorage(std::size_t count)
{
    return TimeIndexStorageFactory::createDenseFromZero(count);
}

std::shared_ptr<DataManager> makeDataManager()
{
    auto dm = std::make_shared<DataManager>();
    auto tf = makeTimeFrame(1000);
    dm->setTime(TimeKey("time"), tf);
    return dm;
}

/// Add an AnalogTimeSeries with specific values
void addAnalogWithValues(DataManager & dm, std::string const & key,
                         std::vector<float> values)
{
    auto count = values.size();
    auto ats = std::make_shared<AnalogTimeSeries>(values, count);
    ats->setTimeFrame(dm.getTime(TimeKey("time")));
    dm.setData<AnalogTimeSeries>(key, ats, TimeKey("time"));
}

/// Add a TensorData with TimeFrameIndex rows and specific column data
void addTensorTFIWithValues(DataManager & dm, std::string const & key,
                            std::vector<float> col_data,
                            std::vector<std::string> col_names = {})
{
    auto num_rows = col_data.size();
    std::size_t num_cols = 1;
    auto ts = makeDenseTimeStorage(num_rows);
    auto tf = dm.getTime(TimeKey("time"));
    auto tensor = std::make_shared<TensorData>(
        TensorData::createTimeSeries2D(col_data, num_rows, num_cols, ts, tf, col_names));
    dm.setData<TensorData>(key, tensor, TimeKey("time"));
}

/// Add a TensorData with Ordinal rows and specific column data
void addTensorOrdinalWithValues(DataManager & dm, std::string const & key,
                                std::vector<float> col_data)
{
    auto num_rows = col_data.size();
    std::vector<AxisDescriptor> axes = {
        {"row", num_rows},
        {"col", 1}
    };
    auto tensor = std::make_shared<TensorData>(
        TensorData::createND(col_data, axes));
    dm.setData<TensorData>(key, tensor, TimeKey("time"));
}

}  // namespace

// =============================================================================
// AnalogTimeSeries × AnalogTimeSeries
// =============================================================================

TEST_CASE("buildScatterPoints: ATS x ATS basic pairing",
          "[ScatterPlot][BuildScatterPoints]")
{
    auto dm = makeDataManager();

    std::vector<float> x_vals = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    std::vector<float> y_vals = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};

    addAnalogWithValues(*dm, "x_ats", x_vals);
    addAnalogWithValues(*dm, "y_ats", y_vals);

    ScatterAxisSource x_src{.data_key = "x_ats"};
    ScatterAxisSource y_src{.data_key = "y_ats"};

    auto result = buildScatterPoints(*dm, x_src, y_src);

    REQUIRE(result.size() == 5);
    CHECK(result.x_values[0] == 1.0f);
    CHECK(result.x_values[4] == 5.0f);
    CHECK(result.y_values[0] == 10.0f);
    CHECK(result.y_values[4] == 50.0f);
    CHECK(result.time_indices[0] == TimeFrameIndex(0));
    CHECK(result.time_indices[4] == TimeFrameIndex(4));
}

TEST_CASE("buildScatterPoints: ATS x ATS with different lengths uses intersection",
          "[ScatterPlot][BuildScatterPoints]")
{
    auto dm = makeDataManager();

    std::vector<float> x_vals = {1.0f, 2.0f, 3.0f};
    std::vector<float> y_vals = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};

    addAnalogWithValues(*dm, "x_short", x_vals);
    addAnalogWithValues(*dm, "y_long", y_vals);

    ScatterAxisSource x_src{.data_key = "x_short"};
    ScatterAxisSource y_src{.data_key = "y_long"};

    auto result = buildScatterPoints(*dm, x_src, y_src);

    // Should only have 3 points (intersection of time indices 0,1,2)
    REQUIRE(result.size() == 3);
    CHECK(result.x_values[0] == 1.0f);
    CHECK(result.y_values[0] == 10.0f);
}

TEST_CASE("buildScatterPoints: ATS x ATS same source (self-pairing)",
          "[ScatterPlot][BuildScatterPoints]")
{
    auto dm = makeDataManager();

    std::vector<float> vals = {1.0f, 2.0f, 3.0f};
    addAnalogWithValues(*dm, "signal", vals);

    ScatterAxisSource x_src{.data_key = "signal"};
    ScatterAxisSource y_src{.data_key = "signal"};

    auto result = buildScatterPoints(*dm, x_src, y_src);

    REQUIRE(result.size() == 3);
    // Should be same values on both axes (identity line)
    for (std::size_t i = 0; i < result.size(); ++i) {
        CHECK(result.x_values[i] == result.y_values[i]);
    }
}

TEST_CASE("buildScatterPoints: ATS x ATS with temporal offset on Y",
          "[ScatterPlot][BuildScatterPoints]")
{
    auto dm = makeDataManager();

    std::vector<float> vals = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
    addAnalogWithValues(*dm, "signal", vals);

    ScatterAxisSource x_src{.data_key = "signal", .time_offset = 0};
    ScatterAxisSource y_src{.data_key = "signal", .time_offset = 1};

    auto result = buildScatterPoints(*dm, x_src, y_src);

    // x(t) vs y(t+1): for t=0, x=10, y=y(1)=20; for t=1, x=20, y=y(2)=30; etc.
    // t=4 won't have y(5), so it's excluded
    REQUIRE(result.size() == 4);
    CHECK(result.x_values[0] == 10.0f);
    CHECK(result.y_values[0] == 20.0f);
    CHECK(result.x_values[3] == 40.0f);
    CHECK(result.y_values[3] == 50.0f);
}

// =============================================================================
// AnalogTimeSeries × TensorData (TFI)
// =============================================================================

TEST_CASE("buildScatterPoints: ATS x TensorData TFI",
          "[ScatterPlot][BuildScatterPoints]")
{
    auto dm = makeDataManager();

    std::vector<float> x_vals = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> y_vals = {100.0f, 200.0f, 300.0f, 400.0f};

    addAnalogWithValues(*dm, "ats_x", x_vals);
    addTensorTFIWithValues(*dm, "tensor_y", y_vals);

    ScatterAxisSource x_src{.data_key = "ats_x"};
    ScatterAxisSource y_src{.data_key = "tensor_y", .tensor_column_index = 0};

    auto result = buildScatterPoints(*dm, x_src, y_src);

    REQUIRE(result.size() == 4);
    CHECK(result.x_values[0] == 1.0f);
    CHECK(result.y_values[0] == 100.0f);
}

// =============================================================================
// TensorData (TFI) × AnalogTimeSeries
// =============================================================================

TEST_CASE("buildScatterPoints: TensorData TFI x ATS",
          "[ScatterPlot][BuildScatterPoints]")
{
    auto dm = makeDataManager();

    std::vector<float> x_vals = {100.0f, 200.0f, 300.0f};
    std::vector<float> y_vals = {1.0f, 2.0f, 3.0f};

    addTensorTFIWithValues(*dm, "tensor_x", x_vals);
    addAnalogWithValues(*dm, "ats_y", y_vals);

    ScatterAxisSource x_src{.data_key = "tensor_x", .tensor_column_index = 0};
    ScatterAxisSource y_src{.data_key = "ats_y"};

    auto result = buildScatterPoints(*dm, x_src, y_src);

    REQUIRE(result.size() == 3);
    CHECK(result.x_values[0] == 100.0f);
    CHECK(result.y_values[0] == 1.0f);
}

// =============================================================================
// TensorData (Ordinal) × TensorData (Ordinal)
// =============================================================================

TEST_CASE("buildScatterPoints: Ordinal Tensor x Ordinal Tensor",
          "[ScatterPlot][BuildScatterPoints]")
{
    auto dm = makeDataManager();

    std::vector<float> x_vals = {5.0f, 10.0f, 15.0f};
    std::vector<float> y_vals = {50.0f, 100.0f, 150.0f};

    addTensorOrdinalWithValues(*dm, "ord_x", x_vals);
    addTensorOrdinalWithValues(*dm, "ord_y", y_vals);

    ScatterAxisSource x_src{.data_key = "ord_x", .tensor_column_index = 0};
    ScatterAxisSource y_src{.data_key = "ord_y", .tensor_column_index = 0};

    auto result = buildScatterPoints(*dm, x_src, y_src);

    REQUIRE(result.size() == 3);
    CHECK(result.x_values[0] == 5.0f);
    CHECK(result.y_values[0] == 50.0f);
    CHECK(result.x_values[2] == 15.0f);
    CHECK(result.y_values[2] == 150.0f);
}

// =============================================================================
// Empty / Missing sources
// =============================================================================

TEST_CASE("buildScatterPoints: empty data key returns empty result",
          "[ScatterPlot][BuildScatterPoints]")
{
    auto dm = makeDataManager();

    ScatterAxisSource x_src{.data_key = ""};
    ScatterAxisSource y_src{.data_key = ""};

    auto result = buildScatterPoints(*dm, x_src, y_src);

    CHECK(result.empty());
}

TEST_CASE("buildScatterPoints: nonexistent data key returns empty result",
          "[ScatterPlot][BuildScatterPoints]")
{
    auto dm = makeDataManager();

    ScatterAxisSource x_src{.data_key = "no_such_key"};
    ScatterAxisSource y_src{.data_key = "also_missing"};

    auto result = buildScatterPoints(*dm, x_src, y_src);

    CHECK(result.empty());
}

TEST_CASE("buildScatterPoints: incompatible sources return empty result",
          "[ScatterPlot][BuildScatterPoints]")
{
    auto dm = makeDataManager();

    std::vector<float> ats_vals = {1.0f, 2.0f, 3.0f};
    addAnalogWithValues(*dm, "ats_key", ats_vals);

    std::vector<float> ord_vals = {10.0f, 20.0f, 30.0f};
    addTensorOrdinalWithValues(*dm, "ord_key", ord_vals);

    ScatterAxisSource x_src{.data_key = "ats_key"};
    ScatterAxisSource y_src{.data_key = "ord_key", .tensor_column_index = 0};

    auto result = buildScatterPoints(*dm, x_src, y_src);

    // ATS x Ordinal are incompatible
    CHECK(result.empty());
}

// =============================================================================
// Named columns
// =============================================================================

TEST_CASE("buildScatterPoints: TensorData with named column",
          "[ScatterPlot][BuildScatterPoints]")
{
    auto dm = makeDataManager();

    std::vector<float> vals = {7.0f, 8.0f, 9.0f};
    addTensorTFIWithValues(*dm, "tensor_named", vals, {"area"});

    std::vector<float> ats_vals = {1.0f, 2.0f, 3.0f};
    addAnalogWithValues(*dm, "ats_x", ats_vals);

    ScatterAxisSource x_src{.data_key = "ats_x"};
    ScatterAxisSource y_src{.data_key = "tensor_named", .tensor_column_name = "area"};

    auto result = buildScatterPoints(*dm, x_src, y_src);

    REQUIRE(result.size() == 3);
    CHECK(result.y_values[0] == 7.0f);
    CHECK(result.y_values[2] == 9.0f);
}

// =============================================================================
// ScatterPointData struct
// =============================================================================

TEST_CASE("ScatterPointData: clear and empty",
          "[ScatterPlot][BuildScatterPoints]")
{
    ScatterPointData data;
    CHECK(data.empty());
    CHECK(data.size() == 0);

    data.x_values = {1.0f};
    data.y_values = {2.0f};
    data.time_indices = {TimeFrameIndex(0)};

    CHECK_FALSE(data.empty());
    CHECK(data.size() == 1);

    data.clear();
    CHECK(data.empty());
}
