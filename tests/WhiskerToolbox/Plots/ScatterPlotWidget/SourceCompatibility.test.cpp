/**
 * @file SourceCompatibility.test.cpp
 * @brief Tests for scatter plot source compatibility validation
 *
 * Verifies the compatibility matrix between AnalogTimeSeries and TensorData
 * sources with different row types (TimeFrameIndex, Interval, Ordinal).
 *
 * @see SourceCompatibility.hpp for the compatibility matrix
 * @see ScatterAxisSource.hpp for the source descriptor
 */

#include "Core/SourceCompatibility.hpp"
#include "Core/ScatterAxisSource.hpp"

#include "DataManager/DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/interval_data.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <memory>
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

/// Add an AnalogTimeSeries with `count` samples to `dm` under `key`.
void addAnalog(DataManager & dm, std::string const & key, std::size_t count)
{
    std::vector<float> values(count, 1.0f);
    auto ats = std::make_shared<AnalogTimeSeries>(values, count);
    ats->setTimeFrame(dm.getTime(TimeKey("time")));
    dm.setData<AnalogTimeSeries>(key, ats, TimeKey("time"));
}

/// Add a TensorData with TimeFrameIndex rows to `dm`.
void addTensorTFI(DataManager & dm, std::string const & key,
                  std::size_t num_rows, std::size_t num_cols)
{
    std::vector<float> data(num_rows * num_cols, 0.0f);
    auto ts = makeDenseTimeStorage(num_rows);
    auto tf = dm.getTime(TimeKey("time"));
    auto tensor = std::make_shared<TensorData>(
        TensorData::createTimeSeries2D(data, num_rows, num_cols, ts, tf));
    dm.setData<TensorData>(key, tensor, TimeKey("time"));
}

/// Add a TensorData with Interval rows to `dm`.
void addTensorInterval(DataManager & dm, std::string const & key,
                       std::size_t num_rows, std::size_t num_cols)
{
    std::vector<float> data(num_rows * num_cols, 0.0f);
    auto tf = dm.getTime(TimeKey("time"));
    std::vector<TimeFrameInterval> intervals;
    intervals.reserve(num_rows);
    for (std::size_t i = 0; i < num_rows; ++i) {
        auto start = TimeFrameIndex(static_cast<int>(i * 10));
        auto end = TimeFrameIndex(static_cast<int>(i * 10 + 5));
        intervals.emplace_back(start, end);
    }
    auto tensor = std::make_shared<TensorData>(
        TensorData::createFromIntervals(data, num_rows, num_cols, intervals, tf));
    dm.setData<TensorData>(key, tensor, TimeKey("time"));
}

/// Add a TensorData with Ordinal rows to `dm`.
void addTensorOrdinal(DataManager & dm, std::string const & key,
                      std::size_t num_rows, std::size_t num_cols)
{
    std::vector<float> data(num_rows * num_cols, 0.0f);
    std::vector<AxisDescriptor> axes = {
        {"row", num_rows},
        {"col", num_cols}
    };
    auto tensor = std::make_shared<TensorData>(
        TensorData::createND(data, axes));
    dm.setData<TensorData>(key, tensor, TimeKey("time"));
}

/// Set up a DataManager with a default TimeFrame.
std::shared_ptr<DataManager> makeDataManager()
{
    auto dm = std::make_shared<DataManager>();
    auto tf = makeTimeFrame(1000);
    dm->setTime(TimeKey("time"), tf);
    return dm;
}

}  // namespace

// =============================================================================
// resolveSourceRowType
// =============================================================================

TEST_CASE("resolveSourceRowType returns Unknown for empty key",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    ScatterAxisSource source{.data_key = ""};
    CHECK(resolveSourceRowType(*dm, source) == ScatterSourceRowType::Unknown);
}

TEST_CASE("resolveSourceRowType returns Unknown for missing key",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    ScatterAxisSource source{.data_key = "nonexistent"};
    CHECK(resolveSourceRowType(*dm, source) == ScatterSourceRowType::Unknown);
}

TEST_CASE("resolveSourceRowType identifies AnalogTimeSeries",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addAnalog(*dm, "ats", 100);

    ScatterAxisSource source{.data_key = "ats"};
    CHECK(resolveSourceRowType(*dm, source) == ScatterSourceRowType::AnalogTimeSeries);
}

TEST_CASE("resolveSourceRowType identifies TensorData TimeFrameIndex",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addTensorTFI(*dm, "tensor_tfi", 50, 3);

    ScatterAxisSource source{.data_key = "tensor_tfi"};
    CHECK(resolveSourceRowType(*dm, source) == ScatterSourceRowType::TensorTimeFrameIndex);
}

