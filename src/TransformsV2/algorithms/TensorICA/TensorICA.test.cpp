/**
 * @file TensorICA.test.cpp
 * @brief Catch2 tests for the TensorICA container transform
 *
 * Tests the TransformsV2 TensorICA wrapper including:
 * - Container transform registration in ElementRegistry
 * - RowDescriptor preservation (TimeFrameIndex, Interval, Ordinal)
 * - Output shape and column names
 * - Input validation (non-2D, too few rows, too few cols)
 * - Progress reporting and cancellation
 */

#include "TransformsV2/algorithms/TensorICA/TensorICA.hpp"

#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
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

std::shared_ptr<TimeFrame> makeTimeFrame(int count) {
    std::vector<int> times(count);
    for (int i = 0; i < count; ++i) {
        times[i] = i;
    }
    return std::make_shared<TimeFrame>(times);
}

std::shared_ptr<TimeIndexStorage> makeDenseTimeStorage(std::size_t count) {
    return TimeIndexStorageFactory::createDenseFromZero(count);
}

/**
 * @brief Create a 2D tensor with mixed signals suitable for ICA
 *
 * 50 observations × 3 features. Simulates a simple mixing of signals.
 */
TensorData makeOrdinalTestTensor() {
    constexpr std::size_t rows = 50;
    constexpr std::size_t cols = 3;
    std::vector<float> data(rows * cols, 0.0f);

    for (std::size_t r = 0; r < rows; ++r) {
        auto const t = static_cast<float>(r) / static_cast<float>(rows);
        // Simple mixed signals
        data[r * cols + 0] = 2.0f * t + 0.5f * (1.0f - t);
        data[r * cols + 1] = 0.3f * t + 1.5f * (1.0f - t);
        data[r * cols + 2] = 1.0f * t + 0.8f * (1.0f - t);
    }

    return TensorData::createOrdinal2D(data, rows, cols,
                                       {"sig_a", "sig_b", "sig_c"});
}

TensorData makeTimeSeriesTestTensor() {
    constexpr std::size_t rows = 50;
    constexpr std::size_t cols = 3;
    std::vector<float> data(rows * cols, 0.0f);

    for (std::size_t r = 0; r < rows; ++r) {
        auto const t = static_cast<float>(r) / static_cast<float>(rows);
        data[r * cols + 0] = 2.0f * t + 0.5f * (1.0f - t);
        data[r * cols + 1] = 0.3f * t + 1.5f * (1.0f - t);
        data[r * cols + 2] = 1.0f * t + 0.8f * (1.0f - t);
    }

    auto ts = makeDenseTimeStorage(rows);
    auto tf = makeTimeFrame(static_cast<int>(rows + 10));

    return TensorData::createTimeSeries2D(
            data, rows, cols, ts, tf, {"sig_a", "sig_b", "sig_c"});
}

TensorData makeIntervalTestTensor() {
    constexpr std::size_t rows = 50;
    constexpr std::size_t cols = 3;
    std::vector<float> data(rows * cols, 0.0f);

    for (std::size_t r = 0; r < rows; ++r) {
        auto const t = static_cast<float>(r) / static_cast<float>(rows);
        data[r * cols + 0] = 2.0f * t + 0.5f * (1.0f - t);
        data[r * cols + 1] = 0.3f * t + 1.5f * (1.0f - t);
        data[r * cols + 2] = 1.0f * t + 0.8f * (1.0f - t);
    }

    std::vector<TimeFrameInterval> intervals;
    for (std::size_t i = 0; i < rows; ++i) {
        auto start = TimeFrameIndex(static_cast<int64_t>(i * 10));
        auto end = TimeFrameIndex(static_cast<int64_t>(i * 10 + 9));
        intervals.push_back({start, end});
    }
    auto tf = makeTimeFrame(static_cast<int>(rows * 10 + 20));

    return TensorData::createFromIntervals(
            data, rows, cols, intervals, tf, {"sig_a", "sig_b", "sig_c"});
}

}// namespace

// ============================================================================
// Container transform registration
// ============================================================================

TEST_CASE("TensorICA is registered in ElementRegistry", "[TransformsV2][TensorICA]") {
    auto & registry = ElementRegistry::instance();
    REQUIRE(registry.isContainerTransform("TensorICA"));
}

// ============================================================================
// Basic execution — ordinal rows
// ============================================================================

