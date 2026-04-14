/**
 * @file TensorTSNE.test.cpp
 * @brief Catch2 tests for the TensorTSNE container transform
 *
 * Tests the TransformsV2 TensorTSNE wrapper including:
 * - Container transform execution via ElementRegistry
 * - RowDescriptor preservation (TimeFrameIndex, Interval, Ordinal)
 * - Output shape and column names
 * - Input validation
 */

#include "TransformsV2/algorithms/TensorTSNE/TensorTSNE.hpp"

#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
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
 * @brief Create a 2D tensor with two separable clusters
 *
 * 30 observations × 4 features. First 15 near origin, last 15 offset.
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

TEST_CASE("TensorTSNE is registered in ElementRegistry", "[TransformsV2][TensorTSNE]") {
    auto & registry = ElementRegistry::instance();
    REQUIRE(registry.isContainerTransform("TensorTSNE"));
}

// ============================================================================
// Basic execution — ordinal rows
// ============================================================================

TEST_CASE("TensorTSNE reduces ordinal tensor", "[TransformsV2][TensorTSNE]") {
    auto & registry = ElementRegistry::instance();
    auto input = makeOrdinalTestTensor();
    ComputeContext const ctx;

    TensorTSNEParams params;
    params.n_components = 2;
    params.perplexity = 5.0;

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorTSNEParams>(
            "TensorTSNE", input, params, ctx);

    REQUIRE(result != nullptr);

    SECTION("output shape is (30, 2)") {
        REQUIRE(result->ndim() == 2);
        REQUIRE(result->numRows() == 30);
        REQUIRE(result->numColumns() == 2);
    }

    SECTION("output row type is Ordinal") {
        REQUIRE(result->rowType() == RowType::Ordinal);
    }

    SECTION("output has tSNE column names") {
        REQUIRE(result->hasNamedColumns());
        auto names = result->columnNames();
        REQUIRE(names.size() == 2);
        REQUIRE(names[0] == "tSNE1");
        REQUIRE(names[1] == "tSNE2");
    }
}

// ============================================================================
// RowDescriptor preservation — TimeFrameIndex
// ============================================================================

TEST_CASE("TensorTSNE preserves TimeFrameIndex rows", "[TransformsV2][TensorTSNE]") {
    auto & registry = ElementRegistry::instance();
    auto input = makeTimeSeriesTestTensor();
    ComputeContext const ctx;

    TensorTSNEParams params;
    params.n_components = 2;
    params.perplexity = 5.0;

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorTSNEParams>(
            "TensorTSNE", input, params, ctx);

    REQUIRE(result != nullptr);
    REQUIRE(result->rowType() == RowType::TimeFrameIndex);
    REQUIRE(result->numRows() == input.numRows());
    REQUIRE(result->getTimeFrame() != nullptr);
}

// ============================================================================
// Edge cases: validation
// ============================================================================

TEST_CASE("TensorTSNE rejects tensor with too few rows", "[TransformsV2][TensorTSNE]") {
    auto & registry = ElementRegistry::instance();
    ComputeContext const ctx;

    std::vector<float> const data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    auto input = TensorData::createOrdinal2D(data, 2, 3);

    TensorTSNEParams params;
    params.n_components = 2;
    params.perplexity = 1.0;

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorTSNEParams>(
            "TensorTSNE", input, params, ctx);

    REQUIRE(result == nullptr);
}

// ============================================================================
// Progress reporting
// ============================================================================

TEST_CASE("TensorTSNE reports progress", "[TransformsV2][TensorTSNE]") {
    auto input = makeOrdinalTestTensor();
    ComputeContext ctx;

    std::vector<int> progress_values;
    ctx.progress = [&](int p) {
        progress_values.push_back(p);
    };

    TensorTSNEParams params;
    params.n_components = 2;
    params.perplexity = 5.0;

    auto result = tensorTSNE(input, params, ctx);
    REQUIRE(result != nullptr);

    REQUIRE(progress_values.size() >= 3);
    REQUIRE(progress_values.front() == 0);
    REQUIRE(progress_values.back() == 100);
}

// ============================================================================
// Cancellation
// ============================================================================

TEST_CASE("TensorTSNE respects cancellation", "[TransformsV2][TensorTSNE]") {
    auto input = makeOrdinalTestTensor();
    ComputeContext ctx;

    std::atomic<bool> cancel_flag{true};
    ctx.is_cancelled = [&]() -> bool { return cancel_flag.load(); };

    TensorTSNEParams params;
    params.n_components = 2;

    auto result = tensorTSNE(input, params, ctx);
    REQUIRE(result == nullptr);
}

// ============================================================================
// NaN policy tests
// ============================================================================

namespace {

/// Create a 30x4 ordinal tensor with NaN in rows 0 and 29
TensorData makeNaNOrdinalTensor_TSNE() {
    constexpr std::size_t rows = 30;
    constexpr std::size_t cols = 4;
    std::vector<float> data(rows * cols, 0.0f);
    float const nan_val = std::numeric_limits<float>::quiet_NaN();

    for (std::size_t r = 0; r < rows; ++r) {
        float const offset = (r < 15) ? 0.0f : 10.0f;
        for (std::size_t c = 0; c < cols; ++c) {
            data[r * cols + c] = offset + 0.1f * static_cast<float>(r * cols + c);
        }
    }
    data[0 * cols + 2] = nan_val;
    data[(rows - 1) * cols + 1] = nan_val;

    return TensorData::createOrdinal2D(data, rows, cols,
                                       {"f0", "f1", "f2", "f3"});
}

}// namespace

TEST_CASE("TensorTSNE NaN policy Fail returns nullptr", "[TransformsV2][TensorTSNE][nan]") {
    auto input = makeNaNOrdinalTensor_TSNE();
    ComputeContext const ctx;

    TensorTSNEParams params;
    params.n_components = 2;
    params.nan_policy = NaNPolicy::Fail;

    auto result = tensorTSNE(input, params, ctx);
    REQUIRE(result == nullptr);
}

TEST_CASE("TensorTSNE NaN policy Propagate preserves row count", "[TransformsV2][TensorTSNE][nan]") {
    auto input = makeNaNOrdinalTensor_TSNE();
    ComputeContext const ctx;

    TensorTSNEParams params;
    params.n_components = 2;
    params.nan_policy = NaNPolicy::Propagate;

    auto result = tensorTSNE(input, params, ctx);
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 30);

    // Rows 0 and 29 should be NaN
    auto flat = result->materializeFlat();
    std::size_t const out_cols = result->numColumns();
    CHECK(std::isnan(flat[0]));
    CHECK(std::isnan(flat[29 * out_cols]));

    // Row 1 should be finite
    CHECK(std::isfinite(flat[1 * out_cols]));
}

TEST_CASE("TensorTSNE NaN policy Drop reduces row count", "[TransformsV2][TensorTSNE][nan]") {
    auto input = makeNaNOrdinalTensor_TSNE();
    ComputeContext const ctx;

    TensorTSNEParams params;
    params.n_components = 2;
    params.nan_policy = NaNPolicy::Drop;

    auto result = tensorTSNE(input, params, ctx);
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 28);// 30 - 2 NaN rows

    auto flat = result->materializeFlat();
    for (std::size_t i = 0; i < flat.size(); ++i) {
        CHECK(std::isfinite(flat[i]));
    }
}
