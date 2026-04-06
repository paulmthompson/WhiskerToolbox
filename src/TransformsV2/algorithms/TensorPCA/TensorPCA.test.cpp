/**
 * @file TensorPCA.test.cpp
 * @brief Catch2 tests for the TensorPCA container transform
 *
 * Tests the TransformsV2 TensorPCA wrapper including:
 * - Container transform execution via ElementRegistry
 * - RowDescriptor preservation (TimeFrameIndex, Interval, Ordinal)
 * - Output shape and column names
 * - Input validation (non-2D, too few rows, bad n_components)
 * - Default parameter behavior
 */

#include "TransformsV2/algorithms/TensorPCA/TensorPCA.hpp"

#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"

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
 * @brief Create a 2D tensor with high-variance feature 0 and low-variance feature 1-3
 *
 * 20 observations × 4 features. Feature 0 has values [0, 19], others are small.
 * PCA should capture most variance in PC1.
 */
TensorData makeOrdinalTestTensor() {
    constexpr std::size_t rows = 20;
    constexpr std::size_t cols = 4;
    std::vector<float> data(rows * cols, 0.0f);

    for (std::size_t r = 0; r < rows; ++r) {
        data[r * cols + 0] = static_cast<float>(r);        // high variance
        data[r * cols + 1] = 0.01f * static_cast<float>(r);// low variance
        data[r * cols + 2] = 0.001f;                       // near-constant
        data[r * cols + 3] = -0.001f;                      // near-constant
    }

    return TensorData::createOrdinal2D(data, rows, cols,
                                       {"feat_a", "feat_b", "feat_c", "feat_d"});
}

TensorData makeTimeSeriesTestTensor() {
    constexpr std::size_t rows = 20;
    constexpr std::size_t cols = 4;
    std::vector<float> data(rows * cols, 0.0f);

    for (std::size_t r = 0; r < rows; ++r) {
        data[r * cols + 0] = static_cast<float>(r);
        data[r * cols + 1] = 0.01f * static_cast<float>(r);
        data[r * cols + 2] = 0.001f;
        data[r * cols + 3] = -0.001f;
    }

    auto ts = makeDenseTimeStorage(rows);
    auto tf = makeTimeFrame(static_cast<int>(rows + 10));

    return TensorData::createTimeSeries2D(
            data, rows, cols, ts, tf, {"feat_a", "feat_b", "feat_c", "feat_d"});
}

TensorData makeIntervalTestTensor() {
    constexpr std::size_t rows = 20;
    constexpr std::size_t cols = 4;
    std::vector<float> data(rows * cols, 0.0f);

    for (std::size_t r = 0; r < rows; ++r) {
        data[r * cols + 0] = static_cast<float>(r);
        data[r * cols + 1] = 0.01f * static_cast<float>(r);
        data[r * cols + 2] = 0.001f;
        data[r * cols + 3] = -0.001f;
    }

    std::vector<TimeFrameInterval> intervals;
    for (std::size_t i = 0; i < rows; ++i) {
        auto start = TimeFrameIndex(static_cast<int64_t>(i * 10));
        auto end = TimeFrameIndex(static_cast<int64_t>(i * 10 + 9));
        intervals.push_back({start, end});
    }
    auto tf = makeTimeFrame(static_cast<int>(rows * 10 + 20));

    return TensorData::createFromIntervals(
            data, rows, cols, intervals, tf, {"feat_a", "feat_b", "feat_c", "feat_d"});
}

}// namespace

// ============================================================================
// Container transform registration
// ============================================================================

TEST_CASE("TensorPCA is registered in ElementRegistry", "[TransformsV2][TensorPCA]") {
    auto & registry = ElementRegistry::instance();
    REQUIRE(registry.isContainerTransform("TensorPCA"));
}

// ============================================================================
// Basic execution — ordinal rows
// ============================================================================

TEST_CASE("TensorPCA reduces ordinal tensor", "[TransformsV2][TensorPCA]") {
    auto & registry = ElementRegistry::instance();
    auto input = makeOrdinalTestTensor();
    ComputeContext const ctx;

    TensorPCAParams params;
    params.n_components = 2;

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorPCAParams>(
            "TensorPCA", input, params, ctx);

    REQUIRE(result != nullptr);

    SECTION("output shape is (20, 2)") {
        REQUIRE(result->ndim() == 2);
        REQUIRE(result->numRows() == 20);
        REQUIRE(result->numColumns() == 2);
    }

    SECTION("output row type is Ordinal") {
        REQUIRE(result->rowType() == RowType::Ordinal);
    }

    SECTION("output has PC column names") {
        REQUIRE(result->hasNamedColumns());
        auto names = result->columnNames();
        REQUIRE(names.size() == 2);
        REQUIRE(names[0] == "PC1");
        REQUIRE(names[1] == "PC2");
    }
}

