/**
 * @file TensorData.test.cpp
 * @brief Unit tests for the refactored TensorData public API
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

TEST_CASE("TensorData default construction is empty", "[TensorData]") {
    TensorData tensor;

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

TEST_CASE("TensorData createTimeSeries2D basic", "[TensorData]") {
    // 3 rows × 4 columns
    std::vector<float> data = {
        1.0f, 2.0f, 3.0f, 4.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
        9.0f, 10.0f, 11.0f, 12.0f
    };
    auto ts = makeDenseTimeStorage(3);
    auto tf = makeTimeFrame(100);

    auto tensor = TensorData::createTimeSeries2D(
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

TEST_CASE("TensorData createTimeSeries2D column access by index", "[TensorData]") {
    std::vector<float> data = {
        1.0f, 2.0f,
        3.0f, 4.0f,
        5.0f, 6.0f
    };
    auto ts = makeDenseTimeStorage(3);
    auto tf = makeTimeFrame(100);

    auto tensor = TensorData::createTimeSeries2D(data, 3, 2, ts, tf);

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

TEST_CASE("TensorData createTimeSeries2D column access by name", "[TensorData]") {
    std::vector<float> data = {
        10.0f, 20.0f,
        30.0f, 40.0f
    };
    auto ts = makeDenseTimeStorage(2);
    auto tf = makeTimeFrame(100);

    auto tensor = TensorData::createTimeSeries2D(
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

TEST_CASE("TensorData createTimeSeries2D element access", "[TensorData]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    auto ts = makeDenseTimeStorage(2);
    auto tf = makeTimeFrame(100);

    auto tensor = TensorData::createTimeSeries2D(data, 2, 3, ts, tf);

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

TEST_CASE("TensorData createTimeSeries2D row access", "[TensorData]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    auto ts = makeDenseTimeStorage(2);
    auto tf = makeTimeFrame(100);

    auto tensor = TensorData::createTimeSeries2D(data, 2, 3, ts, tf);

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

TEST_CASE("TensorData createTimeSeries2D error: null time_storage", "[TensorData]") {
    std::vector<float> data = {1.0f, 2.0f};
    auto tf = makeTimeFrame(100);
    CHECK_THROWS_AS(
        TensorData::createTimeSeries2D(data, 1, 2, nullptr, tf),
        std::invalid_argument);
}

TEST_CASE("TensorData createTimeSeries2D error: null time_frame", "[TensorData]") {
    std::vector<float> data = {1.0f, 2.0f};
    auto ts = makeDenseTimeStorage(1);
    CHECK_THROWS_AS(
        TensorData::createTimeSeries2D(data, 1, 2, ts, nullptr),
        std::invalid_argument);
}

TEST_CASE("TensorData createTimeSeries2D error: time_storage size mismatch", "[TensorData]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    auto ts = makeDenseTimeStorage(3); // 3 != 2
    auto tf = makeTimeFrame(100);
    CHECK_THROWS_AS(
        TensorData::createTimeSeries2D(data, 2, 2, ts, tf),
        std::invalid_argument);
}

// =============================================================================
// createFromIntervals
// =============================================================================

TEST_CASE("TensorData createFromIntervals basic", "[TensorData]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    auto tf = makeTimeFrame(100);
    std::vector<TimeFrameInterval> intervals = {
        {TimeFrameIndex{0}, TimeFrameIndex{10}},
        {TimeFrameIndex{20}, TimeFrameIndex{30}}
    };

    auto tensor = TensorData::createFromIntervals(
        data, 2, 2, intervals, tf, {"metric_a", "metric_b"});

    CHECK(tensor.ndim() == 2);
    CHECK(tensor.numRows() == 2);
    CHECK(tensor.rowType() == RowType::Interval);
    CHECK(tensor.hasNamedColumns());
    CHECK(tensor.getTimeFrame() == tf);
}

TEST_CASE("TensorData createFromIntervals error: intervals size mismatch", "[TensorData]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    auto tf = makeTimeFrame(100);
    std::vector<TimeFrameInterval> intervals = {
        {TimeFrameIndex{0}, TimeFrameIndex{10}}
    }; // 1 interval but num_rows=2
    CHECK_THROWS_AS(
        TensorData::createFromIntervals(data, 2, 2, intervals, tf),
        std::invalid_argument);
}

// =============================================================================
// createND
// =============================================================================

TEST_CASE("TensorData createND 2D", "[TensorData]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    auto tensor = TensorData::createND(data, {{"rows", 2}, {"cols", 3}});

    CHECK(tensor.ndim() == 2);
    CHECK(tensor.shape() == std::vector<std::size_t>{2, 3});
    CHECK(tensor.numRows() == 2);
    CHECK(tensor.rowType() == RowType::Ordinal);
    // Should be Armadillo-backed since ≤3D
    CHECK(tensor.storage().getStorageType() == TensorStorageType::Armadillo);
}

TEST_CASE("TensorData createND 3D", "[TensorData]") {
    // 2 × 3 × 2 = 12 elements
    std::vector<float> data(12);
    for (std::size_t i = 0; i < 12; ++i) {
        data[i] = static_cast<float>(i + 1);
    }

    auto tensor = TensorData::createND(
        data, {{"batch", 2}, {"height", 3}, {"width", 2}});

    CHECK(tensor.ndim() == 3);
    CHECK(tensor.shape() == std::vector<std::size_t>{2, 3, 2});
    CHECK(tensor.storage().getStorageType() == TensorStorageType::Armadillo);
}

TEST_CASE("TensorData createND 4D (Dense fallback)", "[TensorData]") {
    // 2 × 2 × 2 × 3 = 24 elements
    std::vector<float> data(24);
    for (std::size_t i = 0; i < 24; ++i) {
        data[i] = static_cast<float>(i);
    }

    auto tensor = TensorData::createND(
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

TEST_CASE("TensorData createND error: empty axes", "[TensorData]") {
    CHECK_THROWS_AS(
        TensorData::createND({}, {}),
        std::invalid_argument);
}

// =============================================================================
// createFromArmadillo
// =============================================================================

TEST_CASE("TensorData createFromArmadillo matrix", "[TensorData]") {
    arma::fmat m = {{1.0f, 2.0f, 3.0f},
                     {4.0f, 5.0f, 6.0f}};

    auto tensor = TensorData::createFromArmadillo(m, {"x", "y", "z"});

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

TEST_CASE("TensorData createFromArmadillo cube", "[TensorData]") {
    arma::fcube c(2, 3, 4); // 2 rows, 3 cols, 4 slices
    c.fill(1.0f);

    auto tensor = TensorData::createFromArmadillo(std::move(c));

    CHECK(tensor.ndim() == 3);
    // ArmadilloTensorStorage maps cube(n_rows, n_cols, n_slices) to shape [n_slices, n_rows, n_cols]
    CHECK(tensor.shape() == std::vector<std::size_t>{4, 2, 3});
    CHECK(tensor.storage().getStorageType() == TensorStorageType::Armadillo);
}

TEST_CASE("TensorData createFromArmadillo cube with custom axes", "[TensorData]") {
    arma::fcube c(2, 3, 4);
    c.fill(2.0f);

    auto tensor = TensorData::createFromArmadillo(
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

TEST_CASE("TensorData createOrdinal2D", "[TensorData]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    auto tensor = TensorData::createOrdinal2D(data, 2, 2, {"col_a", "col_b"});

    CHECK(tensor.ndim() == 2);
    CHECK(tensor.numRows() == 2);
    CHECK(tensor.rowType() == RowType::Ordinal);
    CHECK(tensor.hasNamedColumns());
    CHECK(tensor.getTimeFrame() == nullptr);
}

// =============================================================================
// materializeFlat
// =============================================================================

TEST_CASE("TensorData materializeFlat on 2D returns row-major data", "[TensorData]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    auto tensor = TensorData::createOrdinal2D(data, 2, 3);

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

TEST_CASE("TensorData materializeFlat on empty tensor returns empty", "[TensorData]") {
    TensorData tensor;
    CHECK(tensor.materializeFlat().empty());
}

// =============================================================================
// Backend Conversion
// =============================================================================

TEST_CASE("TensorData toArmadillo on already-armadillo is identity", "[TensorData]") {
    arma::fmat m = {{1.0f, 2.0f}, {3.0f, 4.0f}};
    auto tensor = TensorData::createFromArmadillo(m);
    auto converted = tensor.toArmadillo();

    CHECK(converted.storage().getStorageType() == TensorStorageType::Armadillo);
    CHECK(converted.ndim() == 2);
}

TEST_CASE("TensorData toArmadillo on 4D throws", "[TensorData]") {
    std::vector<float> data(24, 1.0f);
    auto tensor = TensorData::createND(
        data, {{"a", 2}, {"b", 3}, {"c", 2}, {"d", 2}});

    CHECK_THROWS_AS(tensor.toArmadillo(), std::logic_error);
}

TEST_CASE("TensorData asArmadilloMatrix on empty throws", "[TensorData]") {
    TensorData tensor;
    CHECK_THROWS_AS(tensor.asArmadilloMatrix(), std::logic_error);
}

TEST_CASE("TensorData asArmadilloCube on 2D Armadillo throws", "[TensorData]") {
    arma::fmat m = {{1.0f, 2.0f}};
    auto tensor = TensorData::createFromArmadillo(m);
    CHECK_THROWS(tensor.asArmadilloCube()); // ArmadilloTensorStorage::cube() throws
}

// =============================================================================
// materialize
// =============================================================================

TEST_CASE("TensorData materialize creates independent copy", "[TensorData]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    auto tensor = TensorData::createOrdinal2D(data, 2, 2);
    auto materialized = tensor.materialize();

    CHECK(materialized.ndim() == 2);
    CHECK(materialized.shape() == std::vector<std::size_t>{2, 2});
    CHECK_FALSE(materialized.isEmpty());
}

// =============================================================================
// setData
// =============================================================================

TEST_CASE("TensorData setData replaces storage", "[TensorData]") {
    auto tensor = TensorData::createOrdinal2D({1.0f, 2.0f, 3.0f, 4.0f}, 2, 2);

    CHECK(tensor.ndim() == 2);

    // Replace with 1D data
    tensor.setData({10.0f, 20.0f, 30.0f}, {3});
    CHECK(tensor.ndim() == 1);
    CHECK(tensor.shape() == std::vector<std::size_t>{3});
    CHECK(tensor.numRows() == 3);
    CHECK(tensor.rowType() == RowType::Ordinal);
}

TEST_CASE("TensorData setData notifies observers", "[TensorData]") {
    auto tensor = TensorData::createOrdinal2D({1.0f, 2.0f}, 1, 2);

    bool notified = false;
    tensor.addObserver([&notified]() { notified = true; });

    tensor.setData({10.0f, 20.0f, 30.0f}, {3});
    CHECK(notified);
}

TEST_CASE("TensorData setData error: empty shape", "[TensorData]") {
    TensorData tensor;
    CHECK_THROWS_AS(tensor.setData({1.0f}, {}), std::invalid_argument);
}

// =============================================================================
// TimeFrame Management
// =============================================================================

TEST_CASE("TensorData setTimeFrame and getTimeFrame", "[TensorData]") {
    TensorData tensor;
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

TEST_CASE("TensorData setColumnNames after construction", "[TensorData]") {
    auto tensor = TensorData::createOrdinal2D({1.0f, 2.0f, 3.0f, 4.0f}, 2, 2);
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

TEST_CASE("TensorData copy semantics", "[TensorData]") {
    auto original = TensorData::createOrdinal2D(
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

TEST_CASE("TensorData move semantics", "[TensorData]") {
    auto original = TensorData::createOrdinal2D(
        {1.0f, 2.0f, 3.0f, 4.0f}, 2, 2);

    auto moved = std::move(original);
    CHECK(moved.ndim() == 2);
    CHECK_FALSE(moved.isEmpty());
}

// =============================================================================
// Observer Integration
// =============================================================================

TEST_CASE("TensorData observer registration and notification", "[TensorData]") {
    auto tensor = TensorData::createOrdinal2D({1.0f, 2.0f}, 1, 2);

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

TEST_CASE("TensorData DataTraits are correct", "[TensorData]") {
    using Traits = TensorData::DataTraits;

    STATIC_CHECK_FALSE(Traits::is_ragged);
    STATIC_CHECK(Traits::is_temporal);
    STATIC_CHECK_FALSE(Traits::has_entity_ids);
    STATIC_CHECK_FALSE(Traits::is_spatial);

    STATIC_CHECK(std::is_same_v<Traits::container_type, TensorData>);
    STATIC_CHECK(std::is_same_v<Traits::element_type, float>);
}

TEST_CASE("TensorData satisfies HasDataTraits concept", "[TensorData]") {
    STATIC_CHECK(WhiskerToolbox::TypeTraits::HasDataTraits<TensorData>);
    STATIC_CHECK(WhiskerToolbox::TypeTraits::TemporalContainer<TensorData>);
    STATIC_CHECK_FALSE(WhiskerToolbox::TypeTraits::RaggedContainer<TensorData>);
    STATIC_CHECK_FALSE(WhiskerToolbox::TypeTraits::EntityTrackedContainer<TensorData>);
    STATIC_CHECK_FALSE(WhiskerToolbox::TypeTraits::SpatialContainer<TensorData>);
}

// =============================================================================
// Storage Access
// =============================================================================

TEST_CASE("TensorData storage() provides backend access", "[TensorData]") {
    auto tensor = TensorData::createOrdinal2D({1.0f, 2.0f, 3.0f, 4.0f}, 2, 2);

    auto const & wrapper = tensor.storage();
    CHECK(wrapper.isValid());
    CHECK(wrapper.totalElements() == 4);

    // Type recovery for Armadillo backend
    auto const * arma = wrapper.tryGetAs<ArmadilloTensorStorage>();
    CHECK(arma != nullptr);
}

TEST_CASE("TensorData storage type for 4D is Dense", "[TensorData]") {
    std::vector<float> data(24, 1.0f);
    auto tensor = TensorData::createND(
        data, {{"a", 2}, {"b", 3}, {"c", 2}, {"d", 2}});

    CHECK(tensor.storage().getStorageType() == TensorStorageType::Dense);

    auto const * dense = tensor.storage().tryGetAs<DenseTensorStorage>();
    CHECK(dense != nullptr);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("TensorData 1D tensor via createND", "[TensorData]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f};
    auto tensor = TensorData::createND(data, {{"values", 3}});

    CHECK(tensor.ndim() == 1);
    CHECK(tensor.shape() == std::vector<std::size_t>{3});
    CHECK(tensor.numRows() == 3); // ordinal rows = axis(0).size
    CHECK(tensor.numColumns() == 1); // 1D gets numColumns() == 1
}

TEST_CASE("TensorData getColumn on empty throws", "[TensorData]") {
    TensorData tensor;
    CHECK_THROWS_AS(tensor.getColumn(0), std::runtime_error);
}

TEST_CASE("TensorData getColumn out of range throws", "[TensorData]") {
    auto tensor = TensorData::createOrdinal2D({1.0f, 2.0f}, 1, 2);
    CHECK_THROWS_AS(tensor.getColumn(5), std::out_of_range);
}

TEST_CASE("TensorData getColumn by nonexistent name throws", "[TensorData]") {
    auto tensor = TensorData::createOrdinal2D(
        {1.0f, 2.0f}, 1, 2, {"a", "b"});
    CHECK_THROWS_AS(tensor.getColumn("nonexistent"), std::invalid_argument);
}

TEST_CASE("TensorData at on empty throws", "[TensorData]") {
    TensorData tensor;
    std::vector<std::size_t> idx = {0};
    CHECK_THROWS_AS(tensor.at(idx), std::runtime_error);
}

TEST_CASE("TensorData row on empty throws", "[TensorData]") {
    TensorData tensor;
    CHECK_THROWS_AS(tensor.row(0), std::runtime_error);
}

TEST_CASE("TensorData row out of range throws", "[TensorData]") {
    auto tensor = TensorData::createOrdinal2D({1.0f, 2.0f}, 1, 2);
    CHECK_THROWS_AS(tensor.row(5), std::out_of_range);
}

TEST_CASE("TensorData flatData on empty throws", "[TensorData]") {
    TensorData tensor;
    CHECK_THROWS_AS(tensor.flatData(), std::runtime_error);
}

// =============================================================================
// RowDescriptor access through TensorData
// =============================================================================

TEST_CASE("TensorData rows() returns correct descriptor for time-indexed", "[TensorData]") {
    auto ts = makeDenseTimeStorage(3);
    auto tf = makeTimeFrame(100);
    auto tensor = TensorData::createTimeSeries2D(
        {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}, 3, 2, ts, tf);

    auto const & rows = tensor.rows();
    CHECK(rows.type() == RowType::TimeFrameIndex);
    CHECK(rows.count() == 3);
    CHECK(rows.timeFrame() != nullptr);
}

TEST_CASE("TensorData rows() returns correct descriptor for intervals", "[TensorData]") {
    auto tf = makeTimeFrame(100);
    std::vector<TimeFrameInterval> intervals = {
        {TimeFrameIndex{0}, TimeFrameIndex{10}},
        {TimeFrameIndex{20}, TimeFrameIndex{30}}
    };

    auto tensor = TensorData::createFromIntervals(
        {1.0f, 2.0f, 3.0f, 4.0f}, 2, 2, intervals, tf);

    auto const & rows = tensor.rows();
    CHECK(rows.type() == RowType::Interval);
    CHECK(rows.count() == 2);
    auto spans = rows.intervals();
    REQUIRE(spans.size() == 2);
}

// =============================================================================
// DimensionDescriptor access through TensorData
// =============================================================================

TEST_CASE("TensorData dimensions() provides axis lookup", "[TensorData]") {
    auto ts = makeDenseTimeStorage(3);
    auto tf = makeTimeFrame(100);
    auto tensor = TensorData::createTimeSeries2D(
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

// =============================================================================
// LibTorch-Specific Tests
// =============================================================================

#ifdef TENSOR_BACKEND_LIBTORCH

#include "Tensors/storage/LibTorchTensorStorage.hpp"

TEST_CASE("TensorData createFromTorch basic", "[TensorData][LibTorch]") {
    auto torch_tensor = torch::tensor({{1.0f, 2.0f, 3.0f},
                                        {4.0f, 5.0f, 6.0f}});

    auto tensor = TensorData::createFromTorch(torch_tensor);

    CHECK(tensor.ndim() == 2);
    CHECK(tensor.numRows() == 2);
    CHECK(tensor.numColumns() == 3);
    CHECK_FALSE(tensor.isEmpty());
    CHECK(tensor.isContiguous());

    auto s = tensor.shape();
    REQUIRE(s.size() == 2);
    CHECK(s[0] == 2);
    CHECK(s[1] == 3);

    // Auto-generated axis names
    auto const & dims = tensor.dimensions();
    CHECK(dims.axis(0).name == "dim0");
    CHECK(dims.axis(1).name == "dim1");

    // Row type is ordinal
    CHECK(tensor.rowType() == RowType::Ordinal);
}

TEST_CASE("TensorData createFromTorch with named axes", "[TensorData][LibTorch]") {
    auto torch_tensor = torch::arange(24.0f).reshape({2, 3, 4});

    auto tensor = TensorData::createFromTorch(
        torch_tensor,
        {{"batch", 2}, {"height", 3}, {"width", 4}});

    CHECK(tensor.ndim() == 3);
    CHECK(tensor.numRows() == 2);

    auto const & dims = tensor.dimensions();
    CHECK(dims.axis(0).name == "batch");
    CHECK(dims.axis(1).name == "height");
    CHECK(dims.axis(2).name == "width");
}

TEST_CASE("TensorData createFromTorch element access", "[TensorData][LibTorch]") {
    auto torch_tensor = torch::tensor({{10.0f, 20.0f},
                                        {30.0f, 40.0f},
                                        {50.0f, 60.0f}});

    auto tensor = TensorData::createFromTorch(torch_tensor);

    // Element access
    CHECK_THAT(tensor.at(std::vector<std::size_t>{0, 0}), WithinAbs(10.0f, 1e-5f));
    CHECK_THAT(tensor.at(std::vector<std::size_t>{1, 1}), WithinAbs(40.0f, 1e-5f));
    CHECK_THAT(tensor.at(std::vector<std::size_t>{2, 0}), WithinAbs(50.0f, 1e-5f));

    // Row access
    auto row1 = tensor.row(1);
    REQUIRE(row1.size() == 2);
    CHECK_THAT(row1[0], WithinAbs(30.0f, 1e-5f));
    CHECK_THAT(row1[1], WithinAbs(40.0f, 1e-5f));

    // Column access
    auto col0 = tensor.getColumn(0);
    REQUIRE(col0.size() == 3);
    CHECK_THAT(col0[0], WithinAbs(10.0f, 1e-5f));
    CHECK_THAT(col0[1], WithinAbs(30.0f, 1e-5f));
    CHECK_THAT(col0[2], WithinAbs(50.0f, 1e-5f));
}

TEST_CASE("TensorData createFromTorch 4D (model I/O)", "[TensorData][LibTorch]") {
    // Simulate batch × channel × height × width
    auto torch_tensor = torch::arange(120.0f).reshape({2, 3, 4, 5});

    auto tensor = TensorData::createFromTorch(
        torch_tensor,
        {{"batch", 2}, {"channel", 3}, {"height", 4}, {"width", 5}});

    CHECK(tensor.ndim() == 4);
    CHECK(tensor.numRows() == 2);
    CHECK(tensor.storage().getStorageType() == TensorStorageType::LibTorch);

    // Access element [1, 2, 3, 4] = 1*60 + 2*20 + 3*5 + 4 = 119
    CHECK_THAT(tensor.at(std::vector<std::size_t>{1, 2, 3, 4}), WithinAbs(119.0f, 1e-5f));
}

TEST_CASE("TensorData asTorchTensor zero-copy", "[TensorData][LibTorch]") {
    auto torch_tensor = torch::tensor({{1.0f, 2.0f}, {3.0f, 4.0f}});
    auto tensor = TensorData::createFromTorch(torch_tensor);

    auto const & recovered = tensor.asTorchTensor();
    CHECK(recovered.size(0) == 2);
    CHECK(recovered.size(1) == 2);
    CHECK(recovered.scalar_type() == torch::kFloat32);

    // Verify data matches
    CHECK(recovered[0][0].item<float>() == 1.0f);
    CHECK(recovered[1][1].item<float>() == 4.0f);
}

TEST_CASE("TensorData toLibTorch from Armadillo backend", "[TensorData][LibTorch]") {
    // Create with Armadillo backend, then convert
    auto tensor = TensorData::createOrdinal2D(
        {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}, 2, 3);

    CHECK(tensor.storage().getStorageType() == TensorStorageType::Armadillo);

    auto torch_tensor = tensor.toLibTorch();
    CHECK(torch_tensor.storage().getStorageType() == TensorStorageType::LibTorch);

    // Data should be preserved
    CHECK_THAT(torch_tensor.at(std::vector<std::size_t>{0, 0}), WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(torch_tensor.at(std::vector<std::size_t>{0, 2}), WithinAbs(3.0f, 1e-5f));
    CHECK_THAT(torch_tensor.at(std::vector<std::size_t>{1, 0}), WithinAbs(4.0f, 1e-5f));
    CHECK_THAT(torch_tensor.at(std::vector<std::size_t>{1, 2}), WithinAbs(6.0f, 1e-5f));
}

TEST_CASE("TensorData toLibTorch is no-op for LibTorch backend", "[TensorData][LibTorch]") {
    auto torch_tensor = torch::tensor({{1.0f, 2.0f}, {3.0f, 4.0f}});
    auto tensor = TensorData::createFromTorch(torch_tensor);

    auto converted = tensor.toLibTorch();
    CHECK(converted.storage().getStorageType() == TensorStorageType::LibTorch);
    // Should be the same data (shallow copy)
}

TEST_CASE("TensorData toLibTorch preserves metadata", "[TensorData][LibTorch]") {
    auto tensor = TensorData::createOrdinal2D(
        {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}, 2, 3,
        {"a", "b", "c"});

    auto torch_tensor = tensor.toLibTorch();

    // Column names preserved
    CHECK(torch_tensor.hasNamedColumns());
    CHECK(torch_tensor.columnNames().size() == 3);
    CHECK(torch_tensor.columnNames()[0] == "a");
    CHECK(torch_tensor.columnNames()[1] == "b");
    CHECK(torch_tensor.columnNames()[2] == "c");

    // Dimensions preserved
    CHECK(torch_tensor.ndim() == 2);
    CHECK(torch_tensor.numRows() == 2);
    CHECK(torch_tensor.numColumns() == 3);
}

TEST_CASE("TensorData materializeFlat from LibTorch backend", "[TensorData][LibTorch]") {
    auto torch_tensor = torch::tensor({{1.0f, 2.0f, 3.0f},
                                        {4.0f, 5.0f, 6.0f}});
    auto tensor = TensorData::createFromTorch(torch_tensor);

    auto flat = tensor.materializeFlat();
    REQUIRE(flat.size() == 6);

    // LibTorch is row-major (C contiguous), so materializeFlat should give
    // the same order as the original data
    CHECK_THAT(flat[0], WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(flat[1], WithinAbs(2.0f, 1e-5f));
    CHECK_THAT(flat[2], WithinAbs(3.0f, 1e-5f));
    CHECK_THAT(flat[3], WithinAbs(4.0f, 1e-5f));
    CHECK_THAT(flat[4], WithinAbs(5.0f, 1e-5f));
    CHECK_THAT(flat[5], WithinAbs(6.0f, 1e-5f));
}

TEST_CASE("TensorData createFromTorch error handling", "[TensorData][LibTorch]") {
    SECTION("undefined tensor") {
        torch::Tensor undefined;
        CHECK_THROWS_AS(TensorData::createFromTorch(undefined), std::invalid_argument);
    }

    SECTION("scalar tensor") {
        auto scalar = torch::tensor(42.0f);
        CHECK_THROWS_AS(TensorData::createFromTorch(scalar), std::invalid_argument);
    }

    SECTION("axes count mismatch") {
        auto t = torch::tensor({{1.0f, 2.0f}, {3.0f, 4.0f}});
        CHECK_THROWS_AS(
            TensorData::createFromTorch(t, {{"only_one", 2}}),
            std::invalid_argument);
    }
}

TEST_CASE("TensorData asTorchTensor errors", "[TensorData][LibTorch]") {
    SECTION("empty tensor throws") {
        TensorData empty;
        CHECK_THROWS_AS(empty.asTorchTensor(), std::logic_error);
    }

    SECTION("non-LibTorch backend throws") {
        auto tensor = TensorData::createOrdinal2D(
            {1.0f, 2.0f, 3.0f, 4.0f}, 2, 2);
        CHECK_THROWS_AS(tensor.asTorchTensor(), std::logic_error);
    }
}

TEST_CASE("TensorData LibTorch backend with tryGetAs", "[TensorData][LibTorch]") {
    auto torch_tensor = torch::tensor({{1.0f, 2.0f}, {3.0f, 4.0f}});
    auto tensor = TensorData::createFromTorch(torch_tensor);

    auto const * libtorch = tensor.storage().tryGetAs<LibTorchTensorStorage>();
    REQUIRE(libtorch != nullptr);
    CHECK(libtorch->isCpu());
    CHECK(libtorch->ndim() == 2);

    auto const * arma = tensor.storage().tryGetAs<ArmadilloTensorStorage>();
    CHECK(arma == nullptr);
}

TEST_CASE("TensorData createFromTorch auto-converts double to float", "[TensorData][LibTorch]") {
    // Create a double tensor — createFromTorch should convert to float
    auto double_tensor = torch::tensor({{1.0, 2.0}, {3.0, 4.0}}, torch::kFloat64);

    auto tensor = TensorData::createFromTorch(double_tensor);
    CHECK(tensor.ndim() == 2);
    CHECK_THAT(tensor.at(std::vector<std::size_t>{0, 0}), WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(tensor.at(std::vector<std::size_t>{1, 1}), WithinAbs(4.0f, 1e-5f));

    // Underlying storage should be float
    auto const & t = tensor.asTorchTensor();
    CHECK(t.scalar_type() == torch::kFloat32);
}

#endif // TENSOR_BACKEND_LIBTORCH