TEST_CASE("resolveSourceRowType identifies TensorData Interval",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addTensorInterval(*dm, "tensor_int", 10, 2);

    ScatterAxisSource source{.data_key = "tensor_int"};
    CHECK(resolveSourceRowType(*dm, source) == ScatterSourceRowType::TensorInterval);
}

TEST_CASE("resolveSourceRowType identifies TensorData Ordinal",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addTensorOrdinal(*dm, "tensor_ord", 20, 4);

    ScatterAxisSource source{.data_key = "tensor_ord"};
    CHECK(resolveSourceRowType(*dm, source) == ScatterSourceRowType::TensorOrdinal);
}

// =============================================================================
// checkSourceCompatibility — compatible cases
// =============================================================================

TEST_CASE("compatibility: AnalogTimeSeries x AnalogTimeSeries",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addAnalog(*dm, "ats_x", 100);
    addAnalog(*dm, "ats_y", 100);

    ScatterAxisSource x{.data_key = "ats_x"};
    ScatterAxisSource y{.data_key = "ats_y"};

    auto result = checkSourceCompatibility(*dm, x, y);
    CHECK(result.compatible);
    CHECK(result.bothSourcesResolved());
    CHECK(result.x_row_type == ScatterSourceRowType::AnalogTimeSeries);
    CHECK(result.y_row_type == ScatterSourceRowType::AnalogTimeSeries);
    CHECK(result.warning_message.empty());
}

TEST_CASE("compatibility: AnalogTimeSeries x TensorData (TFI)",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addAnalog(*dm, "ats_x", 100);
    addTensorTFI(*dm, "tensor_y", 50, 2);

    ScatterAxisSource x{.data_key = "ats_x"};
    ScatterAxisSource y{.data_key = "tensor_y"};

    auto result = checkSourceCompatibility(*dm, x, y);
    CHECK(result.compatible);
    CHECK(result.x_row_type == ScatterSourceRowType::AnalogTimeSeries);
    CHECK(result.y_row_type == ScatterSourceRowType::TensorTimeFrameIndex);
}

TEST_CASE("compatibility: TensorData (TFI) x AnalogTimeSeries",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addTensorTFI(*dm, "tensor_x", 50, 2);
    addAnalog(*dm, "ats_y", 100);

    ScatterAxisSource x{.data_key = "tensor_x"};
    ScatterAxisSource y{.data_key = "ats_y"};

    auto result = checkSourceCompatibility(*dm, x, y);
    CHECK(result.compatible);
    CHECK(result.x_row_type == ScatterSourceRowType::TensorTimeFrameIndex);
    CHECK(result.y_row_type == ScatterSourceRowType::AnalogTimeSeries);
}

TEST_CASE("compatibility: TensorData (TFI) x TensorData (TFI)",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addTensorTFI(*dm, "tensor_x", 50, 3);
    addTensorTFI(*dm, "tensor_y", 40, 2);

    ScatterAxisSource x{.data_key = "tensor_x"};
    ScatterAxisSource y{.data_key = "tensor_y"};

    auto result = checkSourceCompatibility(*dm, x, y);
    CHECK(result.compatible);
    CHECK(result.x_row_type == ScatterSourceRowType::TensorTimeFrameIndex);
    CHECK(result.y_row_type == ScatterSourceRowType::TensorTimeFrameIndex);
}

TEST_CASE("compatibility: TensorData (Interval) x TensorData (Interval)",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addTensorInterval(*dm, "tensor_x", 10, 2);
    addTensorInterval(*dm, "tensor_y", 10, 3);

    ScatterAxisSource x{.data_key = "tensor_x"};
    ScatterAxisSource y{.data_key = "tensor_y"};

    auto result = checkSourceCompatibility(*dm, x, y);
    CHECK(result.compatible);
    CHECK(result.x_row_type == ScatterSourceRowType::TensorInterval);
    CHECK(result.y_row_type == ScatterSourceRowType::TensorInterval);
}

TEST_CASE("compatibility: TensorData (Ordinal) x TensorData (Ordinal) same row count",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addTensorOrdinal(*dm, "tensor_x", 20, 3);
    addTensorOrdinal(*dm, "tensor_y", 20, 5);

    ScatterAxisSource x{.data_key = "tensor_x"};
    ScatterAxisSource y{.data_key = "tensor_y"};

    auto result = checkSourceCompatibility(*dm, x, y);
    CHECK(result.compatible);
    CHECK(result.x_row_type == ScatterSourceRowType::TensorOrdinal);
    CHECK(result.y_row_type == ScatterSourceRowType::TensorOrdinal);
}

// =============================================================================
// checkSourceCompatibility — incompatible cases
// =============================================================================

