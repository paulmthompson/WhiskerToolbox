/**
 * @file TensorCAR.test.cpp
 * @brief Catch2 tests for the TensorCAR container transform
 *
 * Tests include:
 * - Registration check
 * - Mean CAR: row-wise mean across channels is approximately zero after transform
 * - Median CAR: row-wise median across channels is approximately zero after transform
 * - RowDescriptor preservation (TimeFrameIndex, Interval, Ordinal)
 * - Output shape and column name preservation
 * - Excluded channels: reference computed only from included channels
 * - Validation failures (1D input, zero rows, all channels excluded)
 */

#include "TransformsV2/algorithms/TensorCAR/TensorCAR.hpp"

#include "Tensors/RowDescriptor.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstddef>
#include <memory>
#include <numeric>
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

std::shared_ptr<TimeIndexStorage> makeDenseTimeStorage(std::size_t count) {
    return TimeIndexStorageFactory::createDenseFromZero(count);
}

/**
 * @brief Create a (num_rows × num_channels) TensorData with known values.
 *
 * data[t, c] = base_values[c] + (float)t
 * so each channel is a ramp starting at base_values[c].
 */
TensorData makeTimeSeriesTensor(
        std::vector<float> const & base_values,
        std::size_t num_rows) {

    std::size_t const num_cols = base_values.size();
    std::vector<float> flat(num_rows * num_cols);
    for (std::size_t r = 0; r < num_rows; ++r) {
        for (std::size_t c = 0; c < num_cols; ++c) {
            flat[r * num_cols + c] = base_values[c] + static_cast<float>(r);
        }
    }
    auto ts = makeDenseTimeStorage(num_rows);
    auto tf = makeTimeFrame(static_cast<int>(num_rows + 10));
    return TensorData::createTimeSeries2D(
            flat, num_rows, num_cols, ts, tf, {});
}

TensorData makeOrdinalTensor(
        std::vector<float> const & base_values,
        std::size_t num_rows) {

    std::size_t const num_cols = base_values.size();
    std::vector<float> flat(num_rows * num_cols);
    for (std::size_t r = 0; r < num_rows; ++r) {
        for (std::size_t c = 0; c < num_cols; ++c) {
            flat[r * num_cols + c] = base_values[c] + static_cast<float>(r);
        }
    }
    return TensorData::createOrdinal2D(flat, num_rows, num_cols, {});
}

TensorData makeIntervalTensor(
        std::vector<float> const & base_values,
        std::size_t num_rows) {

    std::size_t const num_cols = base_values.size();
    std::vector<float> flat(num_rows * num_cols);
    for (std::size_t r = 0; r < num_rows; ++r) {
        for (std::size_t c = 0; c < num_cols; ++c) {
            flat[r * num_cols + c] = base_values[c] + static_cast<float>(r);
        }
    }
    std::vector<TimeFrameInterval> intervals;
    intervals.reserve(num_rows);
    for (std::size_t i = 0; i < num_rows; ++i) {
        auto start = TimeFrameIndex(static_cast<int64_t>(i * 10));
        auto end = TimeFrameIndex(static_cast<int64_t>(i * 10 + 9));
        intervals.push_back({start, end});
    }
    auto tf = makeTimeFrame(static_cast<int>(num_rows * 10 + 20));
    return TensorData::createFromIntervals(flat, num_rows, num_cols, intervals, tf, {});
}

/// Compute the row-wise mean of a TensorData (mean across columns for each row)
std::vector<float> rowMeans(TensorData const & td) {
    std::size_t const nr = td.numRows();
    std::size_t const nc = td.numColumns();
    auto flat = td.materializeFlat();
    std::vector<float> means(nr);
    for (std::size_t r = 0; r < nr; ++r) {
        float sum = 0.0F;
        for (std::size_t c = 0; c < nc; ++c) {
            sum += flat[r * nc + c];
        }
        means[r] = sum / static_cast<float>(nc);
    }
    return means;
}

