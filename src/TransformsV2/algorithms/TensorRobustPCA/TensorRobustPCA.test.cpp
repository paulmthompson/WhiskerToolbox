/**
 * @file TensorRobustPCA.test.cpp
 * @brief Catch2 tests for the TensorRobustPCA container transform
 *
 * Tests the TransformsV2 TensorRobustPCA wrapper including:
 * - Container transform execution via ElementRegistry
 * - RowDescriptor preservation (TimeFrameIndex, Ordinal)
 * - Output shape and column names
 * - Input validation
 * - Progress/cancellation
 */

#include "TransformsV2/algorithms/TensorRobustPCA/TensorRobustPCA.hpp"

#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"

#include <catch2/catch_test_macros.hpp>

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
 * @brief Create a 2D tensor with low-rank structure plus sparse outliers
 *
 * 30 observations x 4 features. First 15 near origin, last 15 offset.
 */
TensorData makeOrdinalTestTensor() {
    constexpr std::size_t rows = 30;
    constexpr std::size_t cols = 4;
    std::vector<float> data(rows * cols, 0.0f);

    for (std::size_t r = 0; r < rows; ++r) {
        float const offset = (r < 15) ? 0.0f : 10.0f;
        data[r * cols + 0] = offset + 0.1f * static_cast<float>(r);
        data[r * cols + 1] = offset + 0.05f * static_cast<float>(r);
        data[r * cols + 2] = 0.01f * static_cast<float>(r);
        data[r * cols + 3] = -0.01f * static_cast<float>(r);
    }

    return TensorData::createOrdinal2D(data, rows, cols,
                                       {"feat_a", "feat_b", "feat_c", "feat_d"});
}

TensorData makeTimeSeriesTestTensor() {
    constexpr std::size_t rows = 30;
    constexpr std::size_t cols = 4;
    std::vector<float> data(rows * cols, 0.0f);

    for (std::size_t r = 0; r < rows; ++r) {
        float const offset = (r < 15) ? 0.0f : 10.0f;
        data[r * cols + 0] = offset + 0.1f * static_cast<float>(r);
        data[r * cols + 1] = offset + 0.05f * static_cast<float>(r);
        data[r * cols + 2] = 0.01f * static_cast<float>(r);
        data[r * cols + 3] = -0.01f * static_cast<float>(r);
    }

    auto ts = makeDenseTimeStorage(rows);
    auto tf = makeTimeFrame(static_cast<int>(rows + 10));

    return TensorData::createTimeSeries2D(
            data, rows, cols, ts, tf, {"feat_a", "feat_b", "feat_c", "feat_d"});
}

}// namespace

// ============================================================================
// Container transform registration
// ============================================================================

TEST_CASE("TensorRobustPCA is registered in ElementRegistry", "[TransformsV2][TensorRobustPCA]") {
    auto & registry = ElementRegistry::instance();
    REQUIRE(registry.isContainerTransform("TensorRobustPCA"));
}

// ============================================================================
// Basic execution — ordinal rows
// ============================================================================

TEST_CASE("TensorRobustPCA reduces ordinal tensor", "[TransformsV2][TensorRobustPCA]") {
    auto & registry = ElementRegistry::instance();
    auto input = makeOrdinalTestTensor();
    ComputeContext const ctx;

    TensorRobustPCAParams params;
    params.n_components = 2;

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorRobustPCAParams>(
            "TensorRobustPCA", input, params, ctx);

    REQUIRE(result != nullptr);

    SECTION("output shape is (30, 2)") {
        REQUIRE(result->ndim() == 2);
        REQUIRE(result->numRows() == 30);
        REQUIRE(result->numColumns() == 2);
    }

    SECTION("output row type is Ordinal") {
        REQUIRE(result->rowType() == RowType::Ordinal);
    }

    SECTION("output has RPCA column names") {
        REQUIRE(result->hasNamedColumns());
        auto names = result->columnNames();
        REQUIRE(names.size() == 2);
        REQUIRE(names[0] == "RPCA1");
        REQUIRE(names[1] == "RPCA2");
    }
}

// ============================================================================
// RowDescriptor preservation — TimeFrameIndex
// ============================================================================

TEST_CASE("TensorRobustPCA preserves TimeFrameIndex rows", "[TransformsV2][TensorRobustPCA]") {
    auto & registry = ElementRegistry::instance();
    auto input = makeTimeSeriesTestTensor();
    ComputeContext const ctx;

    TensorRobustPCAParams params;
    params.n_components = 2;

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorRobustPCAParams>(
            "TensorRobustPCA", input, params, ctx);

    REQUIRE(result != nullptr);
    REQUIRE(result->rowType() == RowType::TimeFrameIndex);
    REQUIRE(result->numRows() == input.numRows());
    REQUIRE(result->getTimeFrame() != nullptr);
}

// ============================================================================
// Edge cases: validation
// ============================================================================

TEST_CASE("TensorRobustPCA rejects tensor with too few rows", "[TransformsV2][TensorRobustPCA]") {
    auto & registry = ElementRegistry::instance();
    ComputeContext const ctx;

    std::vector<float> const data{1.0f, 2.0f, 3.0f};
    auto input = TensorData::createOrdinal2D(data, 1, 3);

    TensorRobustPCAParams params;
    params.n_components = 2;

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorRobustPCAParams>(
            "TensorRobustPCA", input, params, ctx);

    REQUIRE(result == nullptr);
}

TEST_CASE("TensorRobustPCA rejects n_components > features", "[TransformsV2][TensorRobustPCA]") {
    auto & registry = ElementRegistry::instance();
    ComputeContext const ctx;

    // 10 observations x 2 features
    std::vector<float> const data(20, 1.0f);
    auto input = TensorData::createOrdinal2D(data, 10, 2);

    TensorRobustPCAParams params;
    params.n_components = 5;// more than 2 features

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorRobustPCAParams>(
            "TensorRobustPCA", input, params, ctx);

    REQUIRE(result == nullptr);
}

// ============================================================================
// Progress reporting
// ============================================================================

TEST_CASE("TensorRobustPCA reports progress", "[TransformsV2][TensorRobustPCA]") {
    auto input = makeOrdinalTestTensor();
    ComputeContext ctx;

    std::vector<int> progress_values;
    ctx.progress = [&](int p) {
        progress_values.push_back(p);
    };

    TensorRobustPCAParams params;
    params.n_components = 2;

    auto result = tensorRobustPCA(input, params, ctx);
    REQUIRE(result != nullptr);

    REQUIRE(progress_values.size() >= 3);
    REQUIRE(progress_values.front() == 0);
    REQUIRE(progress_values.back() == 100);
}

// ============================================================================
// Cancellation
// ============================================================================

TEST_CASE("TensorRobustPCA respects cancellation", "[TransformsV2][TensorRobustPCA]") {
    auto input = makeOrdinalTestTensor();
    ComputeContext ctx;

    std::atomic<bool> cancel_flag{true};
    ctx.is_cancelled = [&]() -> bool { return cancel_flag.load(); };

    TensorRobustPCAParams params;
    params.n_components = 2;

    auto result = tensorRobustPCA(input, params, ctx);
    REQUIRE(result == nullptr);
}