// ============================================================================
// RowDescriptor preservation — TimeFrameIndex
// ============================================================================

TEST_CASE("TensorPCA preserves TimeFrameIndex rows", "[TransformsV2][TensorPCA]") {
    auto & registry = ElementRegistry::instance();
    auto input = makeTimeSeriesTestTensor();
    ComputeContext const ctx;

    TensorPCAParams params;
    params.n_components = 2;

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorPCAParams>(
            "TensorPCA", input, params, ctx);

    REQUIRE(result != nullptr);
    REQUIRE(result->rowType() == RowType::TimeFrameIndex);
    REQUIRE(result->numRows() == input.numRows());
    REQUIRE(result->getTimeFrame() != nullptr);
}

// ============================================================================
// RowDescriptor preservation — Interval
// ============================================================================

TEST_CASE("TensorPCA preserves Interval rows", "[TransformsV2][TensorPCA]") {
    auto & registry = ElementRegistry::instance();
    auto input = makeIntervalTestTensor();
    ComputeContext const ctx;

    TensorPCAParams params;
    params.n_components = 2;

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorPCAParams>(
            "TensorPCA", input, params, ctx);

    REQUIRE(result != nullptr);
    REQUIRE(result->rowType() == RowType::Interval);
    REQUIRE(result->numRows() == input.numRows());
    REQUIRE(result->getTimeFrame() != nullptr);
}

// ============================================================================
// Default parameters
// ============================================================================

TEST_CASE("TensorPCA uses default n_components=2 when unset", "[TransformsV2][TensorPCA]") {
    auto & registry = ElementRegistry::instance();
    auto input = makeOrdinalTestTensor();// 4 features
    ComputeContext const ctx;

    TensorPCAParams const params;
    // leave n_components and scale unset

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorPCAParams>(
            "TensorPCA", input, params, ctx);

    REQUIRE(result != nullptr);
    REQUIRE(result->numColumns() == 2);
}

// ============================================================================
// Edge cases: validation
// ============================================================================

TEST_CASE("TensorPCA rejects n_components > num_cols", "[TransformsV2][TensorPCA]") {
    auto & registry = ElementRegistry::instance();
    auto input = makeOrdinalTestTensor();// 4 features
    ComputeContext const ctx;

    TensorPCAParams params;
    params.n_components = 10;// > 4

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorPCAParams>(
            "TensorPCA", input, params, ctx);

    REQUIRE(result == nullptr);
}

TEST_CASE("TensorPCA rejects tensor with 1 row", "[TransformsV2][TensorPCA]") {
    auto & registry = ElementRegistry::instance();
    ComputeContext const ctx;

    std::vector<float> const data{1.0f, 2.0f, 3.0f};
    auto input = TensorData::createOrdinal2D(data, 1, 3);

    TensorPCAParams params;
    params.n_components = 2;

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorPCAParams>(
            "TensorPCA", input, params, ctx);

    REQUIRE(result == nullptr);
}

// ============================================================================
// Progress reporting
// ============================================================================

TEST_CASE("TensorPCA reports progress", "[TransformsV2][TensorPCA]") {
    auto input = makeOrdinalTestTensor();
    ComputeContext ctx;

    std::vector<int> progress_values;
    ctx.progress = [&](int p) {
        progress_values.push_back(p);
    };

    TensorPCAParams params;
    params.n_components = 2;

    auto result = tensorPCA(input, params, ctx);
    REQUIRE(result != nullptr);

    // Should have reported 0, 10, 80, 100
    REQUIRE(progress_values.size() >= 3);
    REQUIRE(progress_values.front() == 0);
    REQUIRE(progress_values.back() == 100);
}

// ============================================================================
// Cancellation
// ============================================================================

TEST_CASE("TensorPCA respects cancellation", "[TransformsV2][TensorPCA]") {
    auto input = makeOrdinalTestTensor();
    ComputeContext ctx;

    std::atomic<bool> cancel_flag{true};
    ctx.is_cancelled = [&]() -> bool { return cancel_flag.load(); };

    TensorPCAParams params;
    params.n_components = 2;

    auto result = tensorPCA(input, params, ctx);
    REQUIRE(result == nullptr);
}