/// Compute the row-wise median of a TensorData
std::vector<float> rowMedians(TensorData const & td) {
    std::size_t const nr = td.numRows();
    std::size_t const nc = td.numColumns();
    auto flat = td.materializeFlat();
    std::vector<float> medians(nr);
    std::vector<float> row_buf(nc);
    for (std::size_t r = 0; r < nr; ++r) {
        for (std::size_t c = 0; c < nc; ++c) {
            row_buf[c] = flat[r * nc + c];
        }
        std::sort(row_buf.begin(), row_buf.end());
        if (nc % 2 == 0) {
            medians[r] = (row_buf[nc / 2 - 1] + row_buf[nc / 2]) / 2.0F;
        } else {
            medians[r] = row_buf[nc / 2];
        }
    }
    return medians;
}

}// namespace

// ============================================================================
// Registration
// ============================================================================

TEST_CASE("TensorCAR is registered in ElementRegistry", "[TransformsV2][TensorCAR]") {
    auto & registry = ElementRegistry::instance();
    REQUIRE(registry.isContainerTransform("TensorCAR"));
}

// ============================================================================
// Mean CAR: row-wise mean of output ≈ 0
// ============================================================================

TEST_CASE("TensorCAR Mean removes row-wise mean", "[TransformsV2][TensorCAR]") {
    // 4 channels with offsets [0, 10, 100, 1000]; common ramp of t
    TensorData const input = makeTimeSeriesTensor({0.0F, 10.0F, 100.0F, 1000.0F}, 50);

    TensorCARParams params;
    params.method = CARMethod::Mean;

    auto result = tensorCAR(input, params, makeCtx());

    REQUIRE(result != nullptr);
    REQUIRE(result->ndim() == 2);
    REQUIRE(result->numRows() == 50);
    REQUIRE(result->numColumns() == 4);

    auto means = rowMeans(*result);
    for (float m: means) {
        REQUIRE_THAT(m, Catch::Matchers::WithinAbs(0.0F, 1e-4F));
    }
}

// ============================================================================
// Median CAR: row-wise median of output ≈ 0 (even number of channels → midpoint)
// ============================================================================

TEST_CASE("TensorCAR Median removes row-wise median", "[TransformsV2][TensorCAR]") {
    // 5 channels (odd count so median is exact) with offsets [0, 1, 2, 3, 4]
    TensorData const input = makeTimeSeriesTensor({0.0F, 1.0F, 2.0F, 3.0F, 4.0F}, 40);

    TensorCARParams params;
    params.method = CARMethod::Median;

    auto result = tensorCAR(input, params, makeCtx());

    REQUIRE(result != nullptr);

    auto medians = rowMedians(*result);
    for (float m: medians) {
        REQUIRE_THAT(m, Catch::Matchers::WithinAbs(0.0F, 1e-4F));
    }
}

// ============================================================================
// Default parameters: Median, no exclusions
// ============================================================================

TEST_CASE("TensorCAR default params use Median", "[TransformsV2][TensorCAR]") {
    TensorData const input = makeOrdinalTensor({0.0F, 1.0F, 2.0F, 3.0F, 4.0F}, 20);

    TensorCARParams const params;// default: Median, no exclusions

    auto result = tensorCAR(input, params, makeCtx());
    REQUIRE(result != nullptr);

    auto medians = rowMedians(*result);
    for (float m: medians) {
        REQUIRE_THAT(m, Catch::Matchers::WithinAbs(0.0F, 1e-4F));
    }
}

// ============================================================================
// Excluded channels: reference computed from non-excluded channels only
// ============================================================================