TEST_CASE("incompatibility: TensorData (Ordinal) x TensorData (Ordinal) different row count",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addTensorOrdinal(*dm, "tensor_x", 20, 3);
    addTensorOrdinal(*dm, "tensor_y", 30, 5);

    ScatterAxisSource x{.data_key = "tensor_x"};
    ScatterAxisSource y{.data_key = "tensor_y"};

    auto result = checkSourceCompatibility(*dm, x, y);
    CHECK_FALSE(result.compatible);
    CHECK(result.bothSourcesResolved());
    CHECK_FALSE(result.warning_message.empty());
    // Verify the warning mentions row counts
    CHECK(result.warning_message.find("20") != std::string::npos);
    CHECK(result.warning_message.find("30") != std::string::npos);
}

TEST_CASE("incompatibility: AnalogTimeSeries x TensorData (Interval)",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addAnalog(*dm, "ats_x", 100);
    addTensorInterval(*dm, "tensor_y", 10, 2);

    ScatterAxisSource x{.data_key = "ats_x"};
    ScatterAxisSource y{.data_key = "tensor_y"};

    auto result = checkSourceCompatibility(*dm, x, y);
    CHECK_FALSE(result.compatible);
    CHECK(result.bothSourcesResolved());
    CHECK_FALSE(result.warning_message.empty());
}

TEST_CASE("incompatibility: AnalogTimeSeries x TensorData (Ordinal)",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addAnalog(*dm, "ats_x", 100);
    addTensorOrdinal(*dm, "tensor_y", 20, 3);

    ScatterAxisSource x{.data_key = "ats_x"};
    ScatterAxisSource y{.data_key = "tensor_y"};

    auto result = checkSourceCompatibility(*dm, x, y);
    CHECK_FALSE(result.compatible);
    CHECK(result.bothSourcesResolved());
}

TEST_CASE("incompatibility: TensorData (TFI) x TensorData (Interval)",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addTensorTFI(*dm, "tensor_x", 50, 2);
    addTensorInterval(*dm, "tensor_y", 10, 3);

    ScatterAxisSource x{.data_key = "tensor_x"};
    ScatterAxisSource y{.data_key = "tensor_y"};

    auto result = checkSourceCompatibility(*dm, x, y);
    CHECK_FALSE(result.compatible);
    CHECK(result.bothSourcesResolved());
}

TEST_CASE("incompatibility: TensorData (TFI) x TensorData (Ordinal)",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addTensorTFI(*dm, "tensor_x", 50, 2);
    addTensorOrdinal(*dm, "tensor_y", 50, 3);

    ScatterAxisSource x{.data_key = "tensor_x"};
    ScatterAxisSource y{.data_key = "tensor_y"};

    auto result = checkSourceCompatibility(*dm, x, y);
    CHECK_FALSE(result.compatible);
    CHECK(result.bothSourcesResolved());
}

TEST_CASE("incompatibility: TensorData (Interval) x TensorData (Ordinal)",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addTensorInterval(*dm, "tensor_x", 10, 2);
    addTensorOrdinal(*dm, "tensor_y", 10, 3);

    ScatterAxisSource x{.data_key = "tensor_x"};
    ScatterAxisSource y{.data_key = "tensor_y"};

    auto result = checkSourceCompatibility(*dm, x, y);
    CHECK_FALSE(result.compatible);
    CHECK(result.bothSourcesResolved());
}

// =============================================================================
// checkSourceCompatibility — unknown/missing sources
// =============================================================================

TEST_CASE("incompatibility: X source not found",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addAnalog(*dm, "ats_y", 100);

    ScatterAxisSource x{.data_key = "missing"};
    ScatterAxisSource y{.data_key = "ats_y"};

    auto result = checkSourceCompatibility(*dm, x, y);
    CHECK_FALSE(result.compatible);
    CHECK_FALSE(result.bothSourcesResolved());
    CHECK(result.x_row_type == ScatterSourceRowType::Unknown);
    CHECK(result.warning_message.find("missing") != std::string::npos);
}

TEST_CASE("incompatibility: Y source not found",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();
    addAnalog(*dm, "ats_x", 100);

    ScatterAxisSource x{.data_key = "ats_x"};
    ScatterAxisSource y{.data_key = "missing"};

    auto result = checkSourceCompatibility(*dm, x, y);
    CHECK_FALSE(result.compatible);
    CHECK_FALSE(result.bothSourcesResolved());
    CHECK(result.y_row_type == ScatterSourceRowType::Unknown);
    CHECK(result.warning_message.find("missing") != std::string::npos);
}

TEST_CASE("incompatibility: both sources not found",
          "[ScatterPlot][SourceCompatibility]")
{
    auto dm = makeDataManager();

    ScatterAxisSource x{.data_key = "missing_x"};
    ScatterAxisSource y{.data_key = "missing_y"};

    auto result = checkSourceCompatibility(*dm, x, y);
    CHECK_FALSE(result.compatible);
    CHECK_FALSE(result.bothSourcesResolved());
}
