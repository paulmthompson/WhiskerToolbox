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
#include "TransformsV2/algorithms/TensorTemporalNeighbors/TensorTemporalNeighbors.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

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

// ============================================================================
// Integration: TensorTemporalNeighbors (NaN) -> TensorPCA must not crash
// ============================================================================

TEST_CASE("TensorPCA after TensorTemporalNeighbors NaN uses Propagate by default",
          "[TransformsV2][TensorPCA][integration]") {

    // Simulate the user's workflow: 6001 x 192 feature tensor, but use a
    // smaller tensor (50 x 4) that reproduces the same failure mode.
    // TensorTemporalNeighbors with NaN boundary produces columns containing
    // NaN at the edges. With Propagate (default), PCA skips NaN rows and
    // fills them with NaN in the output.

    constexpr std::size_t rows = 50;
    constexpr std::size_t cols = 4;
    std::vector<float> data(rows * cols);
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            data[r * cols + c] = static_cast<float>(r * cols + c);
        }
    }

    auto ts = makeDenseTimeStorage(rows);
    auto tf = makeTimeFrame(static_cast<int>(rows + 10));
    auto input = std::make_shared<TensorData>(
            TensorData::createTimeSeries2D(data, rows, cols, ts, tf,
                                           {"f0", "f1", "f2", "f3"}));

    // Step 1: TensorTemporalNeighbors with NaN boundary + lead_range=1
    // This adds NaN-filled columns for out-of-bounds rows.
    DataTypeVariant const input_var{input};

    TransformPipeline pipeline;
    {
        TensorTemporalNeighborParams tn_params;
        tn_params.lag_range = 0;
        tn_params.lag_step = 1;
        tn_params.lead_range = 1;
        tn_params.lead_step = 1;
        tn_params.boundary_policy = BoundaryPolicy::NaN;
        tn_params.include_original = true;
        pipeline.addStep("TensorTemporalNeighbors", tn_params);
    }

    // Execute step 1 — should succeed
    auto step1_result = executePipeline(input_var, pipeline);
    auto const * tensor1_ptr = std::get_if<std::shared_ptr<TensorData>>(&step1_result);
    REQUIRE(tensor1_ptr != nullptr);
    REQUIRE(*tensor1_ptr != nullptr);

    // Verify NaN is present (last row, lead columns)
    auto flat1 = (*tensor1_ptr)->materializeFlat();
    std::size_t const out_cols = (*tensor1_ptr)->numColumns();
    // Last row should have NaN in the lead columns
    bool has_nan = false;
    for (std::size_t c = cols; c < out_cols; ++c) {
        if (std::isnan(flat1[(rows - 1) * out_cols + c])) {
            has_nan = true;
            break;
        }
    }
    REQUIRE(has_nan);

    // Step 2: TensorPCA on the NaN-containing output — Propagate skips NaN rows
    TransformPipeline pca_pipeline;
    {
        TensorPCAParams pca_params;
        pca_params.n_components = 2;
        // nan_policy defaults to Propagate
        pca_pipeline.addStep("TensorPCA", pca_params);
    }

    auto pca_result = executePipeline(step1_result, pca_pipeline);
    auto const * pca_ptr = std::get_if<std::shared_ptr<TensorData>>(&pca_result);
    REQUIRE(pca_ptr != nullptr);
    REQUIRE(*pca_ptr != nullptr);

    // Propagate: same row count as input, NaN rows get NaN-filled output
    CHECK((*pca_ptr)->numRows() == rows);
    CHECK((*pca_ptr)->numColumns() == 2);

    // Last row should have NaN in at least one output column (was NaN in input)
    auto pca_flat = (*pca_ptr)->materializeFlat();
    std::size_t const pca_cols = (*pca_ptr)->numColumns();
    bool last_row_has_nan = false;
    for (std::size_t c = 0; c < pca_cols; ++c) {
        if (std::isnan(pca_flat[(rows - 1) * pca_cols + c])) {
            last_row_has_nan = true;
            break;
        }
    }
    CHECK(last_row_has_nan);
}