TEST_CASE("TensorCAR excluded channels not used for reference", "[TransformsV2][TensorCAR]") {
    // 4 channels: [0, 1, 2, outlier=1000]; exclude channel 3
    // Reference from channels 0-2 with offsets [0,1,2] → mean = 1 (at t=0)
    // Channel 3 (outlier) is excluded from reference computation.
    constexpr std::size_t num_rows = 30;
    constexpr std::size_t num_cols = 4;
    std::vector<float> flat(num_rows * num_cols);
    for (std::size_t r = 0; r < num_rows; ++r) {
        flat[r * num_cols + 0] = 0.0F + static_cast<float>(r);
        flat[r * num_cols + 1] = 1.0F + static_cast<float>(r);
        flat[r * num_cols + 2] = 2.0F + static_cast<float>(r);
        flat[r * num_cols + 3] = 1000.0F + static_cast<float>(r);// outlier
    }
    auto ts = makeDenseTimeStorage(num_rows);
    auto tf = makeTimeFrame(static_cast<int>(num_rows + 10));
    TensorData const input = TensorData::createTimeSeries2D(flat, num_rows, num_cols, ts, tf, {});

    TensorCARParams params;
    params.method = CARMethod::Mean;
    params.exclude_channels = {3};// exclude the outlier

    auto result = tensorCAR(input, params, makeCtx());
    REQUIRE(result != nullptr);

    auto result_flat = result->materializeFlat();

    // Reference from channels 0,1,2: mean = (0 + 1 + 2) / 3 = 1.0 at r=0
    // Channel 0 after CAR: 0 - 1 = -1.0
    // Channel 1 after CAR: 1 - 1 = 0.0
    // Channel 2 after CAR: 2 - 1 = 1.0
    // Channel 3 (outlier) after CAR: 1000 - 1 = 999.0  (reference subtracted but not used in ref)
    REQUIRE_THAT(result_flat[0 * num_cols + 0], Catch::Matchers::WithinAbs(-1.0F, 1e-4F));
    REQUIRE_THAT(result_flat[0 * num_cols + 1], Catch::Matchers::WithinAbs(0.0F, 1e-4F));
    REQUIRE_THAT(result_flat[0 * num_cols + 2], Catch::Matchers::WithinAbs(1.0F, 1e-4F));
    REQUIRE_THAT(result_flat[0 * num_cols + 3], Catch::Matchers::WithinAbs(999.0F, 1e-4F));

    // Row means of included channels after CAR ≈ 0
    for (std::size_t r = 0; r < num_rows; ++r) {
        float const included_sum = result_flat[r * num_cols + 0] + result_flat[r * num_cols + 1] + result_flat[r * num_cols + 2];
        float included_mean = included_sum / 3.0F;
        REQUIRE_THAT(included_mean, Catch::Matchers::WithinAbs(0.0F, 1e-4F));
    }
}

// ============================================================================
// Shape and column-name preservation
// ============================================================================

TEST_CASE("TensorCAR preserves shape and column names", "[TransformsV2][TensorCAR]") {
    constexpr std::size_t num_rows = 25;
    std::vector<std::string> col_names = {"ch0", "ch1", "ch2"};
    std::vector<float> const flat(num_rows * 3, 0.0F);
    auto ts = makeDenseTimeStorage(num_rows);
    auto tf = makeTimeFrame(static_cast<int>(num_rows));
    TensorData const input = TensorData::createTimeSeries2D(
            flat, num_rows, 3, ts, tf, col_names);

    TensorCARParams params;
    params.method = CARMethod::Mean;

    auto result = tensorCAR(input, params, makeCtx());
    REQUIRE(result != nullptr);

    REQUIRE(result->numRows() == num_rows);
    REQUIRE(result->numColumns() == 3);
    REQUIRE(result->hasNamedColumns());
    REQUIRE(result->columnNames() == col_names);
}

// ============================================================================
// RowDescriptor preservation
// ============================================================================

TEST_CASE("TensorCAR preserves TimeFrameIndex rows", "[TransformsV2][TensorCAR]") {
    TensorData const input = makeTimeSeriesTensor({0.0F, 1.0F, 2.0F}, 20);
    TensorCARParams const params;

    auto result = tensorCAR(input, params, makeCtx());
    REQUIRE(result != nullptr);
    REQUIRE(result->rowType() == RowType::TimeFrameIndex);
    REQUIRE(result->getTimeFrame() != nullptr);
    REQUIRE(result->getTimeFrame().get() == input.getTimeFrame().get());
}

