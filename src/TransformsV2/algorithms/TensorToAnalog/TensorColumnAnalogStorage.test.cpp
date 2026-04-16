/**
 * @file TensorColumnAnalogStorage.test.cpp
 * @brief Catch2 tests for TensorColumnAnalogStorage and the zero-copy
 *        TensorToAnalog path (Phase 3 of multi-channel storage roadmap).
 *
 * Tests include:
 * - Basic construction and element access
 * - Zero-copy memory sharing (pointer comparison)
 * - Contiguous span access
 * - getDataInTimeFrameIndexRange returns non-empty span
 * - TensorToAnalog round-trip with zero-copy views
 * - Single column / single row edge cases
 * - Storage cache validity
 * - createFromStorage factory method
 */

#include "TransformsV2/algorithms/AnalogToTensor/AnalogToTensor.hpp"
#include "TransformsV2/algorithms/TensorToAnalog/TensorToAnalog.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/storage/TensorColumnAnalogStorage.hpp"
#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "Tensors/storage/ArmadilloTensorStorage.hpp"
#include "Tensors/storage/TensorStorageWrapper.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstddef>
#include <memory>
#include <span>
#include <utility>
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

/// Create a 2D TensorData with known values: tensor(r, c) = c * 1000 + r
std::shared_ptr<TensorData> makeTestTensor(
        std::size_t num_rows,
        std::size_t num_cols,
        std::shared_ptr<TimeFrame> tf = nullptr) {

    std::vector<float> flat;
    flat.reserve(num_rows * num_cols);
    for (std::size_t r = 0; r < num_rows; ++r) {
        for (std::size_t c = 0; c < num_cols; ++c) {
            flat.push_back(static_cast<float>(c * 1000 + r));
        }
    }

    auto time_storage = TimeIndexStorageFactory::createDenseFromZero(num_rows);

    auto tensor = std::make_shared<TensorData>(
            TensorData::createTimeSeries2D(flat, num_rows, num_cols, time_storage, std::move(tf)));
    return tensor;
}

/// Create a test AnalogTimeSeries with consecutive time indices [0, N)
std::shared_ptr<AnalogTimeSeries> makeAnalog(
        std::vector<float> values,
        std::shared_ptr<TimeFrame> tf) {

    auto const n = values.size();
    auto analog = std::make_shared<AnalogTimeSeries>(std::move(values), n);
    analog->setTimeFrame(std::move(tf));
    return analog;
}

}// namespace

// ============================================================================
// TensorColumnAnalogStorage — Basic Construction
// ============================================================================

TEST_CASE("TensorColumnAnalogStorage basic element access", "[TensorColumnAnalogStorage]") {
    constexpr std::size_t rows = 50;
    constexpr std::size_t cols = 4;
    auto tensor = makeTestTensor(rows, cols);

    TensorColumnAnalogStorage const storage(tensor, 2);

    REQUIRE(storage.size() == rows);
    for (std::size_t i = 0; i < rows; ++i) {
        auto const expected = static_cast<float>(static_cast<std::size_t>(2 * 1000) + i);
        CHECK(storage.getValueAt(i) == expected);
    }
}

TEST_CASE("TensorColumnAnalogStorage size matches tensor rows", "[TensorColumnAnalogStorage]") {
    auto tensor = makeTestTensor(100, 3);
    TensorColumnAnalogStorage const storage(tensor, 0);
    CHECK(storage.size() == 100);
    CHECK(!storage.empty());
}

// ============================================================================
// Zero-Copy Verification
// ============================================================================

TEST_CASE("TensorColumnAnalogStorage shares memory with Armadillo tensor", "[TensorColumnAnalogStorage]") {
    constexpr std::size_t rows = 200;
    constexpr std::size_t cols = 8;
    auto tensor = makeTestTensor(rows, cols);

    // Get the Armadillo column pointer directly
    auto const * arma_storage = tensor->storage().tryGetAs<ArmadilloTensorStorage>();
    REQUIRE(arma_storage != nullptr);
    float const * col3_ptr = arma_storage->matrix().colptr(3);

    // Create the storage backend
    TensorColumnAnalogStorage const storage(tensor, 3);

    // Verify the data pointer is the same (zero-copy)
    CHECK(storage.data() == col3_ptr);

    // Verify span also points to the same memory
    auto span = storage.getSpan();
    REQUIRE(!span.empty());
    CHECK(span.data() == col3_ptr);
    CHECK(span.size() == rows);
}

TEST_CASE("TensorColumnAnalogStorage is contiguous for Armadillo backend", "[TensorColumnAnalogStorage]") {
    auto tensor = makeTestTensor(10, 3);
    TensorColumnAnalogStorage const storage(tensor, 1);

    CHECK(storage.isContiguous());
}

// ============================================================================
// Span Access
// ============================================================================

TEST_CASE("TensorColumnAnalogStorage getSpan returns full column", "[TensorColumnAnalogStorage]") {
    constexpr std::size_t rows = 30;
    constexpr std::size_t cols = 2;
    auto tensor = makeTestTensor(rows, cols);

    TensorColumnAnalogStorage const storage(tensor, 1);
    auto span = storage.getSpan();

    REQUIRE(span.size() == rows);
    for (std::size_t i = 0; i < rows; ++i) {
        auto const expected = static_cast<float>(static_cast<std::size_t>(1 * 1000) + i);
        CHECK(span[i] == expected);
    }
}