TEST_CASE("TensorPCA after TensorTemporalNeighbors Drop succeeds",
          "[TransformsV2][TensorPCA][integration]") {

    // Same workflow but with Drop boundary policy — no NaN, PCA should succeed.
    constexpr std::size_t rows = 50;
    constexpr std::size_t cols = 4;
    std::vector<float> data(rows * cols);
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            data[r * cols + c] = static_cast<float>(r * cols + c);
        }
    }

    auto ts = makeDenseTimeStorage(rows);
    auto tf = makeTimeFrame(static_cast<int>(rows + 10));
    auto input = std::make_shared<TensorData>(
            TensorData::createTimeSeries2D(data, rows, cols, ts, tf,
                                           {"f0", "f1", "f2", "f3"}));

    // Step 1: TensorTemporalNeighbors with Drop boundary
    DataTypeVariant const input_var{input};

    TransformPipeline tn_pipeline;
    {
        TensorTemporalNeighborParams tn_params;
        tn_params.lag_range = 1;
        tn_params.lag_step = 1;
        tn_params.lead_range = 1;
        tn_params.lead_step = 1;
        tn_params.boundary_policy = BoundaryPolicy::Drop;
        tn_params.include_original = true;
        tn_pipeline.addStep("TensorTemporalNeighbors", tn_params);
    }

    auto step1_result = executePipeline(input_var, tn_pipeline);
    auto const * tensor1_ptr = std::get_if<std::shared_ptr<TensorData>>(&step1_result);
    REQUIRE(tensor1_ptr != nullptr);
    REQUIRE(*tensor1_ptr != nullptr);
    // Drop boundary removes first and last row
    REQUIRE((*tensor1_ptr)->numRows() == rows - 2);

    // Step 2: TensorPCA on clean data — should succeed
    TransformPipeline pca_pipeline;
    {
        TensorPCAParams pca_params;
        pca_params.n_components = 2;
        pca_pipeline.addStep("TensorPCA", pca_params);
    }

    auto pca_result = executePipeline(step1_result, pca_pipeline);
    auto const * pca_ptr = std::get_if<std::shared_ptr<TensorData>>(&pca_result);
    REQUIRE(pca_ptr != nullptr);
    REQUIRE(*pca_ptr != nullptr);
    CHECK((*pca_ptr)->numRows() == rows - 2);
    CHECK((*pca_ptr)->numColumns() == 2);
}

// ============================================================================
// NaN policy: direct unit tests
// ============================================================================

namespace {

/// Create a 20x4 ordinal tensor with NaN in rows 0 and 19
TensorData makeNaNOrdinalTensor() {
    constexpr std::size_t rows = 20;
    constexpr std::size_t cols = 4;
    std::vector<float> data(rows * cols, 0.0f);
    float const nan_val = std::numeric_limits<float>::quiet_NaN();

    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            data[r * cols + c] = static_cast<float>(r * cols + c);
        }
    }
    // Inject NaN in first and last row
    data[0 * cols + 2] = nan_val;
    data[(rows - 1) * cols + 1] = nan_val;

    return TensorData::createOrdinal2D(data, rows, cols,
                                       {"f0", "f1", "f2", "f3"});
}

/// Create a 20x4 time-series tensor with NaN in rows 0 and 19
TensorData makeNaNTimeSeriesTensor() {
    constexpr std::size_t rows = 20;
    constexpr std::size_t cols = 4;
    std::vector<float> data(rows * cols, 0.0f);
    float const nan_val = std::numeric_limits<float>::quiet_NaN();

    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            data[r * cols + c] = static_cast<float>(r * cols + c);
        }
    }
    data[0 * cols + 2] = nan_val;
    data[(rows - 1) * cols + 1] = nan_val;

    auto ts = makeDenseTimeStorage(rows);
    auto tf = makeTimeFrame(static_cast<int>(rows + 10));
    return TensorData::createTimeSeries2D(data, rows, cols, ts, tf,
                                          {"f0", "f1", "f2", "f3"});
}

}// namespace