TEST_CASE("TensorICA transforms ordinal tensor", "[TransformsV2][TensorICA]") {
    auto & registry = ElementRegistry::instance();
    auto input = makeOrdinalTestTensor();
    ComputeContext const ctx;

    TensorICAParams params;

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorICAParams>(
            "TensorICA", input, params, ctx);

    REQUIRE(result != nullptr);

    SECTION("output shape matches input (ICA does not reduce dims)") {
        REQUIRE(result->ndim() == 2);
        REQUIRE(result->numRows() == 50);
        REQUIRE(result->numColumns() == 3);
    }

    SECTION("output row type is Ordinal") {
        REQUIRE(result->rowType() == RowType::Ordinal);
    }

    SECTION("output has IC column names") {
        REQUIRE(result->hasNamedColumns());
        auto names = result->columnNames();
        REQUIRE(names.size() == 3);
        REQUIRE(names[0] == "IC1");
        REQUIRE(names[1] == "IC2");
        REQUIRE(names[2] == "IC3");
    }
}

// ============================================================================
// RowDescriptor preservation — TimeFrameIndex
// ============================================================================

TEST_CASE("TensorICA preserves TimeFrameIndex rows", "[TransformsV2][TensorICA]") {
    auto & registry = ElementRegistry::instance();
    auto input = makeTimeSeriesTestTensor();
    ComputeContext const ctx;

    TensorICAParams params;

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorICAParams>(
            "TensorICA", input, params, ctx);

    REQUIRE(result != nullptr);
    REQUIRE(result->rowType() == RowType::TimeFrameIndex);
    REQUIRE(result->numRows() == input.numRows());
    REQUIRE(result->getTimeFrame() != nullptr);
}

// ============================================================================
// RowDescriptor preservation — Interval
// ============================================================================

TEST_CASE("TensorICA preserves Interval rows", "[TransformsV2][TensorICA]") {
    auto & registry = ElementRegistry::instance();
    auto input = makeIntervalTestTensor();
    ComputeContext const ctx;

    TensorICAParams params;

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorICAParams>(
            "TensorICA", input, params, ctx);

    REQUIRE(result != nullptr);
    REQUIRE(result->rowType() == RowType::Interval);
    REQUIRE(result->numRows() == input.numRows());
    REQUIRE(result->getTimeFrame() != nullptr);
}

// ============================================================================
// Edge cases: validation
// ============================================================================

TEST_CASE("TensorICA rejects tensor with 1 row", "[TransformsV2][TensorICA]") {
    auto & registry = ElementRegistry::instance();
    ComputeContext const ctx;

    std::vector<float> const data{1.0f, 2.0f, 3.0f};
    auto input = TensorData::createOrdinal2D(data, 1, 3);

    TensorICAParams params;

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorICAParams>(
            "TensorICA", input, params, ctx);

    REQUIRE(result == nullptr);
}

TEST_CASE("TensorICA rejects tensor with 1 column", "[TransformsV2][TensorICA]") {
    auto & registry = ElementRegistry::instance();
    ComputeContext const ctx;

    std::vector<float> const data{1.0f, 2.0f, 3.0f};
    auto input = TensorData::createOrdinal2D(data, 3, 1);

    TensorICAParams params;

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorICAParams>(
            "TensorICA", input, params, ctx);

    REQUIRE(result == nullptr);
}

// ============================================================================
// Progress reporting
// ============================================================================

TEST_CASE("TensorICA reports progress", "[TransformsV2][TensorICA]") {
    auto input = makeOrdinalTestTensor();
    ComputeContext ctx;

    std::vector<int> progress_values;
    ctx.progress = [&](int p) {
        progress_values.push_back(p);
    };

    TensorICAParams params;

    auto result = tensorICA(input, params, ctx);
    REQUIRE(result != nullptr);

    REQUIRE(progress_values.size() >= 3);
    REQUIRE(progress_values.front() == 0);
    REQUIRE(progress_values.back() == 100);
}

// ============================================================================
// Cancellation
// ============================================================================

TEST_CASE("TensorICA respects cancellation", "[TransformsV2][TensorICA]") {
    auto input = makeOrdinalTestTensor();
    ComputeContext ctx;

    std::atomic<bool> cancel_flag{true};
    ctx.is_cancelled = [&]() -> bool { return cancel_flag.load(); };

    TensorICAParams params;

    auto result = tensorICA(input, params, ctx);
    REQUIRE(result == nullptr);
}
