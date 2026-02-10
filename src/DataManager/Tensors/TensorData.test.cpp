/**
 * @file TensorData.test.cpp
 * @brief Unit tests for the refactored TensorDataV2 public API
 *
 * Tests cover:
 * - Default construction (empty tensor)
 * - Factory method: createTimeSeries2D
 * - Factory method: createFromIntervals
 * - Factory method: createND (2D, 3D, 4D)
 * - Factory method: createFromArmadillo (matrix and cube)
 * - Factory method: createOrdinal2D
 * - Dimension queries (ndim, shape, dimensions)
 * - Row queries (rowType, numRows)
 * - Column access (by index, by name)
 * - Element access (at, row, flatData, materializeFlat)
 * - Named columns
 * - Backend conversion (toArmadillo, asArmadilloMatrix, asArmadilloCube)
 * - Materialization
 * - Mutation (setData)
 * - Storage access, isEmpty, isContiguous
 * - TimeFrame management
 * - Copy and move semantics
 * - Observer notification
 * - DataTraits
 * - Error handling / edge cases
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Tensors/TensorData.hpp"

#include "Tensors/storage/ArmadilloTensorStorage.hpp"
#include "Tensors/storage/DenseTensorStorage.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"
#include "DataManager/TypeTraits/DataTypeTraits.hpp"

#include <armadillo>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;

// =============================================================================
// Helpers
// =============================================================================

namespace {

/// Create a simple dense TimeIndexStorage from 0..count-1
std::shared_ptr<TimeIndexStorage> makeDenseTimeStorage(std::size_t count) {
    return TimeIndexStorageFactory::createDenseFromZero(count);
}

/// Create a simple TimeFrame
std::shared_ptr<TimeFrame> makeTimeFrame(std::size_t size) {
    std::vector<int> timestamps(size);
    for (std::size_t i = 0; i < size; ++i) {
        timestamps[i] = static_cast<int>(i);
    }
    return std::make_shared<TimeFrame>(timestamps);
}

} // anonymous namespace

// =============================================================================
// Default Construction
// =============================================================================

TEST_CASE("TensorDataV2 default construction is empty", "[TensorDataV2]") {
    TensorDataV2 tensor;

    CHECK(tensor.isEmpty());
    CHECK(tensor.ndim() == 0);
    CHECK(tensor.shape().empty());
    CHECK(tensor.numRows() == 0);
    CHECK(tensor.rowType() == RowType::Ordinal);
    CHECK_FALSE(tensor.hasNamedColumns());
    CHECK_FALSE(tensor.isContiguous());
    CHECK(tensor.getTimeFrame() == nullptr);
}

// =============================================================================
// createTimeSeries2D
// =============================================================================

TEST_CASE("TensorDataV2 createTimeSeries2D basic", "[TensorDataV2]") {
    // 3 rows × 4 columns
    std::vector<float> data = {
        1.0f, 2.0f, 3.0f, 4.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
        9.0f, 10.0f, 11.0f, 12.0f
    };
    auto ts = makeDenseTimeStorage(3);
    auto tf = makeTimeFrame(100);

    auto tensor = TensorDataV2::createTimeSeries2D(
        data, 3, 4, ts, tf, {"a", "b", "c", "d"});

    CHECK_FALSE(tensor.isEmpty());
    CHECK(tensor.ndim() == 2);
    CHECK(tensor.shape() == std::vector<std::size_t>{3, 4});
    CHECK(tensor.numRows() == 3);
    CHECK(tensor.rowType() == RowType::TimeFrameIndex);
    CHECK(tensor.hasNamedColumns());
    CHECK(tensor.columnNames() == std::vector<std::string>{"a", "b", "c", "d"});
    CHECK(tensor.numColumns() == 4);
    CHECK(tensor.isContiguous());
    CHECK(tensor.getTimeFrame() == tf);
}

TEST_CASE("TensorDataV2 createTimeSeries2D column access by index", "[TensorDataV2]") {
    std::vector<float> data = {
        1.0f, 2.0f,
        3.0f, 4.0f,
        5.0f, 6.0f
    };
    auto ts = makeDenseTimeStorage(3);
    auto tf = makeTimeFrame(100);

    auto tensor = TensorDataV2::createTimeSeries2D(data, 3, 2, ts, tf);

    auto col0 = tensor.getColumn(0);
    REQUIRE(col0.size() == 3);
    CHECK_THAT(col0[0], WithinAbs(1.0f, 1e-5));
    CHECK_THAT(col0[1], WithinAbs(3.0f, 1e-5));
    CHECK_THAT(col0[2], WithinAbs(5.0f, 1e-5));

    auto col1 = tensor.getColumn(1);
    REQUIRE(col1.size() == 3);
    CHECK_THAT(col1[0], WithinAbs(2.0f, 1e-5));
    CHECK_THAT(col1[1], WithinAbs(4.0f, 1e-5));
    CHECK_THAT(col1[2], WithinAbs(6.0f, 1e-5));
}

TEST_CASE("TensorDataV2 createTimeSeries2D column access by name", "[TensorDataV2]") {
    std::vector<float> data = {
        10.0f, 20.0f,
        30.0f, 40.0f
    };
    auto ts = makeDenseTimeStorage(2);
    auto tf = makeTimeFrame(100);

    auto tensor = TensorDataV2::createTimeSeries2D(
        data, 2, 2, ts, tf, {"alpha", "beta"});

    auto alpha = tensor.getColumn("alpha");
    REQUIRE(alpha.size() == 2);
    CHECK_THAT(alpha[0], WithinAbs(10.0f, 1e-5));
    CHECK_THAT(alpha[1], WithinAbs(30.0f, 1e-5));

    auto beta = tensor.getColumn("beta");
    REQUIRE(beta.size() == 2);
    CHECK_THAT(beta[0], WithinAbs(20.0f, 1e-5));
    CHECK_THAT(beta[1], WithinAbs(40.0f, 1e-5));
}

TEST_CASE("TensorDataV2 createTimeSeries2D element access", "[TensorDataV2]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    auto ts = makeDenseTimeStorage(2);
    auto tf = makeTimeFrame(100);

    auto tensor = TensorDataV2::createTimeSeries2D(data, 2, 3, ts, tf);

    // at()
    std::vector<std::size_t> idx = {0, 0};
    CHECK_THAT(tensor.at(idx), WithinAbs(1.0f, 1e-5));
    idx = {0, 2};
    CHECK_THAT(tensor.at(idx), WithinAbs(3.0f, 1e-5));
    idx = {1, 0};
    CHECK_THAT(tensor.at(idx), WithinAbs(4.0f, 1e-5));
    idx = {1, 2};
    CHECK_THAT(tensor.at(idx), WithinAbs(6.0f, 1e-5));
}

TEST_CASE("TensorDataV2 createTimeSeries2D row access", "[TensorDataV2]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    auto ts = makeDenseTimeStorage(2);
    auto tf = makeTimeFrame(100);

    auto tensor = TensorDataV2::createTimeSeries2D(data, 2, 3, ts, tf);

    auto row0 = tensor.row(0);
    REQUIRE(row0.size() == 3);
    CHECK_THAT(row0[0], WithinAbs(1.0f, 1e-5));
    CHECK_THAT(row0[1], WithinAbs(2.0f, 1e-5));
    CHECK_THAT(row0[2], WithinAbs(3.0f, 1e-5));

    auto row1 = tensor.row(1);
    REQUIRE(row1.size() == 3);
    CHECK_THAT(row1[0], WithinAbs(4.0f, 1e-5));
    CHECK_THAT(row1[1], WithinAbs(5.0f, 1e-5));
    CHECK_THAT(row1[2], WithinAbs(6.0f, 1e-5));
}

TEST_CASE("TensorDataV2 createTimeSeries2D error: null time_storage", "[TensorDataV2]") {
    std::vector<float> data = {1.0f, 2.0f};
    auto tf = makeTimeFrame(100);
    CHECK_THROWS_AS(
        TensorDataV2::createTimeSeries2D(data, 1, 2, nullptr, tf),
        std::invalid_argument);
}

TEST_CASE("TensorDataV2 createTimeSeries2D error: null time_frame", "[TensorDataV2]") {
    std::vector<float> data = {1.0f, 2.0f};
    auto ts = makeDenseTimeStorage(1);
    CHECK_THROWS_AS(
        TensorDataV2::createTimeSeries2D(data, 1, 2, ts, nullptr),
        std::invalid_argument);
}

TEST_CASE("TensorDataV2 createTimeSeries2D error: time_storage size mismatch", "[TensorDataV2]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    auto ts = makeDenseTimeStorage(3); // 3 != 2
    auto tf = makeTimeFrame(100);
    CHECK_THROWS_AS(
        TensorDataV2::createTimeSeries2D(data, 2, 2, ts, tf),
        std::invalid_argument);
}

// =============================================================================
// createFromIntervals
// =============================================================================

TEST_CASE("TensorDataV2 createFromIntervals basic", "[TensorDataV2]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    auto tf = makeTimeFrame(100);
    std::vector<TimeFrameInterval> intervals = {
        {TimeFrameIndex{0}, TimeFrameIndex{10}},
        {TimeFrameIndex{20}, TimeFrameIndex{30}}
    };

    auto tensor = TensorDataV2::createFromIntervals(
        data, 2, 2, intervals, tf, {"metric_a", "metric_b"});

    CHECK(tensor.ndim() == 2);
    CHECK(tensor.numRows() == 2);
    CHECK(tensor.rowType() == RowType::Interval);
    CHECK(tensor.hasNamedColumns());
    CHECK(tensor.getTimeFrame() == tf);
}

TEST_CASE("TensorDataV2 createFromIntervals error: intervals size mismatch", "[TensorDataV2]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    auto tf = makeTimeFrame(100);
    std::vector<TimeFrameInterval> intervals = {
        {TimeFrameIndex{0}, TimeFrameIndex{10}}
    }; // 1 interval but num_rows=2
    CHECK_THROWS_AS(
        TensorDataV2::createFromIntervals(data, 2, 2, intervals, tf),
        std::invalid_argument);
}

// =============================================================================
// createND
// =============================================================================

TEST_CASE("TensorDataV2 createND 2D", "[TensorDataV2]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    auto tensor = TensorDataV2::createND(data, {{"rows", 2}, {"cols", 3}});

    CHECK(tensor.ndim() == 2);
    CHECK(tensor.shape() == std::vector<std::size_t>{2, 3});
    CHECK(tensor.numRows() == 2);
    CHECK(tensor.rowType() == RowType::Ordinal);
    // Should be Armadillo-backed since ≤3D
    CHECK(tensor.storage().getStorageType() == TensorStorageType::Armadillo);
}

TEST_CASE("TensorDataV2 createND 3D", "[TensorDataV2]") {
    // 2 × 3 × 2 = 12 elements
    std::vector<float> data(12);
    for (std::size_t i = 0; i < 12; ++i) {
        data[i] = static_cast<float>(i + 1);
    }

    auto tensor = TensorDataV2::createND(
        data, {{"batch", 2}, {"height", 3}, {"width", 2}});

    CHECK(tensor.ndim() == 3);
    CHECK(tensor.shape() == std::vector<std::size_t>{2, 3, 2});
    CHECK(tensor.storage().getStorageType() == TensorStorageType::Armadillo);
}

TEST_CASE("TensorDataV2 createND 4D (Dense fallback)", "[TensorDataV2]") {
    // 2 × 2 × 2 × 3 = 24 elements
    std::vector<float> data(24);
    for (std::size_t i = 0; i < 24; ++i) {
        data[i] = static_cast<float>(i);
    }

    auto tensor = TensorDataV2::createND(
        data, {{"batch", 2}, {"channel", 2}, {"height", 2}, {"width", 3}});

    CHECK(tensor.ndim() == 4);
    CHECK(tensor.shape() == std::vector<std::size_t>{2, 2, 2, 3});
    CHECK(tensor.storage().getStorageType() == TensorStorageType::Dense);

    // Verify element access
    std::vector<std::size_t> idx = {0, 0, 0, 0};
    CHECK_THAT(tensor.at(idx), WithinAbs(0.0f, 1e-5));
    idx = {1, 1, 1, 2};
    CHECK_THAT(tensor.at(idx), WithinAbs(23.0f, 1e-5));
}

TEST_CASE("TensorDataV2 createND error: empty axes", "[TensorDataV2]") {
    CHECK_THROWS_AS(
        TensorDataV2::createND({}, {}),
        std::invalid_argument);
}

// =============================================================================
// createFromArmadillo
// =============================================================================

TEST_CASE("TensorDataV2 createFromArmadillo matrix", "[TensorDataV2]") {
    arma::fmat m = {{1.0f, 2.0f, 3.0f},
                     {4.0f, 5.0f, 6.0f}};

    auto tensor = TensorDataV2::createFromArmadillo(m, {"x", "y", "z"});

    CHECK(tensor.ndim() == 2);
    CHECK(tensor.shape() == std::vector<std::size_t>{2, 3});
    CHECK(tensor.hasNamedColumns());
    CHECK(tensor.columnNames() == std::vector<std::string>{"x", "y", "z"});
    CHECK(tensor.storage().getStorageType() == TensorStorageType::Armadillo);

    // Zero-copy access
    auto const & mat_ref = tensor.asArmadilloMatrix();
    CHECK(mat_ref.n_rows == 2);
    CHECK(mat_ref.n_cols == 3);
    CHECK_THAT(mat_ref(0, 0), WithinAbs(1.0f, 1e-5));
    CHECK_THAT(mat_ref(1, 2), WithinAbs(6.0f, 1e-5));
}

TEST_CASE("TensorDataV2 createFromArmadillo cube", "[TensorDataV2]") {
    arma::fcube c(2, 3, 4); // 2 rows, 3 cols, 4 slices
    c.fill(1.0f);

    auto tensor = TensorDataV2::createFromArmadillo(std::move(c));

    CHECK(tensor.ndim() == 3);
    // ArmadilloTensorStorage maps cube(n_rows, n_cols, n_slices) to shape [n_slices, n_rows, n_cols]
    CHECK(tensor.shape() == std::vector<std::size_t>{4, 2, 3});
    CHECK(tensor.storage().getStorageType() == TensorStorageType::Armadillo);
}

TEST_CASE("TensorDataV2 createFromArmadillo cube with custom axes", "[TensorDataV2]") {
    arma::fcube c(2, 3, 4);
    c.fill(2.0f);

    auto tensor = TensorDataV2::createFromArmadillo(
        std::move(c), {{"time", 4}, {"row", 2}, {"col", 3}});

    CHECK(tensor.ndim() == 3);
    auto const & dims = tensor.dimensions();
    auto time_ax = dims.findAxis("time");
    REQUIRE(time_ax.has_value());
    CHECK(dims.axis(time_ax.value()).size == 4);
}

// =============================================================================
// createOrdinal2D
// =============================================================================

TEST_CASE("TensorDataV2 createOrdinal2D", "[TensorDataV2]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    auto tensor = TensorDataV2::createOrdinal2D(data, 2, 2, {"col_a", "col_b"});

    CHECK(tensor.ndim() == 2);
    CHECK(tensor.numRows() == 2);
    CHECK(tensor.rowType() == RowType::Ordinal);
    CHECK(tensor.hasNamedColumns());
    CHECK(tensor.getTimeFrame() == nullptr);
}

// =============================================================================
// materializeFlat
// =============================================================================

TEST_CASE("TensorDataV2 materializeFlat on 2D returns row-major data", "[TensorDataV2]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    auto tensor = TensorDataV2::createOrdinal2D(data, 2, 3);

    auto flat = tensor.materializeFlat();
    REQUIRE(flat.size() == 6);
    // Row-major: row 0 = {1, 2, 3}, row 1 = {4, 5, 6}
    CHECK_THAT(flat[0], WithinAbs(1.0f, 1e-5));
    CHECK_THAT(flat[1], WithinAbs(2.0f, 1e-5));
    CHECK_THAT(flat[2], WithinAbs(3.0f, 1e-5));
    CHECK_THAT(flat[3], WithinAbs(4.0f, 1e-5));
    CHECK_THAT(flat[4], WithinAbs(5.0f, 1e-5));
    CHECK_THAT(flat[5], WithinAbs(6.0f, 1e-5));
}

TEST_CASE("TensorDataV2 materializeFlat on empty tensor returns empty", "[TensorDataV2]") {
    TensorDataV2 tensor;
    CHECK(tensor.materializeFlat().empty());
}

// =============================================================================
// Backend Conversion
// =============================================================================

TEST_CASE("TensorDataV2 toArmadillo on already-armadillo is identity", "[TensorDataV2]") {
    arma::fmat m = {{1.0f, 2.0f}, {3.0f, 4.0f}};
    auto tensor = TensorDataV2::createFromArmadillo(m);
    auto converted = tensor.toArmadillo();

    CHECK(converted.storage().getStorageType() == TensorStorageType::Armadillo);
    CHECK(converted.ndim() == 2);
}

TEST_CASE("TensorDataV2 toArmadillo on 4D throws", "[TensorDataV2]") {
    std::vector<float> data(24, 1.0f);
    auto tensor = TensorDataV2::createND(
        data, {{"a", 2}, {"b", 3}, {"c", 2}, {"d", 2}});

    CHECK_THROWS_AS(tensor.toArmadillo(), std::logic_error);
}

TEST_CASE("TensorDataV2 asArmadilloMatrix on empty throws", "[TensorDataV2]") {
    TensorDataV2 tensor;
    CHECK_THROWS_AS(tensor.asArmadilloMatrix(), std::logic_error);
}

TEST_CASE("TensorDataV2 asArmadilloCube on 2D Armadillo throws", "[TensorDataV2]") {
    arma::fmat m = {{1.0f, 2.0f}};
    auto tensor = TensorDataV2::createFromArmadillo(m);
    CHECK_THROWS(tensor.asArmadilloCube()); // ArmadilloTensorStorage::cube() throws
}

// =============================================================================
// materialize
// =============================================================================

TEST_CASE("TensorDataV2 materialize creates independent copy", "[TensorDataV2]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    auto tensor = TensorDataV2::createOrdinal2D(data, 2, 2);
    auto materialized = tensor.materialize();

    CHECK(materialized.ndim() == 2);
    CHECK(materialized.shape() == std::vector<std::size_t>{2, 2});
    CHECK_FALSE(materialized.isEmpty());
}

// =============================================================================
// setData
// =============================================================================

TEST_CASE("TensorDataV2 setData replaces storage", "[TensorDataV2]") {
    auto tensor = TensorDataV2::createOrdinal2D({1.0f, 2.0f, 3.0f, 4.0f}, 2, 2);

    CHECK(tensor.ndim() == 2);

    // Replace with 1D data
    tensor.setData({10.0f, 20.0f, 30.0f}, {3});
    CHECK(tensor.ndim() == 1);
    CHECK(tensor.shape() == std::vector<std::size_t>{3});
    CHECK(tensor.numRows() == 3);
    CHECK(tensor.rowType() == RowType::Ordinal);
}

TEST_CASE("TensorDataV2 setData notifies observers", "[TensorDataV2]") {
    auto tensor = TensorDataV2::createOrdinal2D({1.0f, 2.0f}, 1, 2);

    bool notified = false;
    tensor.addObserver([&notified]() { notified = true; });

    tensor.setData({10.0f, 20.0f, 30.0f}, {3});
    CHECK(notified);
}

TEST_CASE("TensorDataV2 setData error: empty shape", "[TensorDataV2]") {
    TensorDataV2 tensor;
    CHECK_THROWS_AS(tensor.setData({1.0f}, {}), std::invalid_argument);
}

// =============================================================================
// TimeFrame Management
// =============================================================================

TEST_CASE("TensorDataV2 setTimeFrame and getTimeFrame", "[TensorDataV2]") {
    TensorDataV2 tensor;
    CHECK(tensor.getTimeFrame() == nullptr);

    auto tf = makeTimeFrame(50);
    tensor.setTimeFrame(tf);
    CHECK(tensor.getTimeFrame() == tf);

    tensor.setTimeFrame(nullptr);
    CHECK(tensor.getTimeFrame() == nullptr);
}

// =============================================================================
// Column Names
// =============================================================================

TEST_CASE("TensorDataV2 setColumnNames after construction", "[TensorDataV2]") {
    auto tensor = TensorDataV2::createOrdinal2D({1.0f, 2.0f, 3.0f, 4.0f}, 2, 2);
    CHECK_FALSE(tensor.hasNamedColumns());

    tensor.setColumnNames({"x", "y"});
    CHECK(tensor.hasNamedColumns());
    CHECK(tensor.columnNames() == std::vector<std::string>{"x", "y"});

    auto col = tensor.getColumn("x");
    REQUIRE(col.size() == 2);
}

// =============================================================================
// Copy and Move Semantics
// =============================================================================

TEST_CASE("TensorDataV2 copy semantics", "[TensorDataV2]") {
    auto original = TensorDataV2::createOrdinal2D(
        {1.0f, 2.0f, 3.0f, 4.0f}, 2, 2, {"a", "b"});

    // Copy construction
    auto copy = original; // NOLINT(performance-unnecessary-copy-initialization)
    CHECK(copy.ndim() == 2);
    CHECK(copy.shape() == std::vector<std::size_t>{2, 2});
    CHECK(copy.hasNamedColumns());

    // Verify data integrity
    std::vector<std::size_t> idx = {0, 0};
    CHECK_THAT(copy.at(idx), WithinAbs(1.0f, 1e-5));
}

TEST_CASE("TensorDataV2 move semantics", "[TensorDataV2]") {
    auto original = TensorDataV2::createOrdinal2D(
        {1.0f, 2.0f, 3.0f, 4.0f}, 2, 2);

    auto moved = std::move(original);
    CHECK(moved.ndim() == 2);
    CHECK_FALSE(moved.isEmpty());
}

// =============================================================================
// Observer Integration
// =============================================================================

TEST_CASE("TensorDataV2 observer registration and notification", "[TensorDataV2]") {
    auto tensor = TensorDataV2::createOrdinal2D({1.0f, 2.0f}, 1, 2);

    int notify_count = 0;
    auto id = tensor.addObserver([&notify_count]() { ++notify_count; });

    // setData should fire observer
    tensor.setData({10.0f, 20.0f, 30.0f}, {3});
    CHECK(notify_count == 1);

    tensor.setData({1.0f}, {1});
    CHECK(notify_count == 2);

    // Remove observer
    tensor.removeObserver(id);
    tensor.setData({1.0f, 2.0f}, {2});
    CHECK(notify_count == 2); // no more notifications
}

// =============================================================================
// DataTraits
// =============================================================================

TEST_CASE("TensorDataV2 DataTraits are correct", "[TensorDataV2]") {
    using Traits = TensorDataV2::DataTraits;

    STATIC_CHECK_FALSE(Traits::is_ragged);
    STATIC_CHECK(Traits::is_temporal);
    STATIC_CHECK_FALSE(Traits::has_entity_ids);
    STATIC_CHECK_FALSE(Traits::is_spatial);

    STATIC_CHECK(std::is_same_v<Traits::container_type, TensorDataV2>);
    STATIC_CHECK(std::is_same_v<Traits::element_type, float>);
}

TEST_CASE("TensorDataV2 satisfies HasDataTraits concept", "[TensorDataV2]") {
    STATIC_CHECK(WhiskerToolbox::TypeTraits::HasDataTraits<TensorDataV2>);
    STATIC_CHECK(WhiskerToolbox::TypeTraits::TemporalContainer<TensorDataV2>);
    STATIC_CHECK_FALSE(WhiskerToolbox::TypeTraits::RaggedContainer<TensorDataV2>);
    STATIC_CHECK_FALSE(WhiskerToolbox::TypeTraits::EntityTrackedContainer<TensorDataV2>);
    STATIC_CHECK_FALSE(WhiskerToolbox::TypeTraits::SpatialContainer<TensorDataV2>);
}

// =============================================================================
// Storage Access
// =============================================================================

TEST_CASE("TensorDataV2 storage() provides backend access", "[TensorDataV2]") {
    auto tensor = TensorDataV2::createOrdinal2D({1.0f, 2.0f, 3.0f, 4.0f}, 2, 2);

    auto const & wrapper = tensor.storage();
    CHECK(wrapper.isValid());
    CHECK(wrapper.totalElements() == 4);

    // Type recovery for Armadillo backend
    auto const * arma = wrapper.tryGetAs<ArmadilloTensorStorage>();
    CHECK(arma != nullptr);
}

TEST_CASE("TensorDataV2 storage type for 4D is Dense", "[TensorDataV2]") {
    std::vector<float> data(24, 1.0f);
    auto tensor = TensorDataV2::createND(
        data, {{"a", 2}, {"b", 3}, {"c", 2}, {"d", 2}});

    CHECK(tensor.storage().getStorageType() == TensorStorageType::Dense);

    auto const * dense = tensor.storage().tryGetAs<DenseTensorStorage>();
    CHECK(dense != nullptr);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("TensorDataV2 1D tensor via createND", "[TensorDataV2]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f};
    auto tensor = TensorDataV2::createND(data, {{"values", 3}});

    CHECK(tensor.ndim() == 1);
    CHECK(tensor.shape() == std::vector<std::size_t>{3});
    CHECK(tensor.numRows() == 3); // ordinal rows = axis(0).size
    CHECK(tensor.numColumns() == 1); // 1D gets numColumns() == 1
}

TEST_CASE("TensorDataV2 getColumn on empty throws", "[TensorDataV2]") {
    TensorDataV2 tensor;
    CHECK_THROWS_AS(tensor.getColumn(0), std::runtime_error);
}

TEST_CASE("TensorDataV2 getColumn out of range throws", "[TensorDataV2]") {
    auto tensor = TensorDataV2::createOrdinal2D({1.0f, 2.0f}, 1, 2);
    CHECK_THROWS_AS(tensor.getColumn(5), std::out_of_range);
}

TEST_CASE("TensorDataV2 getColumn by nonexistent name throws", "[TensorDataV2]") {
    auto tensor = TensorDataV2::createOrdinal2D(
        {1.0f, 2.0f}, 1, 2, {"a", "b"});
    CHECK_THROWS_AS(tensor.getColumn("nonexistent"), std::invalid_argument);
}

TEST_CASE("TensorDataV2 at on empty throws", "[TensorDataV2]") {
    TensorDataV2 tensor;
    std::vector<std::size_t> idx = {0};
    CHECK_THROWS_AS(tensor.at(idx), std::runtime_error);
}

TEST_CASE("TensorDataV2 row on empty throws", "[TensorDataV2]") {
    TensorDataV2 tensor;
    CHECK_THROWS_AS(tensor.row(0), std::runtime_error);
}

TEST_CASE("TensorDataV2 row out of range throws", "[TensorDataV2]") {
    auto tensor = TensorDataV2::createOrdinal2D({1.0f, 2.0f}, 1, 2);
    CHECK_THROWS_AS(tensor.row(5), std::out_of_range);
}

TEST_CASE("TensorDataV2 flatData on empty throws", "[TensorDataV2]") {
    TensorDataV2 tensor;
    CHECK_THROWS_AS(tensor.flatData(), std::runtime_error);
}

// =============================================================================
// RowDescriptor access through TensorDataV2
// =============================================================================

TEST_CASE("TensorDataV2 rows() returns correct descriptor for time-indexed", "[TensorDataV2]") {
    auto ts = makeDenseTimeStorage(3);
    auto tf = makeTimeFrame(100);
    auto tensor = TensorDataV2::createTimeSeries2D(
        {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}, 3, 2, ts, tf);

    auto const & rows = tensor.rows();
    CHECK(rows.type() == RowType::TimeFrameIndex);
    CHECK(rows.count() == 3);
    CHECK(rows.timeFrame() != nullptr);
}

TEST_CASE("TensorDataV2 rows() returns correct descriptor for intervals", "[TensorDataV2]") {
    auto tf = makeTimeFrame(100);
    std::vector<TimeFrameInterval> intervals = {
        {TimeFrameIndex{0}, TimeFrameIndex{10}},
        {TimeFrameIndex{20}, TimeFrameIndex{30}}
    };

    auto tensor = TensorDataV2::createFromIntervals(
        {1.0f, 2.0f, 3.0f, 4.0f}, 2, 2, intervals, tf);

    auto const & rows = tensor.rows();
    CHECK(rows.type() == RowType::Interval);
    CHECK(rows.count() == 2);
    auto spans = rows.intervals();
    REQUIRE(spans.size() == 2);
}

// =============================================================================
// DimensionDescriptor access through TensorDataV2
// =============================================================================

TEST_CASE("TensorDataV2 dimensions() provides axis lookup", "[TensorDataV2]") {
    auto ts = makeDenseTimeStorage(3);
    auto tf = makeTimeFrame(100);
    auto tensor = TensorDataV2::createTimeSeries2D(
        {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}, 3, 2, ts, tf);

    auto const & dims = tensor.dimensions();
    CHECK(dims.ndim() == 2);
    CHECK(dims.is2D());

    auto time_idx = dims.findAxis("time");
    REQUIRE(time_idx.has_value());
    CHECK(time_idx.value() == 0);
    CHECK(dims.axis(0).size == 3);

    auto chan_idx = dims.findAxis("channel");
    REQUIRE(chan_idx.has_value());
    CHECK(chan_idx.value() == 1);
    CHECK(dims.axis(1).size == 2);
}
