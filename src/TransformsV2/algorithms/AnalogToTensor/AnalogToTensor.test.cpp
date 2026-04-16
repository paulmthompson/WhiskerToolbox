/**
 * @file AnalogToTensor.test.cpp
 * @brief Catch2 tests for AnalogToTensor and TensorToAnalog transforms
 *
 * Tests include:
 * - Round-trip: N AnalogTimeSeries → AnalogToTensor → TensorToAnalog → verify values
 * - Single channel
 * - Mismatched TimeFrames (should fail gracefully)
 * - Mismatched sample counts (should fail gracefully)
 * - Column names preservation
 * - Time index preservation
 * - Empty input handling
 * - TensorToAnalog column selection
 */

#include "TransformsV2/algorithms/AnalogToTensor/AnalogToTensor.hpp"
#include "TransformsV2/algorithms/TensorToAnalog/TensorToAnalog.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Helpers
// ============================================================================

namespace {

ComputeContext makeCtx() {
    return ComputeContext{};
}

std::shared_ptr<TimeFrame> makeTimeFrame(int count) {
    std::vector<int> times(count);
    for (int i = 0; i < count; ++i) {
        times[i] = i;
    }
    return std::make_shared<TimeFrame>(times);
}

/// Create a test AnalogTimeSeries with consecutive time indices [0, N)
std::shared_ptr<AnalogTimeSeries> makeAnalog(
        std::vector<float> values,
        std::shared_ptr<TimeFrame> tf) {

    auto const n = values.size();
    auto analog = std::make_shared<AnalogTimeSeries>(std::move(values), n);
    analog->setTimeFrame(tf);
    return analog;
}

}// namespace

// ============================================================================
// AnalogToTensor Tests
// ============================================================================

TEST_CASE("AnalogToTensor round-trip preserves values", "[AnalogToTensor]") {
    constexpr std::size_t num_samples = 100;
    constexpr std::size_t num_channels = 4;

    auto tf = makeTimeFrame(static_cast<int>(num_samples + 10));

    // Create channels with distinct values
    std::vector<std::shared_ptr<AnalogTimeSeries const>> channels;
    for (std::size_t c = 0; c < num_channels; ++c) {
        std::vector<float> values(num_samples);
        for (std::size_t i = 0; i < num_samples; ++i) {
            values[i] = static_cast<float>(c * 1000 + i);
        }
        channels.push_back(makeAnalog(std::move(values), tf));
    }

    AnalogToTensorParams a2t_params;
    a2t_params.channel_keys = {"ch0", "ch1", "ch2", "ch3"};

    auto tensor = analogToTensor(channels, a2t_params, makeCtx());
    REQUIRE(tensor != nullptr);
    REQUIRE(tensor->ndim() == 2);
    REQUIRE(tensor->numRows() == num_samples);
    REQUIRE(tensor->numColumns() == num_channels);

    // Verify column names
    REQUIRE(tensor->hasNamedColumns());
    auto const & col_names = tensor->columnNames();
    REQUIRE(col_names.size() == num_channels);
    REQUIRE(col_names[0] == "ch0");
    REQUIRE(col_names[3] == "ch3");

    // Convert back
    TensorToAnalogParams t2a_params;
    t2a_params.output_key_prefix = "out";

    auto analogs = tensorToAnalog(*tensor, t2a_params, makeCtx());
    REQUIRE(analogs.size() == num_channels);

    // Verify round-trip values
    for (std::size_t c = 0; c < num_channels; ++c) {
        REQUIRE(analogs[c]->getNumSamples() == num_samples);
        std::size_t idx = 0;
        for (float value: analogs[c]->viewValues()) {
            float const expected = static_cast<float>(c * 1000 + idx);
            REQUIRE(value == expected);
            ++idx;
        }
    }
}

TEST_CASE("AnalogToTensor single channel", "[AnalogToTensor]") {
    auto tf = makeTimeFrame(20);
    std::vector<float> values = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    auto ch = makeAnalog(values, tf);

    AnalogToTensorParams params;
    params.channel_keys = {"signal"};

    auto tensor = analogToTensor({ch}, params, makeCtx());
    REQUIRE(tensor != nullptr);
    REQUIRE(tensor->numRows() == 5);
    REQUIRE(tensor->numColumns() == 1);
    REQUIRE(tensor->columnNames()[0] == "signal");

    // Verify values
    auto col = tensor->getColumn(0);
    REQUIRE(col.size() == 5);
    for (std::size_t i = 0; i < 5; ++i) {
        REQUIRE(col[i] == values[i]);
    }
}

TEST_CASE("AnalogToTensor rejects mismatched TimeFrames", "[AnalogToTensor]") {
    auto tf1 = makeTimeFrame(20);
    auto tf2 = makeTimeFrame(20);// Different pointer, same content

    auto ch0 = makeAnalog({1.0f, 2.0f, 3.0f}, tf1);
    auto ch1 = makeAnalog({4.0f, 5.0f, 6.0f}, tf2);

    AnalogToTensorParams params;
    auto tensor = analogToTensor({ch0, ch1}, params, makeCtx());
    REQUIRE(tensor == nullptr);
}

TEST_CASE("AnalogToTensor rejects mismatched sample counts", "[AnalogToTensor]") {
    auto tf = makeTimeFrame(20);

    auto ch0 = makeAnalog({1.0f, 2.0f, 3.0f}, tf);
    auto ch1 = makeAnalog({4.0f, 5.0f}, tf);

    AnalogToTensorParams params;
    auto tensor = analogToTensor({ch0, ch1}, params, makeCtx());
    REQUIRE(tensor == nullptr);
}