TEST_CASE("TensorColumnAnalogStorage getSpanRange works correctly", "[TensorColumnAnalogStorage]") {
    constexpr std::size_t rows = 100;
    auto tensor = makeTestTensor(rows, 4);

    TensorColumnAnalogStorage const storage(tensor, 2);

    auto sub_span = storage.getSpanRange(10, 20);
    REQUIRE(sub_span.size() == 10);
    for (std::size_t i = 0; i < 10; ++i) {
        auto const expected = static_cast<float>(2 * 1000 + 10 + i);
        CHECK(sub_span[i] == expected);
    }
}

TEST_CASE("TensorColumnAnalogStorage getSpanRange edge cases", "[TensorColumnAnalogStorage]") {
    auto tensor = makeTestTensor(50, 3);
    TensorColumnAnalogStorage const storage(tensor, 0);

    // Empty range
    CHECK(storage.getSpanRange(10, 10).empty());

    // Invalid: start > end
    CHECK(storage.getSpanRange(20, 10).empty());

    // Invalid: end > size
    CHECK(storage.getSpanRange(0, 100).empty());

    // Full range
    auto full = storage.getSpanRange(0, 50);
    CHECK(full.size() == 50);
}

// ============================================================================
// Cache Optimization
// ============================================================================

TEST_CASE("TensorColumnAnalogStorage tryGetCache returns valid cache", "[TensorColumnAnalogStorage]") {
    auto tensor = makeTestTensor(40, 5);
    TensorColumnAnalogStorage const storage(tensor, 4);

    auto cache = storage.tryGetCache();
    CHECK(cache.isValid());
    CHECK(cache.cache_size == 40);
    CHECK(cache.data_ptr != nullptr);
    CHECK(cache.is_contiguous);

    // Verify cache data matches
    for (std::size_t i = 0; i < 40; ++i) {
        auto const expected = static_cast<float>(static_cast<std::size_t>(4 * 1000) + i);
        CHECK(cache.getValue(i) == expected);
    }
}

TEST_CASE("TensorColumnAnalogStorage storage type is Custom", "[TensorColumnAnalogStorage]") {
    auto tensor = makeTestTensor(10, 2);
    TensorColumnAnalogStorage const storage(tensor, 0);

    CHECK(storage.getStorageType() == AnalogStorageType::Custom);
}

// ============================================================================
// AnalogTimeSeries createFromStorage factory
// ============================================================================

TEST_CASE("createFromStorage creates working AnalogTimeSeries", "[TensorColumnAnalogStorage]") {
    constexpr std::size_t rows = 60;
    constexpr std::size_t cols = 3;
    auto tensor = makeTestTensor(rows, cols);

    auto time_storage = TimeIndexStorageFactory::createDenseFromZero(rows);
    auto col_storage = TensorColumnAnalogStorage(tensor, 1);
    AnalogDataStorageWrapper wrapper(col_storage);

    auto analog = AnalogTimeSeries::createFromStorage(std::move(wrapper), time_storage);
    REQUIRE(analog != nullptr);
    REQUIRE(analog->getNumSamples() == rows);

    // Verify values
    std::size_t idx = 0;
    for (float const value: analog->viewValues()) {
        auto const expected = static_cast<float>(static_cast<std::size_t>(1 * 1000) + idx);
        CHECK(value == expected);
        ++idx;
    }
}

TEST_CASE("createFromStorage provides contiguous span access", "[TensorColumnAnalogStorage]") {
    constexpr std::size_t rows = 80;
    auto tensor = makeTestTensor(rows, 4);

    auto time_storage = TimeIndexStorageFactory::createDenseFromZero(rows);
    auto col_storage = TensorColumnAnalogStorage(tensor, 2);
    AnalogDataStorageWrapper wrapper(col_storage);

    auto analog = AnalogTimeSeries::createFromStorage(std::move(wrapper), time_storage);

    // getAnalogTimeSeries should return a non-empty span (unlike mmap)
    auto span = analog->getAnalogTimeSeries();
    REQUIRE(span.size() == rows);
    CHECK(span[0] == 2000.0f);
    CHECK(span[rows - 1] == static_cast<float>(2000 + rows - 1));
}

TEST_CASE("createFromStorage getDataInTimeFrameIndexRange returns non-empty span", "[TensorColumnAnalogStorage]") {
    constexpr std::size_t rows = 100;
    auto tensor = makeTestTensor(rows, 3);

    auto time_storage = TimeIndexStorageFactory::createDenseFromZero(rows);
    auto col_storage = TensorColumnAnalogStorage(tensor, 0);
    AnalogDataStorageWrapper wrapper(col_storage);

    auto analog = AnalogTimeSeries::createFromStorage(std::move(wrapper), time_storage);

    auto range_span = analog->getDataInTimeFrameIndexRange(
            TimeFrameIndex(10), TimeFrameIndex(19));
    REQUIRE(range_span.size() == 10);
    for (std::size_t i = 0; i < 10; ++i) {
        CHECK(range_span[i] == static_cast<float>(10 + i));
    }
}