TEST_CASE("TensorCAR preserves Ordinal rows", "[TransformsV2][TensorCAR]") {
    TensorData const input = makeOrdinalTensor({0.0F, 5.0F, 10.0F}, 15);
    TensorCARParams const params;

    auto result = tensorCAR(input, params, makeCtx());
    REQUIRE(result != nullptr);
    REQUIRE(result->rowType() == RowType::Ordinal);
    REQUIRE(result->numRows() == 15);
}

TEST_CASE("TensorCAR preserves Interval rows", "[TransformsV2][TensorCAR]") {
    TensorData const input = makeIntervalTensor({0.0F, 2.0F, 4.0F}, 10);
    TensorCARParams const params;

    auto result = tensorCAR(input, params, makeCtx());
    REQUIRE(result != nullptr);
    REQUIRE(result->rowType() == RowType::Interval);
    REQUIRE(result->numRows() == 10);
    REQUIRE(result->getTimeFrame() != nullptr);
}

// ============================================================================
// Via ElementRegistry
// ============================================================================

TEST_CASE("TensorCAR executes through ElementRegistry", "[TransformsV2][TensorCAR]") {
    auto & registry = ElementRegistry::instance();
    TensorData const input = makeTimeSeriesTensor({0.0F, 1.0F, 2.0F, 3.0F}, 30);
    ComputeContext const ctx;

    TensorCARParams params;
    params.method = CARMethod::Mean;

    auto result = registry.executeContainerTransform<TensorData, TensorData, TensorCARParams>(
            "TensorCAR", input, params, ctx);

    REQUIRE(result != nullptr);
    REQUIRE(result->numRows() == 30);
    REQUIRE(result->numColumns() == 4);

    auto means = rowMeans(*result);
    for (float m: means) {
        REQUIRE_THAT(m, Catch::Matchers::WithinAbs(0.0F, 1e-4F));
    }
}

// ============================================================================
// Validation failures
// ============================================================================

TEST_CASE("TensorCAR returns nullptr for 1D input", "[TransformsV2][TensorCAR]") {
    // Create a 1D tensor via createOrdinal2D with 1 column, then we can't make 1D easily
    // Instead use the direct function with ndim != 2 — use a 1-column ordinal as proxy
    // (ndim is still 2 for a 1-col matrix; we need a genuine 1D tensor)
    // Build a 1D tensor via createND
    std::vector<float> const data = {1.0F, 2.0F, 3.0F};
    TensorData const input = TensorData::createND(data, {{"time", 3}});

    TensorCARParams const params;
    auto result = tensorCAR(input, params, makeCtx());
    REQUIRE(result == nullptr);
}

TEST_CASE("TensorCAR returns nullptr when all channels excluded", "[TransformsV2][TensorCAR]") {
    TensorData const input = makeOrdinalTensor({0.0F, 1.0F, 2.0F}, 10);

    TensorCARParams params;
    params.exclude_channels = {0, 1, 2};// exclude all

    auto result = tensorCAR(input, params, makeCtx());
    REQUIRE(result == nullptr);
}

// ============================================================================
// Edge case: single channel
// ============================================================================

TEST_CASE("TensorCAR single channel produces all-zero output", "[TransformsV2][TensorCAR]") {
    // With 1 channel, reference = the channel itself → output = 0
    TensorData const input = makeOrdinalTensor({5.0F}, 20);// single channel, offset=5

    TensorCARParams params;
    params.method = CARMethod::Mean;

    auto result = tensorCAR(input, params, makeCtx());
    REQUIRE(result != nullptr);
    REQUIRE(result->numColumns() == 1);

    auto flat = result->materializeFlat();
    for (float v: flat) {
        REQUIRE_THAT(v, Catch::Matchers::WithinAbs(0.0F, 1e-5F));
    }
}