TEST_CASE("AnalogToTensor rejects empty channels vector", "[AnalogToTensor]") {
    AnalogToTensorParams params;
    auto tensor = analogToTensor({}, params, makeCtx());
    REQUIRE(tensor == nullptr);
}

TEST_CASE("AnalogToTensor generates default column names when keys mismatch", "[AnalogToTensor]") {
    auto tf = makeTimeFrame(10);
    auto ch0 = makeAnalog({1.0f, 2.0f}, tf);
    auto ch1 = makeAnalog({3.0f, 4.0f}, tf);

    AnalogToTensorParams params;
    // channel_keys empty → should generate "ch0", "ch1"

    auto tensor = analogToTensor({ch0, ch1}, params, makeCtx());
    REQUIRE(tensor != nullptr);
    REQUIRE(tensor->columnNames()[0] == "ch0");
    REQUIRE(tensor->columnNames()[1] == "ch1");
}

TEST_CASE("AnalogToTensor preserves TimeFrame in output TensorData", "[AnalogToTensor]") {
    auto tf = makeTimeFrame(50);
    auto ch0 = makeAnalog({1.0f, 2.0f, 3.0f}, tf);

    AnalogToTensorParams params;
    auto tensor = analogToTensor({ch0}, params, makeCtx());
    REQUIRE(tensor != nullptr);
    REQUIRE(tensor->getTimeFrame().get() == tf.get());

    // Check RowDescriptor is TimeFrameIndex
    REQUIRE(tensor->rows().type() == RowType::TimeFrameIndex);
}

// ============================================================================
// TensorToAnalog Tests
// ============================================================================

TEST_CASE("TensorToAnalog extracts all columns by default", "[TensorToAnalog]") {
    constexpr std::size_t rows = 10;
    constexpr std::size_t cols = 3;
    std::vector<float> data(rows * cols);
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            data[r * cols + c] = static_cast<float>(c * 100 + r);
        }
    }

    auto ts = TimeIndexStorageFactory::createDenseFromZero(rows);
    auto tf = makeTimeFrame(static_cast<int>(rows + 5));
    auto tensor = TensorData::createTimeSeries2D(data, rows, cols, ts, tf, {"a", "b", "c"});

    TensorToAnalogParams params;
    params.output_key_prefix = "out";

    auto analogs = tensorToAnalog(tensor, params, makeCtx());
    REQUIRE(analogs.size() == 3);

    for (std::size_t c = 0; c < cols; ++c) {
        REQUIRE(analogs[c]->getNumSamples() == rows);
        std::size_t idx = 0;
        for (float value: analogs[c]->viewValues()) {
            REQUIRE(value == static_cast<float>(c * 100 + idx));
            ++idx;
        }
    }
}

TEST_CASE("TensorToAnalog extracts selected columns", "[TensorToAnalog]") {
    constexpr std::size_t rows = 5;
    constexpr std::size_t cols = 4;
    std::vector<float> data(rows * cols);
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            data[r * cols + c] = static_cast<float>(c);
        }
    }

    auto ts = TimeIndexStorageFactory::createDenseFromZero(rows);
    auto tf = makeTimeFrame(10);
    auto tensor = TensorData::createTimeSeries2D(data, rows, cols, ts, tf, {"a", "b", "c", "d"});

    TensorToAnalogParams params;
    params.columns = {1, 3};

    auto analogs = tensorToAnalog(tensor, params, makeCtx());
    REQUIRE(analogs.size() == 2);

    // First extracted column should be column 1
    for (float value: analogs[0]->viewValues()) {
        REQUIRE(value == 1.0f);
    }
    // Second extracted column should be column 3
    for (float value: analogs[1]->viewValues()) {
        REQUIRE(value == 3.0f);
    }
}

TEST_CASE("TensorToAnalog rejects out-of-range column index", "[TensorToAnalog]") {
    auto tensor = TensorData::createOrdinal2D({1.0f, 2.0f}, 1, 2, {"a", "b"});

    TensorToAnalogParams params;
    params.columns = {5};

    auto analogs = tensorToAnalog(tensor, params, makeCtx());
    REQUIRE(analogs.empty());
}

TEST_CASE("TensorToAnalog preserves time indices", "[TensorToAnalog]") {
    constexpr std::size_t rows = 3;
    std::vector<float> data = {10.0f, 20.0f, 30.0f};

    auto ts = TimeIndexStorageFactory::createDenseFromZero(rows);
    auto tf = makeTimeFrame(10);
    auto tensor = TensorData::createTimeSeries2D(data, rows, 1, ts, tf, {"val"});

    TensorToAnalogParams params;
    auto analogs = tensorToAnalog(tensor, params, makeCtx());
    REQUIRE(analogs.size() == 1);

    // Verify the TimeFrame is preserved
    REQUIRE(analogs[0]->getTimeFrame().get() == tf.get());
    REQUIRE(analogs[0]->getNumSamples() == rows);
}

TEST_CASE("TensorToAnalog handles ordinal tensor", "[TensorToAnalog]") {
    auto tensor = TensorData::createOrdinal2D({1.0f, 2.0f, 3.0f, 4.0f}, 2, 2, {"x", "y"});

    TensorToAnalogParams params;
    auto analogs = tensorToAnalog(tensor, params, makeCtx());
    REQUIRE(analogs.size() == 2);
    REQUIRE(analogs[0]->getNumSamples() == 2);
    REQUIRE(analogs[1]->getNumSamples() == 2);
}
