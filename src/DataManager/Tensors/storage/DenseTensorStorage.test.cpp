/**
 * @file DenseTensorStorage.test.cpp
 * @brief Unit tests for DenseTensorStorage (flat std::vector<float> + shape)
 *
 * Tests cover:
 * - Construction from flat data + shape
 * - Construction from shape only (zero-initialized)
 * - Shape, totalElements, isContiguous, ndim metadata
 * - Element access via getValueAt (2D, 3D, 4D, 5D)
 * - Column extraction via getColumn (2D, 3D, 4D)
 * - Axis slicing via sliceAlongAxis (2D, 3D, 4D)
 * - flatData access and row-major layout verification
 * - Cache support (tryGetCache) with stride verification
 * - Storage type reporting
 * - Mutable access (mutableFlatData, setValueAt)
 * - Error handling (mismatched sizes, out-of-range, wrong dimensionality)
 * - Edge cases (single element, dimension with size 1, large tensors)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Tensors/storage/DenseTensorStorage.hpp"

#include <cstddef>
#include <numeric>
#include <span>
#include <stdexcept>
#include <vector>

using Catch::Matchers::WithinAbs;

// =============================================================================
// Construction Tests
// =============================================================================

TEST_CASE("DenseTensorStorage construction from data and shape", "[DenseTensorStorage]") {
    // 2x3 matrix in row-major: [[1,2,3],[4,5,6]]
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    std::vector<std::size_t> shape = {2, 3};
    DenseTensorStorage storage(data, shape);

    SECTION("metadata") {
        CHECK(storage.ndim() == 2);
        CHECK(storage.totalElements() == 6);
        CHECK(storage.isContiguous());
        CHECK(storage.getStorageType() == TensorStorageType::Dense);

        auto s = storage.shape();
        REQUIRE(s.size() == 2);
        CHECK(s[0] == 2);
        CHECK(s[1] == 3);
    }

    SECTION("strides are row-major") {
        auto const & strides = storage.strides();
        REQUIRE(strides.size() == 2);
        CHECK(strides[0] == 3); // stride along rows = ncols
        CHECK(strides[1] == 1); // stride along cols = 1
    }
}

TEST_CASE("DenseTensorStorage construction from shape only", "[DenseTensorStorage]") {
    std::vector<std::size_t> shape = {3, 4, 5};
    DenseTensorStorage storage(shape);

    CHECK(storage.ndim() == 3);
    CHECK(storage.totalElements() == 60);

    // All zeros
    auto flat = storage.flatData();
    for (std::size_t i = 0; i < flat.size(); ++i) {
        CHECK(flat[i] == 0.0f);
    }
}

TEST_CASE("DenseTensorStorage construction validates data size", "[DenseTensorStorage]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f};
    std::vector<std::size_t> shape = {2, 3}; // expects 6 elements

    CHECK_THROWS_AS(DenseTensorStorage(data, shape), std::invalid_argument);
}

TEST_CASE("DenseTensorStorage construction rejects empty shape", "[DenseTensorStorage]") {
    std::vector<float> data;
    std::vector<std::size_t> empty_shape;

    CHECK_THROWS_AS(DenseTensorStorage(data, empty_shape), std::invalid_argument);
    CHECK_THROWS_AS(DenseTensorStorage(empty_shape), std::invalid_argument);
}

// =============================================================================
// 1D Tests
// =============================================================================

TEST_CASE("DenseTensorStorage 1D", "[DenseTensorStorage]") {
    std::vector<float> data = {10.0f, 20.0f, 30.0f, 40.0f};
    DenseTensorStorage storage(data, {4});

    CHECK(storage.ndim() == 1);
    CHECK(storage.totalElements() == 4);

    SECTION("element access") {
        std::size_t idx;
        idx = 0;
        CHECK_THAT(static_cast<double>(storage.getValueAt({&idx, 1})), WithinAbs(10.0, 1e-6));
        idx = 3;
        CHECK_THAT(static_cast<double>(storage.getValueAt({&idx, 1})), WithinAbs(40.0, 1e-6));
    }

    SECTION("flatData") {
        auto flat = storage.flatData();
        REQUIRE(flat.size() == 4);
        CHECK_THAT(static_cast<double>(flat[0]), WithinAbs(10.0, 1e-6));
        CHECK_THAT(static_cast<double>(flat[3]), WithinAbs(40.0, 1e-6));
    }

    SECTION("sliceAlongAxis returns single element") {
        auto slice = storage.sliceAlongAxis(0, 1);
        REQUIRE(slice.size() == 1);
        CHECK_THAT(static_cast<double>(slice[0]), WithinAbs(20.0, 1e-6));
    }

    SECTION("getColumn throws for 1D") {
        CHECK_THROWS_AS(storage.getColumn(0), std::invalid_argument);
    }

    SECTION("strides") {
        auto const & strides = storage.strides();
        REQUIRE(strides.size() == 1);
        CHECK(strides[0] == 1);
    }
}

// =============================================================================
// 2D Matrix Tests
// =============================================================================

TEST_CASE("DenseTensorStorage 2D element access", "[DenseTensorStorage]") {
    // 3x4 matrix:
    // Row 0: 1, 2, 3, 4
    // Row 1: 5, 6, 7, 8
    // Row 2: 9, 10, 11, 12
    std::vector<float> data;
    for (int i = 1; i <= 12; ++i) {
        data.push_back(static_cast<float>(i));
    }
    DenseTensorStorage storage(data, {3, 4});

    SECTION("row-major element access") {
        std::vector<std::size_t> idx;

        idx = {0, 0};
        CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(1.0, 1e-6));

        idx = {0, 3};
        CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(4.0, 1e-6));

        idx = {1, 0};
        CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(5.0, 1e-6));

        idx = {2, 3};
        CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(12.0, 1e-6));
    }

    SECTION("getColumn") {
        auto col0 = storage.getColumn(0);
        REQUIRE(col0.size() == 3);
        CHECK_THAT(static_cast<double>(col0[0]), WithinAbs(1.0, 1e-6));
        CHECK_THAT(static_cast<double>(col0[1]), WithinAbs(5.0, 1e-6));
        CHECK_THAT(static_cast<double>(col0[2]), WithinAbs(9.0, 1e-6));

        auto col3 = storage.getColumn(3);
        REQUIRE(col3.size() == 3);
        CHECK_THAT(static_cast<double>(col3[0]), WithinAbs(4.0, 1e-6));
        CHECK_THAT(static_cast<double>(col3[1]), WithinAbs(8.0, 1e-6));
        CHECK_THAT(static_cast<double>(col3[2]), WithinAbs(12.0, 1e-6));
    }

    SECTION("sliceAlongAxis axis=0 (row slice)") {
        auto row1 = storage.sliceAlongAxis(0, 1);
        REQUIRE(row1.size() == 4);
        CHECK_THAT(static_cast<double>(row1[0]), WithinAbs(5.0, 1e-6));
        CHECK_THAT(static_cast<double>(row1[1]), WithinAbs(6.0, 1e-6));
        CHECK_THAT(static_cast<double>(row1[2]), WithinAbs(7.0, 1e-6));
        CHECK_THAT(static_cast<double>(row1[3]), WithinAbs(8.0, 1e-6));
    }

    SECTION("sliceAlongAxis axis=1 (column slice)") {
        auto col2 = storage.sliceAlongAxis(1, 2);
        REQUIRE(col2.size() == 3);
        CHECK_THAT(static_cast<double>(col2[0]), WithinAbs(3.0, 1e-6));
        CHECK_THAT(static_cast<double>(col2[1]), WithinAbs(7.0, 1e-6));
        CHECK_THAT(static_cast<double>(col2[2]), WithinAbs(11.0, 1e-6));
    }

    SECTION("flatData is row-major") {
        auto flat = storage.flatData();
        REQUIRE(flat.size() == 12);
        for (int i = 0; i < 12; ++i) {
            CHECK_THAT(static_cast<double>(flat[i]),
                       WithinAbs(static_cast<double>(i + 1), 1e-6));
        }
    }
}

// =============================================================================
// 3D Tests
// =============================================================================

TEST_CASE("DenseTensorStorage 3D element access", "[DenseTensorStorage]") {
    // Shape [2, 3, 4]: 2 slices, 3 rows, 4 cols
    // Value at (s, r, c) = s*100 + r*10 + c
    std::vector<float> data;
    data.reserve(24);
    for (std::size_t s = 0; s < 2; ++s) {
        for (std::size_t r = 0; r < 3; ++r) {
            for (std::size_t c = 0; c < 4; ++c) {
                data.push_back(static_cast<float>(s * 100 + r * 10 + c));
            }
        }
    }
    DenseTensorStorage storage(data, {2, 3, 4});

    SECTION("metadata") {
        CHECK(storage.ndim() == 3);
        CHECK(storage.totalElements() == 24);

        auto sh = storage.shape();
        REQUIRE(sh.size() == 3);
        CHECK(sh[0] == 2);
        CHECK(sh[1] == 3);
        CHECK(sh[2] == 4);
    }

    SECTION("strides") {
        auto const & strides = storage.strides();
        REQUIRE(strides.size() == 3);
        CHECK(strides[0] == 12); // 3 * 4
        CHECK(strides[1] == 4);  // 4
        CHECK(strides[2] == 1);
    }

    SECTION("element access") {
        std::vector<std::size_t> idx;

        idx = {0, 0, 0};
        CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(0.0, 1e-6));

        idx = {0, 1, 2};
        CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(12.0, 1e-6));

        idx = {1, 0, 0};
        CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(100.0, 1e-6));

        idx = {1, 2, 3};
        CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(123.0, 1e-6));
    }

    SECTION("getColumn on 3D") {
        // getColumn(col) extracts along axis 1 — returns shape[0]*shape[2..] elements
        // For shape [2, 3, 4], getColumn(0) returns 2*4=8 elements? No —
        // getColumn returns totalElements/shape[1] = 24/3 = 8 elements.
        // These are all elements where the axis-1 index is `col`.
        // In row-major, for column 0: indices where (flat_idx / stride[1]) % shape[1] == 0
        // stride[1] = 4, shape[1] = 3
        // So elements at flat indices where (i/4)%3 == 0:
        // i=0..3 → (0..0)%3=0 ✓ → values 0,1,2,3
        // i=4..7 → (1)%3=1 ✗
        // i=8..11 → (2)%3=2 ✗
        // i=12..15 → (3)%3=0 ✓ → values 100,101,102,103
        // i=16..19 → (4)%3=1 ✗
        // i=20..23 → (5)%3=2 ✗
        auto col0 = storage.getColumn(0);
        REQUIRE(col0.size() == 8);
        // Slice 0, row 0: 0, 1, 2, 3
        CHECK_THAT(static_cast<double>(col0[0]), WithinAbs(0.0, 1e-6));
        CHECK_THAT(static_cast<double>(col0[1]), WithinAbs(1.0, 1e-6));
        CHECK_THAT(static_cast<double>(col0[2]), WithinAbs(2.0, 1e-6));
        CHECK_THAT(static_cast<double>(col0[3]), WithinAbs(3.0, 1e-6));
        // Slice 1, row 0: 100, 101, 102, 103
        CHECK_THAT(static_cast<double>(col0[4]), WithinAbs(100.0, 1e-6));
        CHECK_THAT(static_cast<double>(col0[5]), WithinAbs(101.0, 1e-6));
        CHECK_THAT(static_cast<double>(col0[6]), WithinAbs(102.0, 1e-6));
        CHECK_THAT(static_cast<double>(col0[7]), WithinAbs(103.0, 1e-6));
    }

    SECTION("sliceAlongAxis axis=0 (fix slice)") {
        // Fix slice=1 → shape [3, 4] = 12 elements
        auto slice1 = storage.sliceAlongAxis(0, 1);
        REQUIRE(slice1.size() == 12);
        // Row 0: 100, 101, 102, 103
        CHECK_THAT(static_cast<double>(slice1[0]), WithinAbs(100.0, 1e-6));
        CHECK_THAT(static_cast<double>(slice1[3]), WithinAbs(103.0, 1e-6));
        // Row 1: 110, 111, 112, 113
        CHECK_THAT(static_cast<double>(slice1[4]), WithinAbs(110.0, 1e-6));
        CHECK_THAT(static_cast<double>(slice1[7]), WithinAbs(113.0, 1e-6));
        // Row 2: 120, 121, 122, 123
        CHECK_THAT(static_cast<double>(slice1[8]), WithinAbs(120.0, 1e-6));
        CHECK_THAT(static_cast<double>(slice1[11]), WithinAbs(123.0, 1e-6));
    }

    SECTION("sliceAlongAxis axis=1 (fix row)") {
        // Fix row=0 → shape [2, 4] = 8 elements
        auto row0 = storage.sliceAlongAxis(1, 0);
        REQUIRE(row0.size() == 8);
        // Slice 0: 0, 1, 2, 3
        CHECK_THAT(static_cast<double>(row0[0]), WithinAbs(0.0, 1e-6));
        CHECK_THAT(static_cast<double>(row0[3]), WithinAbs(3.0, 1e-6));
        // Slice 1: 100, 101, 102, 103
        CHECK_THAT(static_cast<double>(row0[4]), WithinAbs(100.0, 1e-6));
        CHECK_THAT(static_cast<double>(row0[7]), WithinAbs(103.0, 1e-6));
    }

    SECTION("sliceAlongAxis axis=2 (fix col)") {
        // Fix col=1 → shape [2, 3] = 6 elements
        auto col1 = storage.sliceAlongAxis(2, 1);
        REQUIRE(col1.size() == 6);
        // Slice 0: rows 0,1,2 at col=1 → 1, 11, 21
        CHECK_THAT(static_cast<double>(col1[0]), WithinAbs(1.0, 1e-6));
        CHECK_THAT(static_cast<double>(col1[1]), WithinAbs(11.0, 1e-6));
        CHECK_THAT(static_cast<double>(col1[2]), WithinAbs(21.0, 1e-6));
        // Slice 1: rows 0,1,2 at col=1 → 101, 111, 121
        CHECK_THAT(static_cast<double>(col1[3]), WithinAbs(101.0, 1e-6));
        CHECK_THAT(static_cast<double>(col1[4]), WithinAbs(111.0, 1e-6));
        CHECK_THAT(static_cast<double>(col1[5]), WithinAbs(121.0, 1e-6));
    }
}

// =============================================================================
// 4D Tests (primary use case for DenseTensorStorage: >3D)
// =============================================================================

TEST_CASE("DenseTensorStorage 4D element access and slicing", "[DenseTensorStorage]") {
    // Shape [2, 3, 4, 5] — batch × channel × height × width
    // Value at (b, c, h, w) = b*1000 + c*100 + h*10 + w
    std::vector<float> data;
    data.reserve(2 * 3 * 4 * 5);
    for (std::size_t b = 0; b < 2; ++b) {
        for (std::size_t c = 0; c < 3; ++c) {
            for (std::size_t h = 0; h < 4; ++h) {
                for (std::size_t w = 0; w < 5; ++w) {
                    data.push_back(static_cast<float>(b * 1000 + c * 100 + h * 10 + w));
                }
            }
        }
    }
    DenseTensorStorage storage(data, {2, 3, 4, 5});

    SECTION("metadata") {
        CHECK(storage.ndim() == 4);
        CHECK(storage.totalElements() == 120);
    }

    SECTION("strides") {
        auto const & strides = storage.strides();
        REQUIRE(strides.size() == 4);
        CHECK(strides[0] == 60); // 3*4*5
        CHECK(strides[1] == 20); // 4*5
        CHECK(strides[2] == 5);  // 5
        CHECK(strides[3] == 1);
    }

    SECTION("element access") {
        std::vector<std::size_t> idx;

        idx = {0, 0, 0, 0};
        CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(0.0, 1e-6));

        idx = {1, 2, 3, 4};
        CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(1234.0, 1e-6));

        idx = {0, 1, 2, 3};
        CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(123.0, 1e-6));
    }

    SECTION("sliceAlongAxis axis=0 (fix batch)") {
        // Fix batch=1 → shape [3, 4, 5] = 60 elements
        auto batch1 = storage.sliceAlongAxis(0, 1);
        REQUIRE(batch1.size() == 60);
        // First element: (c=0, h=0, w=0) → value 1000
        CHECK_THAT(static_cast<double>(batch1[0]), WithinAbs(1000.0, 1e-6));
        // Last element: (c=2, h=3, w=4) → value 1234
        CHECK_THAT(static_cast<double>(batch1[59]), WithinAbs(1234.0, 1e-6));
    }

    SECTION("sliceAlongAxis axis=1 (fix channel)") {
        // Fix channel=2 → shape [2, 4, 5] = 40 elements
        auto ch2 = storage.sliceAlongAxis(1, 2);
        REQUIRE(ch2.size() == 40);
        // First element: (b=0, h=0, w=0) → 200
        CHECK_THAT(static_cast<double>(ch2[0]), WithinAbs(200.0, 1e-6));
        // (b=0, h=3, w=4) → 234
        CHECK_THAT(static_cast<double>(ch2[19]), WithinAbs(234.0, 1e-6));
        // (b=1, h=0, w=0) → 1200
        CHECK_THAT(static_cast<double>(ch2[20]), WithinAbs(1200.0, 1e-6));
    }

    SECTION("getColumn on 4D") {
        // getColumn(col) extracts along axis 1 (channel axis)
        // For shape [2, 3, 4, 5], getColumn(0) returns total/shape[1] = 120/3 = 40 elements
        auto col0 = storage.getColumn(0);
        REQUIRE(col0.size() == 40);
        // First chunk: b=0, channel=0, all h,w → values 0..19
        CHECK_THAT(static_cast<double>(col0[0]), WithinAbs(0.0, 1e-6));
        CHECK_THAT(static_cast<double>(col0[19]), WithinAbs(34.0, 1e-6)); // h=3,w=4 → 34? No wait
        // b=0,c=0,h=3,w=4 → 0*1000+0*100+3*10+4 = 34
        CHECK_THAT(static_cast<double>(col0[19]), WithinAbs(34.0, 1e-6));
        // Second chunk: b=1, channel=0, all h,w → values 1000..1034
        CHECK_THAT(static_cast<double>(col0[20]), WithinAbs(1000.0, 1e-6));
    }
}

// =============================================================================
// 5D Tests
// =============================================================================

TEST_CASE("DenseTensorStorage 5D basic operations", "[DenseTensorStorage]") {
    // Shape [2, 2, 2, 2, 2] = 32 elements
    std::vector<float> data(32);
    std::iota(data.begin(), data.end(), 0.0f);
    DenseTensorStorage storage(data, {2, 2, 2, 2, 2});

    CHECK(storage.ndim() == 5);
    CHECK(storage.totalElements() == 32);

    SECTION("strides") {
        auto const & strides = storage.strides();
        REQUIRE(strides.size() == 5);
        CHECK(strides[0] == 16);
        CHECK(strides[1] == 8);
        CHECK(strides[2] == 4);
        CHECK(strides[3] == 2);
        CHECK(strides[4] == 1);
    }

    SECTION("element access") {
        // Element at {1,0,1,1,0} → offset = 1*16 + 0*8 + 1*4 + 1*2 + 0*1 = 22
        std::vector<std::size_t> idx = {1, 0, 1, 1, 0};
        CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(22.0, 1e-6));
    }

    SECTION("round trip via flatData") {
        auto flat = storage.flatData();
        REQUIRE(flat.size() == 32);
        for (std::size_t i = 0; i < 32; ++i) {
            CHECK_THAT(static_cast<double>(flat[i]),
                       WithinAbs(static_cast<double>(i), 1e-6));
        }
    }
}

// =============================================================================
// Mutable Access Tests
// =============================================================================

TEST_CASE("DenseTensorStorage mutableFlatData", "[DenseTensorStorage]") {
    DenseTensorStorage storage({3, 4}); // 12 zeros

    auto mutable_data = storage.mutableFlatData();
    REQUIRE(mutable_data.size() == 12);

    // Fill row-major: row 0 = [1,2,3,4], row 1 = [5,6,7,8], row 2 = [9,10,11,12]
    for (std::size_t i = 0; i < 12; ++i) {
        mutable_data[i] = static_cast<float>(i + 1);
    }

    // Verify via getValueAt
    std::vector<std::size_t> idx = {1, 2}; // row 1, col 2 → flat index 6 → value 7
    CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(7.0, 1e-6));
}

TEST_CASE("DenseTensorStorage setValueAt", "[DenseTensorStorage]") {
    std::vector<float> data = {1, 2, 3, 4, 5, 6};
    DenseTensorStorage storage(data, {2, 3});

    std::vector<std::size_t> idx = {1, 1}; // should be value 5
    CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(5.0, 1e-6));

    storage.setValueAt(idx, 99.0f);
    CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(99.0, 1e-6));

    // Other values untouched
    idx = {0, 0};
    CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(1.0, 1e-6));
}

// =============================================================================
// Cache Tests
// =============================================================================

TEST_CASE("DenseTensorStorage tryGetCache", "[DenseTensorStorage]") {
    std::vector<float> data = {1, 2, 3, 4, 5, 6};
    DenseTensorStorage storage(data, {2, 3});

    auto cache = storage.tryGetCache();
    CHECK(cache.isValid());
    CHECK(cache.total_elements == 6);
    CHECK(cache.data_ptr != nullptr);

    REQUIRE(cache.shape.size() == 2);
    CHECK(cache.shape[0] == 2);
    CHECK(cache.shape[1] == 3);

    // Row-major strides
    REQUIRE(cache.strides.size() == 2);
    CHECK(cache.strides[0] == 3);
    CHECK(cache.strides[1] == 1);

    // Verify data pointer matches flatData
    auto flat = storage.flatData();
    CHECK(cache.data_ptr == flat.data());
}

TEST_CASE("DenseTensorStorage 4D cache strides", "[DenseTensorStorage]") {
    DenseTensorStorage storage({2, 3, 4, 5});

    auto cache = storage.tryGetCache();
    CHECK(cache.isValid());
    CHECK(cache.total_elements == 120);

    REQUIRE(cache.strides.size() == 4);
    CHECK(cache.strides[0] == 60);
    CHECK(cache.strides[1] == 20);
    CHECK(cache.strides[2] == 5);
    CHECK(cache.strides[3] == 1);
}

// =============================================================================
// CRTP Interface Test
// =============================================================================

TEST_CASE("DenseTensorStorage CRTP base interface accessible", "[DenseTensorStorage]") {
    std::vector<float> data(6, 1.0f);
    DenseTensorStorage storage(data, {2, 3});

    TensorStorageBase<DenseTensorStorage> const & base = storage;

    CHECK(base.totalElements() == 6);
    CHECK(base.isContiguous());
    CHECK(base.getStorageType() == TensorStorageType::Dense);

    auto s = base.shape();
    REQUIRE(s.size() == 2);
    CHECK(s[0] == 2);
    CHECK(s[1] == 3);

    auto cache = base.tryGetCache();
    CHECK(cache.isValid());
}

// =============================================================================
// Error Handling
// =============================================================================

TEST_CASE("DenseTensorStorage getValueAt error handling", "[DenseTensorStorage]") {
    std::vector<float> data(24, 0.0f);
    DenseTensorStorage storage(data, {2, 3, 4});

    SECTION("wrong number of indices") {
        std::vector<std::size_t> idx = {0, 0};
        CHECK_THROWS_AS(storage.getValueAt(idx), std::invalid_argument);

        idx = {0, 0, 0, 0};
        CHECK_THROWS_AS(storage.getValueAt(idx), std::invalid_argument);
    }

    SECTION("index out of range dim 0") {
        std::vector<std::size_t> idx = {2, 0, 0};
        CHECK_THROWS_AS(storage.getValueAt(idx), std::out_of_range);
    }

    SECTION("index out of range dim 1") {
        std::vector<std::size_t> idx = {0, 3, 0};
        CHECK_THROWS_AS(storage.getValueAt(idx), std::out_of_range);
    }

    SECTION("index out of range dim 2") {
        std::vector<std::size_t> idx = {0, 0, 4};
        CHECK_THROWS_AS(storage.getValueAt(idx), std::out_of_range);
    }
}

TEST_CASE("DenseTensorStorage setValueAt error handling", "[DenseTensorStorage]") {
    std::vector<float> data(6, 0.0f);
    DenseTensorStorage storage(data, {2, 3});

    SECTION("wrong number of indices") {
        std::size_t idx = 0;
        CHECK_THROWS_AS(storage.setValueAt({&idx, 1}, 1.0f), std::invalid_argument);
    }

    SECTION("out of range") {
        std::vector<std::size_t> idx = {2, 0};
        CHECK_THROWS_AS(storage.setValueAt(idx, 1.0f), std::out_of_range);
    }
}

TEST_CASE("DenseTensorStorage sliceAlongAxis error handling", "[DenseTensorStorage]") {
    std::vector<float> data(24, 0.0f);
    DenseTensorStorage storage(data, {2, 3, 4});

    SECTION("axis out of range") {
        CHECK_THROWS_AS(storage.sliceAlongAxis(3, 0), std::out_of_range);
    }

    SECTION("index out of range for axis 0") {
        CHECK_THROWS_AS(storage.sliceAlongAxis(0, 2), std::out_of_range);
    }

    SECTION("index out of range for axis 1") {
        CHECK_THROWS_AS(storage.sliceAlongAxis(1, 3), std::out_of_range);
    }

    SECTION("index out of range for axis 2") {
        CHECK_THROWS_AS(storage.sliceAlongAxis(2, 4), std::out_of_range);
    }
}

TEST_CASE("DenseTensorStorage getColumn error handling", "[DenseTensorStorage]") {
    SECTION("1D tensor") {
        DenseTensorStorage storage({1.0f, 2.0f}, {2});
        CHECK_THROWS_AS(storage.getColumn(0), std::invalid_argument);
    }

    SECTION("column out of range") {
        std::vector<float> data(6, 0.0f);
        DenseTensorStorage storage(data, {2, 3});
        CHECK_THROWS_AS(storage.getColumn(3), std::out_of_range);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("DenseTensorStorage single element", "[DenseTensorStorage]") {
    DenseTensorStorage storage({42.0f}, {1});

    CHECK(storage.ndim() == 1);
    CHECK(storage.totalElements() == 1);

    std::size_t idx = 0;
    CHECK_THAT(static_cast<double>(storage.getValueAt({&idx, 1})), WithinAbs(42.0, 1e-6));
}

TEST_CASE("DenseTensorStorage dimension with size 1", "[DenseTensorStorage]") {
    // Shape [1, 5] — single row, 5 columns
    std::vector<float> data = {10, 20, 30, 40, 50};
    DenseTensorStorage storage(data, {1, 5});

    CHECK(storage.ndim() == 2);
    CHECK(storage.totalElements() == 5);

    auto col2 = storage.getColumn(2);
    REQUIRE(col2.size() == 1);
    CHECK_THAT(static_cast<double>(col2[0]), WithinAbs(30.0, 1e-6));

    auto row0 = storage.sliceAlongAxis(0, 0);
    REQUIRE(row0.size() == 5);
    CHECK_THAT(static_cast<double>(row0[0]), WithinAbs(10.0, 1e-6));
    CHECK_THAT(static_cast<double>(row0[4]), WithinAbs(50.0, 1e-6));
}

TEST_CASE("DenseTensorStorage shape with zero dimension", "[DenseTensorStorage]") {
    // A dimension of size 0 is valid — total elements = 0
    std::vector<float> data;
    DenseTensorStorage storage(data, {3, 0});

    CHECK(storage.ndim() == 2);
    CHECK(storage.totalElements() == 0);
    CHECK(storage.flatData().empty());
}

TEST_CASE("DenseTensorStorage large tensor column extraction", "[DenseTensorStorage]") {
    // 100 rows × 50 columns
    std::size_t const nrows = 100;
    std::size_t const ncols = 50;
    std::vector<float> data(nrows * ncols);
    for (std::size_t r = 0; r < nrows; ++r) {
        for (std::size_t c = 0; c < ncols; ++c) {
            data[r * ncols + c] = static_cast<float>(r * 1000 + c);
        }
    }
    DenseTensorStorage storage(data, {nrows, ncols});

    auto col25 = storage.getColumn(25);
    REQUIRE(col25.size() == nrows);
    for (std::size_t r = 0; r < nrows; ++r) {
        CHECK_THAT(static_cast<double>(col25[r]),
                   WithinAbs(static_cast<double>(r * 1000 + 25), 1e-4));
    }
}

TEST_CASE("DenseTensorStorage round-trip read/write consistency", "[DenseTensorStorage]") {
    std::size_t const nrows = 10;
    std::size_t const ncols = 5;
    std::vector<float> data(nrows * ncols);
    std::iota(data.begin(), data.end(), 0.0f);

    DenseTensorStorage storage(data, {nrows, ncols});

    for (std::size_t r = 0; r < nrows; ++r) {
        for (std::size_t c = 0; c < ncols; ++c) {
            std::vector<std::size_t> idx = {r, c};
            float expected = static_cast<float>(r * ncols + c);
            CHECK_THAT(static_cast<double>(storage.getValueAt(idx)),
                       WithinAbs(static_cast<double>(expected), 1e-6));
        }
    }
}