// ============================================================================
// TensorToAnalog integration (zero-copy round-trip)
// ============================================================================

TEST_CASE("TensorToAnalog produces zero-copy views", "[TensorColumnAnalogStorage]") {
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

    // Analog → Tensor
    AnalogToTensorParams a2t_params;
    a2t_params.channel_keys = {"ch0", "ch1", "ch2", "ch3"};

    auto tensor = analogToTensor(channels, a2t_params, makeCtx());
    REQUIRE(tensor != nullptr);

    // Tensor → Analog (zero-copy)
    TensorToAnalogParams t2a_params;
    t2a_params.output_key_prefix = "out";

    auto analogs = tensorToAnalog(*tensor, t2a_params, makeCtx());
    REQUIRE(analogs.size() == num_channels);

    // Verify round-trip values
    for (std::size_t c = 0; c < num_channels; ++c) {
        REQUIRE(analogs[c]->getNumSamples() == num_samples);
        std::size_t idx = 0;
        for (float const value: analogs[c]->viewValues()) {
            auto const expected = static_cast<float>(c * 1000 + idx);
            REQUIRE(value == expected);
            ++idx;
        }
    }
}

TEST_CASE("TensorToAnalog views have contiguous span access", "[TensorColumnAnalogStorage]") {
    constexpr std::size_t rows = 50;
    constexpr std::size_t cols = 3;

    auto tensor = TensorData::createOrdinal2D(
            std::vector<float>(rows * cols, 42.0f), rows, cols);

    TensorToAnalogParams params;
    params.output_key_prefix = "test";

    auto analogs = tensorToAnalog(tensor, params, makeCtx());
    REQUIRE(analogs.size() == cols);

    for (auto const & analog: analogs) {
        auto span = analog->getAnalogTimeSeries();
        REQUIRE(span.size() == rows);
        for (float const v: span) {
            CHECK(v == 42.0f);
        }
    }
}

TEST_CASE("TensorToAnalog preserves TimeFrame on views", "[TensorColumnAnalogStorage]") {
    constexpr std::size_t rows = 20;
    constexpr std::size_t cols = 2;
    auto tf = makeTimeFrame(static_cast<int>(rows + 5));

    auto time_storage = TimeIndexStorageFactory::createDenseFromZero(rows);
    auto tensor = TensorData::createTimeSeries2D(
            std::vector<float>(rows * cols, 1.0f), rows, cols, time_storage, tf);

    TensorToAnalogParams params;
    params.output_key_prefix = "out";

    auto analogs = tensorToAnalog(tensor, params, makeCtx());
    REQUIRE(analogs.size() == cols);

    for (auto const & analog: analogs) {
        CHECK(analog->getTimeFrame() == tf);
    }
}

TEST_CASE("TensorToAnalog column selection works with zero-copy", "[TensorColumnAnalogStorage]") {
    constexpr std::size_t rows = 30;
    constexpr std::size_t cols = 5;

    auto tensor = makeTestTensor(rows, cols);

    TensorToAnalogParams params;
    params.output_key_prefix = "sel";
    params.columns = {1, 3};

    auto analogs = tensorToAnalog(*tensor, params, makeCtx());
    REQUIRE(analogs.size() == 2);

    // Verify column 1
    auto span1 = analogs[0]->getAnalogTimeSeries();
    REQUIRE(span1.size() == rows);
    CHECK(span1[0] == 1000.0f);
    CHECK(span1[5] == 1005.0f);

    // Verify column 3
    auto span3 = analogs[1]->getAnalogTimeSeries();
    REQUIRE(span3.size() == rows);
    CHECK(span3[0] == 3000.0f);
    CHECK(span3[5] == 3005.0f);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("TensorColumnAnalogStorage single column tensor", "[TensorColumnAnalogStorage]") {
    auto tensor = makeTestTensor(25, 1);
    TensorColumnAnalogStorage const storage(tensor, 0);

    CHECK(storage.size() == 25);
    auto span = storage.getSpan();
    REQUIRE(span.size() == 25);
    for (std::size_t i = 0; i < 25; ++i) {
        CHECK(span[i] == static_cast<float>(i));
    }
}

TEST_CASE("TensorColumnAnalogStorage single row tensor", "[TensorColumnAnalogStorage]") {
    auto tensor = makeTestTensor(1, 4);

    for (std::size_t c = 0; c < 4; ++c) {
        TensorColumnAnalogStorage const storage(tensor, c);
        CHECK(storage.size() == 1);
        CHECK(storage.getValueAt(0) == static_cast<float>(c * 1000));
    }
}

TEST_CASE("TensorColumnAnalogStorage accessor methods", "[TensorColumnAnalogStorage]") {
    auto tensor = makeTestTensor(10, 3);
    TensorColumnAnalogStorage const storage(tensor, 2);

    CHECK(storage.tensor() == tensor);
    CHECK(storage.columnIndex() == 2);
    CHECK(storage.data() != nullptr);
}