TEST_CASE("TensorPCA NaN policy Fail returns nullptr", "[TransformsV2][TensorPCA][nan]") {
    auto input = makeNaNOrdinalTensor();
    ComputeContext const ctx;

    TensorPCAParams params;
    params.n_components = 2;
    params.nan_policy = NaNPolicy::Fail;

    auto result = tensorPCA(input, params, ctx);
    REQUIRE(result == nullptr);
}

TEST_CASE("TensorPCA NaN policy Propagate preserves row count", "[TransformsV2][TensorPCA][nan]") {
    auto input = makeNaNOrdinalTensor();
    ComputeContext const ctx;

    TensorPCAParams params;
    params.n_components = 2;
    params.nan_policy = NaNPolicy::Propagate;

    auto result = tensorPCA(input, params, ctx);
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 20);
    CHECK(result->numColumns() == 2);

    // Rows 0 and 19 should be NaN
    auto flat = result->materializeFlat();
    CHECK(std::isnan(flat[0]));         // row 0, col 0
    CHECK(std::isnan(flat[1]));         // row 0, col 1
    CHECK(std::isnan(flat[19 * 2]));    // row 19, col 0
    CHECK(std::isnan(flat[19 * 2 + 1]));// row 19, col 1

    // Row 1 should be finite
    CHECK(std::isfinite(flat[1 * 2]));
    CHECK(std::isfinite(flat[1 * 2 + 1]));
}

TEST_CASE("TensorPCA NaN policy Drop reduces row count", "[TransformsV2][TensorPCA][nan]") {
    auto input = makeNaNOrdinalTensor();
    ComputeContext const ctx;

    TensorPCAParams params;
    params.n_components = 2;
    params.nan_policy = NaNPolicy::Drop;

    auto result = tensorPCA(input, params, ctx);
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 18);// 20 - 2 NaN rows
    CHECK(result->numColumns() == 2);

    // All output values should be finite
    auto flat = result->materializeFlat();
    for (std::size_t i = 0; i < flat.size(); ++i) {
        CHECK(std::isfinite(flat[i]));
    }
}

TEST_CASE("TensorPCA NaN policy Propagate preserves TimeFrameIndex", "[TransformsV2][TensorPCA][nan]") {
    auto input = makeNaNTimeSeriesTensor();
    ComputeContext const ctx;

    TensorPCAParams params;
    params.n_components = 2;
    params.nan_policy = NaNPolicy::Propagate;

    auto result = tensorPCA(input, params, ctx);
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 20);
    CHECK(result->rowType() == RowType::TimeFrameIndex);
    CHECK(result->getTimeFrame() != nullptr);
}

TEST_CASE("TensorPCA NaN policy Drop filters TimeFrameIndex", "[TransformsV2][TensorPCA][nan]") {
    auto input = makeNaNTimeSeriesTensor();
    ComputeContext const ctx;

    TensorPCAParams params;
    params.n_components = 2;
    params.nan_policy = NaNPolicy::Drop;

    auto result = tensorPCA(input, params, ctx);
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 18);
    CHECK(result->rowType() == RowType::TimeFrameIndex);
    CHECK(result->getTimeFrame() != nullptr);
}

TEST_CASE("TensorPCA clean data bypasses NaN filter", "[TransformsV2][TensorPCA][nan]") {
    // No NaN — should work identically regardless of policy
    auto input = makeOrdinalTestTensor();
    ComputeContext const ctx;

    TensorPCAParams params;
    params.n_components = 2;
    params.nan_policy = NaNPolicy::Propagate;

    auto result = tensorPCA(input, params, ctx);
    REQUIRE(result != nullptr);
    CHECK(result->numRows() == 20);
    CHECK(result->numColumns() == 2);
}
