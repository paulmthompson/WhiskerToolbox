/**
 * @file LazyColumnTensorStorage.test.cpp
 * @brief Unit tests for LazyColumnTensorStorage
 *
 * Tests cover:
 * - Construction (valid, invalid inputs)
 * - Lazy materialization (on-demand column computation)
 * - Per-column caching and invalidation
 * - Element access (getValueAt)
 * - Column extraction (getColumn)
 * - Row extraction (sliceAlongAxis axis=0)
 * - Column extraction via slice (sliceAlongAxis axis=1)
 * - Shape, totalElements, isContiguous, getStorageType metadata
 * - materializeAll, materializeFlat
 * - setColumnProvider (dynamic reconfiguration)
 * - flatData throws (non-contiguous)
 * - tryGetCache returns invalid
 * - Provider call counting (lazy evaluation verification)
 * - Provider returning wrong size (error handling)
 * - Integration with TensorStorageWrapper (type-erasure)
 * - Integration with TensorData::createFromLazyColumns
 * - InvalidationWiringFn callback
 * - Mixed materialized + lazy columns
 * - Observer notification via wiring callback
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Tensors/TensorData.hpp"
#include "Tensors/storage/LazyColumnTensorStorage.hpp"
#include "Tensors/storage/TensorStorageWrapper.hpp"

#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;

// =============================================================================
// Helpers
// =============================================================================

namespace {

/// Create a simple column provider that returns sequential floats starting at `start`
ColumnProviderFn makeSequentialProvider(std::size_t count, float start = 0.0f) {
    return [count, start]() -> std::vector<float> {
        std::vector<float> data(count);
        for (std::size_t i = 0; i < count; ++i) {
            data[i] = start + static_cast<float>(i);
        }
        return data;
    };
}

/// Create a provider that counts how many times it's been called
ColumnProviderFn makeCountingProvider(std::size_t count, float value,
                                       std::shared_ptr<std::atomic<int>> call_count) {
    return [count, value, call_count]() -> std::vector<float> {
        (*call_count)++;
        return std::vector<float>(count, value);
    };
}

/// Create a provider that returns wrong size (for error testing)
ColumnProviderFn makeBadSizeProvider(std::size_t wrong_size) {
    return [wrong_size]() -> std::vector<float> {
        return std::vector<float>(wrong_size, 0.0f);
    };
}

/// Create a simple TimeFrame
std::shared_ptr<TimeFrame> makeTimeFrame(std::size_t size) {
    std::vector<int> timestamps(size);
    for (std::size_t i = 0; i < size; ++i) {
        timestamps[i] = static_cast<int>(i);
    }
    return std::make_shared<TimeFrame>(timestamps);
}

/// Create a dense TimeIndexStorage
std::shared_ptr<TimeIndexStorage> makeDenseTimeStorage(std::size_t count) {
    return TimeIndexStorageFactory::createDenseFromZero(count);
}

} // anonymous namespace

// =============================================================================
// Construction Tests
// =============================================================================

TEST_CASE("LazyColumnTensorStorage construction", "[LazyColumnTensorStorage]") {
    SECTION("valid construction") {
        std::vector<ColumnSource> cols = {
            {"col_a", makeSequentialProvider(5), {}},
            {"col_b", makeSequentialProvider(5, 10.0f), {}}
        };
        LazyColumnTensorStorage storage(5, std::move(cols));

        CHECK(storage.numRows() == 5);
        CHECK(storage.numColumns() == 2);
        CHECK(storage.getStorageType() == TensorStorageType::Lazy);
        CHECK_FALSE(storage.isContiguous());
    }

    SECTION("rejects zero rows") {
        std::vector<ColumnSource> cols = {
            {"col", makeSequentialProvider(0), {}}
        };
        CHECK_THROWS_AS(
            LazyColumnTensorStorage(0, std::move(cols)),
            std::invalid_argument);
    }

    SECTION("rejects empty columns") {
        CHECK_THROWS_AS(
            LazyColumnTensorStorage(5, {}),
            std::invalid_argument);
    }

    SECTION("rejects null provider") {
        std::vector<ColumnSource> cols = {
            {"col", nullptr, {}}
        };
        CHECK_THROWS_AS(
            LazyColumnTensorStorage(5, std::move(cols)),
            std::invalid_argument);
    }
}

// =============================================================================
// Shape and Metadata Tests
// =============================================================================

TEST_CASE("LazyColumnTensorStorage shape and metadata", "[LazyColumnTensorStorage]") {
    std::vector<ColumnSource> cols = {
        {"x", makeSequentialProvider(4), {}},
        {"y", makeSequentialProvider(4, 10.0f), {}},
        {"z", makeSequentialProvider(4, 20.0f), {}}
    };
    LazyColumnTensorStorage storage(4, std::move(cols));

    SECTION("shape") {
        auto s = storage.shape();
        REQUIRE(s.size() == 2);
        CHECK(s[0] == 4);
        CHECK(s[1] == 3);
    }

    SECTION("totalElements") {
        CHECK(storage.totalElements() == 12);
    }

    SECTION("isContiguous is false") {
        CHECK_FALSE(storage.isContiguous());
    }

    SECTION("storageType is Lazy") {
        CHECK(storage.getStorageType() == TensorStorageType::Lazy);
    }

    SECTION("tryGetCache is invalid") {
        auto cache = storage.tryGetCache();
        CHECK_FALSE(cache.isValid());
    }

    SECTION("columnNames") {
        auto names = storage.columnNames();
        REQUIRE(names.size() == 3);
        CHECK(names[0] == "x");
        CHECK(names[1] == "y");
        CHECK(names[2] == "z");
    }
}

// =============================================================================
// Lazy Materialization Tests
// =============================================================================

TEST_CASE("LazyColumnTensorStorage lazy materialization", "[LazyColumnTensorStorage]") {
    auto call_count_a = std::make_shared<std::atomic<int>>(0);
    auto call_count_b = std::make_shared<std::atomic<int>>(0);

    std::vector<ColumnSource> cols = {
        {"a", makeCountingProvider(3, 1.0f, call_count_a), {}},
        {"b", makeCountingProvider(3, 2.0f, call_count_b), {}}
    };
    LazyColumnTensorStorage storage(3, std::move(cols));

    SECTION("providers not called on construction") {
        CHECK(*call_count_a == 0);
        CHECK(*call_count_b == 0);
    }

    SECTION("provider called on first column access") {
        auto col = storage.getColumn(0);
        CHECK(*call_count_a == 1);
        CHECK(*call_count_b == 0);
        REQUIRE(col.size() == 3);
        CHECK(col[0] == 1.0f);
    }

    SECTION("provider called only once (cached)") {
        storage.getColumn(0);
        CHECK(*call_count_a == 1);
        storage.getColumn(0);
        CHECK(*call_count_a == 1); // Still 1, cached
    }

    SECTION("each column is independent") {
        storage.getColumn(1);
        CHECK(*call_count_a == 0);
        CHECK(*call_count_b == 1);
    }

    SECTION("materializeAll calls all providers") {
        storage.materializeAll();
        CHECK(*call_count_a == 1);
        CHECK(*call_count_b == 1);
    }

    SECTION("materializeColumn is idempotent") {
        storage.materializeColumn(0);
        storage.materializeColumn(0);
        CHECK(*call_count_a == 1);
    }
}

// =============================================================================
// Invalidation Tests
// =============================================================================

TEST_CASE("LazyColumnTensorStorage invalidation", "[LazyColumnTensorStorage]") {
    auto call_count = std::make_shared<std::atomic<int>>(0);

    std::vector<ColumnSource> cols = {
        {"a", makeCountingProvider(3, 1.0f, call_count), {}},
        {"b", makeSequentialProvider(3, 10.0f), {}}
    };
    LazyColumnTensorStorage storage(3, std::move(cols));

    SECTION("invalidateColumn triggers recomputation") {
        storage.getColumn(0);
        CHECK(*call_count == 1);

        storage.invalidateColumn(0);
        storage.getColumn(0);
        CHECK(*call_count == 2);
    }

    SECTION("invalidateAll clears all caches") {
        storage.materializeAll();
        CHECK(*call_count == 1);

        storage.invalidateAll();
        storage.getColumn(0);
        CHECK(*call_count == 2);
    }

    SECTION("invalidateColumn out of range throws") {
        CHECK_THROWS_AS(storage.invalidateColumn(99), std::out_of_range);
    }
}

// =============================================================================
// Element Access Tests
// =============================================================================

TEST_CASE("LazyColumnTensorStorage element access", "[LazyColumnTensorStorage]") {
    // 3 rows × 2 columns
    // col 0: [0, 1, 2]
    // col 1: [10, 11, 12]
    std::vector<ColumnSource> cols = {
        {"a", makeSequentialProvider(3, 0.0f), {}},
        {"b", makeSequentialProvider(3, 10.0f), {}}
    };
    LazyColumnTensorStorage storage(3, std::move(cols));

    SECTION("getValueAt") {
        std::size_t idx00[] = {0, 0};
        CHECK_THAT(storage.getValueAt(idx00), WithinAbs(0.0f, 1e-6f));

        std::size_t idx10[] = {1, 0};
        CHECK_THAT(storage.getValueAt(idx10), WithinAbs(1.0f, 1e-6f));

        std::size_t idx21[] = {2, 1};
        CHECK_THAT(storage.getValueAt(idx21), WithinAbs(12.0f, 1e-6f));

        std::size_t idx01[] = {0, 1};
        CHECK_THAT(storage.getValueAt(idx01), WithinAbs(10.0f, 1e-6f));
    }

    SECTION("getValueAt with wrong number of indices throws") {
        std::size_t idx[] = {0};
        CHECK_THROWS_AS(storage.getValueAt(idx), std::invalid_argument);

        std::size_t idx3[] = {0, 0, 0};
        CHECK_THROWS_AS(storage.getValueAt(idx3), std::invalid_argument);
    }

    SECTION("getValueAt with out-of-range row throws") {
        std::size_t idx[] = {3, 0};
        CHECK_THROWS_AS(storage.getValueAt(idx), std::out_of_range);
    }

    SECTION("getValueAt with out-of-range column throws") {
        std::size_t idx[] = {0, 2};
        CHECK_THROWS_AS(storage.getValueAt(idx), std::out_of_range);
    }
}

// =============================================================================
// Column Extraction Tests
// =============================================================================

TEST_CASE("LazyColumnTensorStorage getColumn", "[LazyColumnTensorStorage]") {
    std::vector<ColumnSource> cols = {
        {"a", makeSequentialProvider(4, 0.0f), {}},
        {"b", makeSequentialProvider(4, 100.0f), {}}
    };
    LazyColumnTensorStorage storage(4, std::move(cols));

    SECTION("column 0") {
        auto col = storage.getColumn(0);
        REQUIRE(col.size() == 4);
        CHECK_THAT(col[0], WithinAbs(0.0f, 1e-6f));
        CHECK_THAT(col[1], WithinAbs(1.0f, 1e-6f));
        CHECK_THAT(col[2], WithinAbs(2.0f, 1e-6f));
        CHECK_THAT(col[3], WithinAbs(3.0f, 1e-6f));
    }

    SECTION("column 1") {
        auto col = storage.getColumn(1);
        REQUIRE(col.size() == 4);
        CHECK_THAT(col[0], WithinAbs(100.0f, 1e-6f));
        CHECK_THAT(col[3], WithinAbs(103.0f, 1e-6f));
    }

    SECTION("out of range throws") {
        CHECK_THROWS_AS(storage.getColumn(2), std::out_of_range);
    }
}

// =============================================================================
// Slice Tests
// =============================================================================

TEST_CASE("LazyColumnTensorStorage sliceAlongAxis", "[LazyColumnTensorStorage]") {
    // 3 rows × 2 columns
    // col 0: [0, 1, 2], col 1: [10, 11, 12]
    std::vector<ColumnSource> cols = {
        {"a", makeSequentialProvider(3, 0.0f), {}},
        {"b", makeSequentialProvider(3, 10.0f), {}}
    };
    LazyColumnTensorStorage storage(3, std::move(cols));

    SECTION("axis 0 (row slice)") {
        auto row0 = storage.sliceAlongAxis(0, 0);
        REQUIRE(row0.size() == 2);
        CHECK_THAT(row0[0], WithinAbs(0.0f, 1e-6f));
        CHECK_THAT(row0[1], WithinAbs(10.0f, 1e-6f));

        auto row2 = storage.sliceAlongAxis(0, 2);
        REQUIRE(row2.size() == 2);
        CHECK_THAT(row2[0], WithinAbs(2.0f, 1e-6f));
        CHECK_THAT(row2[1], WithinAbs(12.0f, 1e-6f));
    }

    SECTION("axis 0 out of range throws") {
        CHECK_THROWS_AS(storage.sliceAlongAxis(0, 3), std::out_of_range);
    }

    SECTION("axis 1 (column slice) is same as getColumn") {
        auto col = storage.sliceAlongAxis(1, 0);
        auto col_direct = storage.getColumn(0);
        REQUIRE(col.size() == col_direct.size());
        for (std::size_t i = 0; i < col.size(); ++i) {
            CHECK_THAT(col[i], WithinAbs(col_direct[i], 1e-6f));
        }
    }

    SECTION("axis 2 throws (only 2D)") {
        CHECK_THROWS_AS(storage.sliceAlongAxis(2, 0), std::out_of_range);
    }
}

// =============================================================================
// materializeFlat Tests
// =============================================================================

TEST_CASE("LazyColumnTensorStorage materializeFlat", "[LazyColumnTensorStorage]") {
    // 2 rows × 3 columns
    // col 0: [0, 1], col 1: [10, 11], col 2: [20, 21]
    std::vector<ColumnSource> cols = {
        {"a", makeSequentialProvider(2, 0.0f), {}},
        {"b", makeSequentialProvider(2, 10.0f), {}},
        {"c", makeSequentialProvider(2, 20.0f), {}}
    };
    LazyColumnTensorStorage storage(2, std::move(cols));

    auto flat = storage.materializeFlat();
    REQUIRE(flat.size() == 6);

    // Row-major: [row0_col0, row0_col1, row0_col2, row1_col0, row1_col1, row1_col2]
    CHECK_THAT(flat[0], WithinAbs(0.0f, 1e-6f));   // [0,0]
    CHECK_THAT(flat[1], WithinAbs(10.0f, 1e-6f));  // [0,1]
    CHECK_THAT(flat[2], WithinAbs(20.0f, 1e-6f));  // [0,2]
    CHECK_THAT(flat[3], WithinAbs(1.0f, 1e-6f));   // [1,0]
    CHECK_THAT(flat[4], WithinAbs(11.0f, 1e-6f));  // [1,1]
    CHECK_THAT(flat[5], WithinAbs(21.0f, 1e-6f));  // [1,2]
}

// =============================================================================
// flatData throws
// =============================================================================

TEST_CASE("LazyColumnTensorStorage flatData throws", "[LazyColumnTensorStorage]") {
    std::vector<ColumnSource> cols = {
        {"col", makeSequentialProvider(3), {}}
    };
    LazyColumnTensorStorage storage(3, std::move(cols));
    CHECK_THROWS_AS(storage.flatData(), std::runtime_error);
}

// =============================================================================
// setColumnProvider Tests
// =============================================================================

TEST_CASE("LazyColumnTensorStorage setColumnProvider", "[LazyColumnTensorStorage]") {
    auto count_original = std::make_shared<std::atomic<int>>(0);
    auto count_replacement = std::make_shared<std::atomic<int>>(0);

    std::vector<ColumnSource> cols = {
        {"original", makeCountingProvider(3, 1.0f, count_original), {}}
    };
    LazyColumnTensorStorage storage(3, std::move(cols));

    // Materialize original
    auto col = storage.getColumn(0);
    CHECK(*count_original == 1);
    CHECK(col[0] == 1.0f);

    // Replace provider
    storage.setColumnProvider(0, "replaced",
        makeCountingProvider(3, 99.0f, count_replacement));

    // Name updated
    CHECK(storage.columnNames()[0] == "replaced");

    // Cache was invalidated, next access calls new provider
    auto col2 = storage.getColumn(0);
    CHECK(*count_original == 1);   // original not called again
    CHECK(*count_replacement == 1);
    CHECK(col2[0] == 99.0f);

    SECTION("null provider throws") {
        CHECK_THROWS_AS(
            storage.setColumnProvider(0, "bad", nullptr),
            std::invalid_argument);
    }

    SECTION("out of range throws") {
        CHECK_THROWS_AS(
            storage.setColumnProvider(99, "bad", makeSequentialProvider(3)),
            std::out_of_range);
    }
}

// =============================================================================
// Error Handling: Provider Returns Wrong Size
// =============================================================================

TEST_CASE("LazyColumnTensorStorage provider wrong size throws", "[LazyColumnTensorStorage]") {
    std::vector<ColumnSource> cols = {
        {"bad", makeBadSizeProvider(10), {}}  // expects 3, returns 10
    };
    LazyColumnTensorStorage storage(3, std::move(cols));

    CHECK_THROWS_AS(storage.getColumn(0), std::runtime_error);
}

// =============================================================================
// TensorStorageWrapper Integration
// =============================================================================

TEST_CASE("LazyColumnTensorStorage with TensorStorageWrapper", "[LazyColumnTensorStorage]") {
    std::vector<ColumnSource> cols = {
        {"x", makeSequentialProvider(4, 0.0f), {}},
        {"y", makeSequentialProvider(4, 10.0f), {}}
    };
    TensorStorageWrapper wrapper{
        LazyColumnTensorStorage{4, std::move(cols)}};

    SECTION("basic queries through wrapper") {
        CHECK(wrapper.isValid());
        CHECK(wrapper.getStorageType() == TensorStorageType::Lazy);
        CHECK_FALSE(wrapper.isContiguous());

        auto s = wrapper.shape();
        REQUIRE(s.size() == 2);
        CHECK(s[0] == 4);
        CHECK(s[1] == 2);
        CHECK(wrapper.totalElements() == 8);
    }

    SECTION("getColumn through wrapper") {
        auto col = wrapper.getColumn(0);
        REQUIRE(col.size() == 4);
        CHECK_THAT(col[0], WithinAbs(0.0f, 1e-6f));
        CHECK_THAT(col[3], WithinAbs(3.0f, 1e-6f));
    }

    SECTION("getValueAt through wrapper") {
        std::size_t idx[] = {2, 1};
        CHECK_THAT(wrapper.getValueAt(idx), WithinAbs(12.0f, 1e-6f));
    }

    SECTION("tryGetAs recovers concrete type") {
        auto const * lazy = wrapper.tryGetAs<LazyColumnTensorStorage>();
        REQUIRE(lazy != nullptr);
        CHECK(lazy->numColumns() == 2);
        CHECK(lazy->numRows() == 4);
    }

    SECTION("tryGetCache is invalid") {
        auto cache = wrapper.tryGetCache();
        CHECK_FALSE(cache.isValid());
    }

    SECTION("flatData throws through wrapper") {
        CHECK_THROWS_AS(wrapper.flatData(), std::runtime_error);
    }
}

// =============================================================================
// TensorData::createFromLazyColumns Integration
// =============================================================================

TEST_CASE("TensorData createFromLazyColumns basic", "[TensorData][LazyColumnTensorStorage]") {
    std::vector<ColumnSource> cols = {
        {"feature_a", makeSequentialProvider(5, 0.0f), {}},
        {"feature_b", makeSequentialProvider(5, 100.0f), {}}
    };

    auto tensor = TensorData::createFromLazyColumns(
        5, std::move(cols), RowDescriptor::ordinal(5));

    SECTION("basic metadata") {
        CHECK(tensor.ndim() == 2);
        CHECK(tensor.numRows() == 5);
        CHECK(tensor.numColumns() == 2);
        CHECK(tensor.rowType() == RowType::Ordinal);
        CHECK(tensor.hasNamedColumns());
        CHECK_FALSE(tensor.isEmpty());
    }

    SECTION("shape") {
        auto s = tensor.shape();
        REQUIRE(s.size() == 2);
        CHECK(s[0] == 5);
        CHECK(s[1] == 2);
    }

    SECTION("column names") {
        auto const & names = tensor.columnNames();
        REQUIRE(names.size() == 2);
        CHECK(names[0] == "feature_a");
        CHECK(names[1] == "feature_b");
    }

    SECTION("getColumn by name") {
        auto col = tensor.getColumn("feature_a");
        REQUIRE(col.size() == 5);
        CHECK_THAT(col[0], WithinAbs(0.0f, 1e-6f));
        CHECK_THAT(col[4], WithinAbs(4.0f, 1e-6f));
    }

    SECTION("getColumn by index") {
        auto col = tensor.getColumn(1);
        REQUIRE(col.size() == 5);
        CHECK_THAT(col[0], WithinAbs(100.0f, 1e-6f));
        CHECK_THAT(col[4], WithinAbs(104.0f, 1e-6f));
    }

    SECTION("element access via at()") {
        std::size_t idx[] = {3, 0};
        CHECK_THAT(tensor.at(idx), WithinAbs(3.0f, 1e-6f));

        std::size_t idx2[] = {0, 1};
        CHECK_THAT(tensor.at(idx2), WithinAbs(100.0f, 1e-6f));
    }

    SECTION("row access") {
        auto r = tensor.row(2);
        REQUIRE(r.size() == 2);
        CHECK_THAT(r[0], WithinAbs(2.0f, 1e-6f));
        CHECK_THAT(r[1], WithinAbs(102.0f, 1e-6f));
    }

    SECTION("isContiguous is false") {
        CHECK_FALSE(tensor.isContiguous());
    }

    SECTION("storage type is Lazy") {
        CHECK(tensor.storage().getStorageType() == TensorStorageType::Lazy);
    }

    SECTION("materializeFlat works") {
        auto flat = tensor.materializeFlat();
        REQUIRE(flat.size() == 10);
        // Row-major: [row0_col0, row0_col1, row1_col0, ...]
        CHECK_THAT(flat[0], WithinAbs(0.0f, 1e-6f));    // [0,0]
        CHECK_THAT(flat[1], WithinAbs(100.0f, 1e-6f));  // [0,1]
        CHECK_THAT(flat[2], WithinAbs(1.0f, 1e-6f));    // [1,0]
        CHECK_THAT(flat[3], WithinAbs(101.0f, 1e-6f));  // [1,1]
    }
}

TEST_CASE("TensorData createFromLazyColumns with time rows", "[TensorData][LazyColumnTensorStorage]") {
    auto ts = makeDenseTimeStorage(4);
    auto tf = makeTimeFrame(100);

    std::vector<ColumnSource> cols = {
        {"signal", makeSequentialProvider(4, 5.0f), {}}
    };

    auto tensor = TensorData::createFromLazyColumns(
        4, std::move(cols),
        RowDescriptor::fromTimeIndices(ts, tf));

    CHECK(tensor.rowType() == RowType::TimeFrameIndex);
    CHECK(tensor.numRows() == 4);
    CHECK(tensor.getTimeFrame() != nullptr);

    auto col = tensor.getColumn("signal");
    REQUIRE(col.size() == 4);
    CHECK_THAT(col[0], WithinAbs(5.0f, 1e-6f));
}

TEST_CASE("TensorData createFromLazyColumns with interval rows", "[TensorData][LazyColumnTensorStorage]") {
    auto tf = makeTimeFrame(100);
    std::vector<TimeFrameInterval> intervals = {
        {TimeFrameIndex{0}, TimeFrameIndex{10}},
        {TimeFrameIndex{10}, TimeFrameIndex{20}},
        {TimeFrameIndex{20}, TimeFrameIndex{30}}
    };

    std::vector<ColumnSource> cols = {
        {"mean_value", []() -> std::vector<float> { return {1.5f, 2.3f, 0.8f}; }, {}},
        {"count", []() -> std::vector<float> { return {10.0f, 15.0f, 8.0f}; }, {}}
    };

    auto tensor = TensorData::createFromLazyColumns(
        3, std::move(cols),
        RowDescriptor::fromIntervals(intervals, tf));

    CHECK(tensor.rowType() == RowType::Interval);
    CHECK(tensor.numRows() == 3);
    CHECK(tensor.numColumns() == 2);

    auto means = tensor.getColumn("mean_value");
    REQUIRE(means.size() == 3);
    CHECK_THAT(means[1], WithinAbs(2.3f, 1e-5f));

    auto counts = tensor.getColumn("count");
    REQUIRE(counts.size() == 3);
    CHECK_THAT(counts[2], WithinAbs(8.0f, 1e-6f));
}

// =============================================================================
// InvalidationWiringFn Tests
// =============================================================================

TEST_CASE("TensorData createFromLazyColumns with wiring callback", "[TensorData][LazyColumnTensorStorage]") {
    bool wiring_called = false;
    LazyColumnTensorStorage * wired_storage = nullptr;
    TensorData * wired_tensor = nullptr;

    std::vector<ColumnSource> cols = {
        {"col", makeSequentialProvider(3), {}}
    };

    auto wiring = [&](LazyColumnTensorStorage & lazy, TensorData & td) {
        wiring_called = true;
        wired_storage = &lazy;
        wired_tensor = &td;
    };

    auto tensor = TensorData::createFromLazyColumns(
        3, std::move(cols), RowDescriptor::ordinal(3), wiring);

    CHECK(wiring_called);
    CHECK(wired_storage != nullptr);
    CHECK(wired_tensor != nullptr);
}

TEST_CASE("TensorData createFromLazyColumns wiring enables observer invalidation",
           "[TensorData][LazyColumnTensorStorage]") {
    auto call_count = std::make_shared<std::atomic<int>>(0);
    int observer_notifications = 0;

    std::vector<ColumnSource> cols = {
        {"dynamic", makeCountingProvider(3, 42.0f, call_count), {}}
    };

    // Simulate a wiring callback that would be called by an external source change
    // We capture the storage pointer and tensor pointer so we can invalidate later
    LazyColumnTensorStorage * stored_lazy = nullptr;
    TensorData * stored_td = nullptr;

    auto wiring = [&](LazyColumnTensorStorage & lazy, TensorData & td) {
        stored_lazy = &lazy;
        stored_td = &td;
    };

    auto tensor = TensorData::createFromLazyColumns(
        3, std::move(cols), RowDescriptor::ordinal(3), wiring);

    // Register an observer on the tensor
    tensor.addObserver([&]() { observer_notifications++; });

    // First access: provider called once
    auto col1 = tensor.getColumn("dynamic");
    CHECK(*call_count == 1);
    CHECK(col1[0] == 42.0f);

    // Simulate external invalidation (as would happen from observer callback)
    REQUIRE(stored_lazy != nullptr);
    stored_lazy->invalidateColumn(0);
    stored_td->notifyObservers();
    CHECK(observer_notifications == 1);

    // Next access: provider called again (cache was invalidated)
    auto col2 = tensor.getColumn("dynamic");
    CHECK(*call_count == 2);
}

// =============================================================================
// Mixed Materialized + Lazy Columns
// =============================================================================

TEST_CASE("LazyColumnTensorStorage mixed materialized and lazy", "[LazyColumnTensorStorage]") {
    // "Materialized" column: provider returns fixed data
    std::vector<float> precomputed = {10.0f, 20.0f, 30.0f};
    auto static_provider = [data = precomputed]() { return data; };

    // "Lazy" column: provider computes dynamically
    auto call_count = std::make_shared<std::atomic<int>>(0);
    auto dynamic_provider = makeCountingProvider(3, 5.0f, call_count);

    std::vector<ColumnSource> cols = {
        {"static_col", std::move(static_provider), {}},
        {"dynamic_col", std::move(dynamic_provider), {}}
    };
    LazyColumnTensorStorage storage(3, std::move(cols));

    auto static_col = storage.getColumn(0);
    REQUIRE(static_col.size() == 3);
    CHECK_THAT(static_col[0], WithinAbs(10.0f, 1e-6f));
    CHECK_THAT(static_col[2], WithinAbs(30.0f, 1e-6f));

    auto dynamic_col = storage.getColumn(1);
    REQUIRE(dynamic_col.size() == 3);
    CHECK(dynamic_col[0] == 5.0f);
    CHECK(*call_count == 1);
}

// =============================================================================
// Materialize (backend conversion) from Lazy
// =============================================================================

TEST_CASE("TensorData materialize from lazy storage", "[TensorData][LazyColumnTensorStorage]") {
    std::vector<ColumnSource> cols = {
        {"a", makeSequentialProvider(3, 0.0f), {}},
        {"b", makeSequentialProvider(3, 10.0f), {}}
    };

    auto lazy_tensor = TensorData::createFromLazyColumns(
        3, std::move(cols), RowDescriptor::ordinal(3));

    CHECK_FALSE(lazy_tensor.isContiguous());

    // Materialize to owned storage
    auto materialized = lazy_tensor.materialize();
    CHECK(materialized.isContiguous());
    CHECK(materialized.ndim() == 2);
    CHECK(materialized.numRows() == 3);
    CHECK(materialized.numColumns() == 2);

    // Verify data matches
    for (std::size_t r = 0; r < 3; ++r) {
        std::size_t idx_a[] = {r, 0};
        std::size_t idx_b[] = {r, 1};
        CHECK_THAT(materialized.at(idx_a),
            WithinAbs(lazy_tensor.at(idx_a), 1e-6f));
        CHECK_THAT(materialized.at(idx_b),
            WithinAbs(lazy_tensor.at(idx_b), 1e-6f));
    }
}

// =============================================================================
// Error Handling for createFromLazyColumns
// =============================================================================

TEST_CASE("TensorData createFromLazyColumns error handling", "[TensorData][LazyColumnTensorStorage]") {
    SECTION("zero rows") {
        std::vector<ColumnSource> cols = {
            {"col", makeSequentialProvider(0), {}}
        };
        CHECK_THROWS_AS(
            TensorData::createFromLazyColumns(0, std::move(cols),
                RowDescriptor::ordinal(0)),
            std::invalid_argument);
    }

    SECTION("empty columns") {
        CHECK_THROWS_AS(
            TensorData::createFromLazyColumns(3, {},
                RowDescriptor::ordinal(3)),
            std::invalid_argument);
    }
}

// =============================================================================
// Copy Semantics (shared via wrapper)
// =============================================================================

TEST_CASE("TensorData with lazy storage copy shares data", "[TensorData][LazyColumnTensorStorage]") {
    auto call_count = std::make_shared<std::atomic<int>>(0);

    std::vector<ColumnSource> cols = {
        {"col", makeCountingProvider(3, 7.0f, call_count), {}}
    };

    auto original = TensorData::createFromLazyColumns(
        3, std::move(cols), RowDescriptor::ordinal(3));

    // Access to populate cache
    original.getColumn(0);
    CHECK(*call_count == 1);

    // Copy
    auto copy = original;
    auto col = copy.getColumn(0);
    // Should use shared storage, no additional provider call
    CHECK(*call_count == 1);
    CHECK(col[0] == 7.0f);
}
