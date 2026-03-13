/**
 * @file ArmadilloTensorStorage.test.cpp
 * @brief Unit tests for TensorStorageBase CRTP and ArmadilloTensorStorage
 *
 * Tests cover:
 * - Construction from arma::fvec, arma::fmat, arma::fcube
 * - Construction from flat row-major data
 * - Shape, totalElements, isContiguous metadata
 * - Element access via getValueAt
 * - Column extraction via getColumn
 * - Axis slicing via sliceAlongAxis
 * - flatData access
 * - Cache support (tryGetCache)
 * - Storage type reporting
 * - Direct Armadillo access (vector/matrix/cube getters)
 * - Error handling (out-of-range, wrong dimensionality)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Tensors/storage/ArmadilloTensorStorage.hpp"

#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

using Catch::Matchers::WithinAbs;

// =============================================================================
// 1D Vector Tests
// =============================================================================

TEST_CASE("ArmadilloTensorStorage 1D construction from fvec", "[ArmadilloTensorStorage]") {
    arma::fvec v = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    ArmadilloTensorStorage storage(v);

    SECTION("metadata") {
        CHECK(storage.ndim() == 1);
        CHECK(storage.totalElements() == 5);
        CHECK(storage.isContiguous());
        CHECK(storage.getStorageType() == TensorStorageType::Armadillo);

        auto s = storage.shape();
        REQUIRE(s.size() == 1);
        CHECK(s[0] == 5);
    }

    SECTION("element access") {
        std::size_t idx;

        idx = 0;
        CHECK_THAT(storage.getValueAt({&idx, 1}), WithinAbs(1.0f, 1e-6f));

        idx = 2;
        CHECK_THAT(storage.getValueAt({&idx, 1}), WithinAbs(3.0f, 1e-6f));

        idx = 4;
        CHECK_THAT(storage.getValueAt({&idx, 1}), WithinAbs(5.0f, 1e-6f));
    }

    SECTION("flatData") {
        auto flat = storage.flatData();
        REQUIRE(flat.size() == 5);
        CHECK_THAT(static_cast<double>(flat[0]), WithinAbs(1.0, 1e-6));
        CHECK_THAT(static_cast<double>(flat[4]), WithinAbs(5.0, 1e-6));
    }

    SECTION("sliceAlongAxis on 1D") {
        auto slice = storage.sliceAlongAxis(0, 2);
        REQUIRE(slice.size() == 1);
        CHECK_THAT(static_cast<double>(slice[0]), WithinAbs(3.0, 1e-6));
    }

    SECTION("getColumn throws for 1D") {
        CHECK_THROWS_AS(storage.getColumn(0), std::invalid_argument);
    }

    SECTION("direct Armadillo access") {
        auto const & vec = storage.vector();
        CHECK(vec.n_elem == 5);
        CHECK_THAT(static_cast<double>(vec(0)), WithinAbs(1.0, 1e-6));

        CHECK_THROWS_AS(storage.matrix(), std::logic_error);
        CHECK_THROWS_AS(storage.cube(), std::logic_error);
    }

    SECTION("cache") {
        auto cache = storage.tryGetCache();
        CHECK(cache.isValid());
        CHECK(cache.total_elements == 5);
        CHECK(cache.data_ptr != nullptr);
        REQUIRE(cache.shape.size() == 1);
        CHECK(cache.shape[0] == 5);
        REQUIRE(cache.strides.size() == 1);
        CHECK(cache.strides[0] == 1);
    }
}

TEST_CASE("ArmadilloTensorStorage 1D error handling", "[ArmadilloTensorStorage]") {
    arma::fvec v = {10.0f, 20.0f, 30.0f};
    ArmadilloTensorStorage storage(v);

    SECTION("wrong number of indices") {
        std::vector<std::size_t> indices = {0, 1};
        CHECK_THROWS_AS(
            storage.getValueAt(indices),
            std::invalid_argument);
    }

    SECTION("index out of range") {
        std::size_t idx = 3;
        CHECK_THROWS_AS(
            storage.getValueAt({&idx, 1}),
            std::out_of_range);
    }

    SECTION("sliceAlongAxis out of range axis") {
        CHECK_THROWS_AS(storage.sliceAlongAxis(1, 0), std::out_of_range);
    }

    SECTION("sliceAlongAxis out of range index") {
        CHECK_THROWS_AS(storage.sliceAlongAxis(0, 3), std::out_of_range);
    }
}

// =============================================================================
// 2D Matrix Tests
// =============================================================================

TEST_CASE("ArmadilloTensorStorage 2D construction from fmat", "[ArmadilloTensorStorage]") {
    // Create a 3x4 matrix:
    // Row 0: 1, 2, 3, 4
    // Row 1: 5, 6, 7, 8
    // Row 2: 9, 10, 11, 12
    arma::fmat m(3, 4);
    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 4; ++c) {
            m(r, c) = static_cast<float>(r * 4 + c + 1);
        }
    }
    ArmadilloTensorStorage storage(m);

    SECTION("metadata") {
        CHECK(storage.ndim() == 2);
        CHECK(storage.totalElements() == 12);
        CHECK(storage.isContiguous());
        CHECK(storage.getStorageType() == TensorStorageType::Armadillo);

        auto s = storage.shape();
        REQUIRE(s.size() == 2);
        CHECK(s[0] == 3);
        CHECK(s[1] == 4);
    }

    SECTION("element access (row-major semantics)") {
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

    SECTION("direct Armadillo access") {
        auto const & mat = storage.matrix();
        CHECK(mat.n_rows == 3);
        CHECK(mat.n_cols == 4);

        CHECK_THROWS_AS(storage.vector(), std::logic_error);
        CHECK_THROWS_AS(storage.cube(), std::logic_error);
    }

    SECTION("mutable Armadillo access") {
        auto & mat = storage.mutableMatrix();
        mat(0, 0) = 99.0f;
        std::vector<std::size_t> idx = {0, 0};
        CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(99.0, 1e-6));
    }

    SECTION("cache") {
        auto cache = storage.tryGetCache();
        CHECK(cache.isValid());
        CHECK(cache.total_elements == 12);
        REQUIRE(cache.shape.size() == 2);
        CHECK(cache.shape[0] == 3);
        CHECK(cache.shape[1] == 4);
        // Column-major strides: row stride=1, col stride=n_rows=3
        REQUIRE(cache.strides.size() == 2);
        CHECK(cache.strides[0] == 1);
        CHECK(cache.strides[1] == 3);
    }
}

TEST_CASE("ArmadilloTensorStorage 2D construction from row-major data", "[ArmadilloTensorStorage]") {
    // Row-major: row0=[1,2,3], row1=[4,5,6]
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    ArmadilloTensorStorage storage(data, 2, 3);

    CHECK(storage.ndim() == 2);

    auto s = storage.shape();
    REQUIRE(s.size() == 2);
    CHECK(s[0] == 2);
    CHECK(s[1] == 3);

    // Verify row-major semantics
    std::vector<std::size_t> idx;

    idx = {0, 0};
    CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(1.0, 1e-6));

    idx = {0, 2};
    CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(3.0, 1e-6));

    idx = {1, 0};
    CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(4.0, 1e-6));

    idx = {1, 2};
    CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(6.0, 1e-6));
}

TEST_CASE("ArmadilloTensorStorage 2D row-major constructor validates size", "[ArmadilloTensorStorage]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f};
    CHECK_THROWS_AS(
        ArmadilloTensorStorage(data, 2, 3),
        std::invalid_argument);
}

TEST_CASE("ArmadilloTensorStorage 2D error handling", "[ArmadilloTensorStorage]") {
    arma::fmat m(3, 4, arma::fill::zeros);
    ArmadilloTensorStorage storage(m);

    SECTION("wrong index count") {
        std::vector<std::size_t> idx = {0};
        CHECK_THROWS_AS(storage.getValueAt(idx), std::invalid_argument);

        idx = {0, 0, 0};
        CHECK_THROWS_AS(storage.getValueAt(idx), std::invalid_argument);
    }

    SECTION("row index out of range") {
        std::vector<std::size_t> idx = {3, 0};
        CHECK_THROWS_AS(storage.getValueAt(idx), std::out_of_range);
    }

    SECTION("col index out of range") {
        std::vector<std::size_t> idx = {0, 4};
        CHECK_THROWS_AS(storage.getValueAt(idx), std::out_of_range);
    }

    SECTION("getColumn out of range") {
        CHECK_THROWS_AS(storage.getColumn(4), std::out_of_range);
    }

    SECTION("sliceAlongAxis out of range axis") {
        CHECK_THROWS_AS(storage.sliceAlongAxis(2, 0), std::out_of_range);
    }

    SECTION("sliceAlongAxis axis=0 out of range index") {
        CHECK_THROWS_AS(storage.sliceAlongAxis(0, 3), std::out_of_range);
    }

    SECTION("sliceAlongAxis axis=1 out of range index") {
        CHECK_THROWS_AS(storage.sliceAlongAxis(1, 4), std::out_of_range);
    }
}

// =============================================================================
// 3D Cube Tests
// =============================================================================

TEST_CASE("ArmadilloTensorStorage 3D construction from fcube", "[ArmadilloTensorStorage]") {
    // Create a cube: shape [2 slices, 3 rows, 4 cols]
    // Our public shape = [n_slices=2, n_rows=3, n_cols=4]
    arma::fcube c(3, 4, 2);
    // Fill: value = slice*100 + row*10 + col
    for (std::size_t s = 0; s < 2; ++s) {
        for (std::size_t r = 0; r < 3; ++r) {
            for (std::size_t col = 0; col < 4; ++col) {
                c(r, col, s) = static_cast<float>(s * 100 + r * 10 + col);
            }
        }
    }
    ArmadilloTensorStorage storage(c);

    SECTION("metadata") {
        CHECK(storage.ndim() == 3);
        CHECK(storage.totalElements() == 24);
        CHECK(storage.isContiguous());
        CHECK(storage.getStorageType() == TensorStorageType::Armadillo);

        auto s = storage.shape();
        REQUIRE(s.size() == 3);
        CHECK(s[0] == 2); // n_slices
        CHECK(s[1] == 3); // n_rows
        CHECK(s[2] == 4); // n_cols
    }

    SECTION("element access") {
        // getValueAt({slice, row, col})
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
        // getColumn(col) returns [n_slices * n_rows] values
        // For col=0: values for (s=0,r=0), (s=0,r=1), (s=0,r=2), (s=1,r=0), (s=1,r=1), (s=1,r=2)
        auto col0 = storage.getColumn(0);
        REQUIRE(col0.size() == 6); // 2 slices * 3 rows
        CHECK_THAT(static_cast<double>(col0[0]), WithinAbs(0.0, 1e-6));   // s=0, r=0, c=0
        CHECK_THAT(static_cast<double>(col0[1]), WithinAbs(10.0, 1e-6));  // s=0, r=1, c=0
        CHECK_THAT(static_cast<double>(col0[2]), WithinAbs(20.0, 1e-6));  // s=0, r=2, c=0
        CHECK_THAT(static_cast<double>(col0[3]), WithinAbs(100.0, 1e-6)); // s=1, r=0, c=0
        CHECK_THAT(static_cast<double>(col0[4]), WithinAbs(110.0, 1e-6)); // s=1, r=1, c=0
        CHECK_THAT(static_cast<double>(col0[5]), WithinAbs(120.0, 1e-6)); // s=1, r=2, c=0
    }

    SECTION("sliceAlongAxis axis=0 (fix slice)") {
        // Fix slice=1 → [3 rows, 4 cols] in row-major
        auto slice1 = storage.sliceAlongAxis(0, 1);
        REQUIRE(slice1.size() == 12); // 3 * 4
        // Row 0: 100, 101, 102, 103
        CHECK_THAT(static_cast<double>(slice1[0]), WithinAbs(100.0, 1e-6));
        CHECK_THAT(static_cast<double>(slice1[1]), WithinAbs(101.0, 1e-6));
        CHECK_THAT(static_cast<double>(slice1[2]), WithinAbs(102.0, 1e-6));
        CHECK_THAT(static_cast<double>(slice1[3]), WithinAbs(103.0, 1e-6));
        // Row 1: 110, 111, 112, 113
        CHECK_THAT(static_cast<double>(slice1[4]), WithinAbs(110.0, 1e-6));
        CHECK_THAT(static_cast<double>(slice1[7]), WithinAbs(113.0, 1e-6));
        // Row 2: 120, 121, 122, 123
        CHECK_THAT(static_cast<double>(slice1[8]), WithinAbs(120.0, 1e-6));
        CHECK_THAT(static_cast<double>(slice1[11]), WithinAbs(123.0, 1e-6));
    }

    SECTION("sliceAlongAxis axis=1 (fix row)") {
        // Fix row=0 → [2 slices, 4 cols] in row-major
        auto row0 = storage.sliceAlongAxis(1, 0);
        REQUIRE(row0.size() == 8); // 2 * 4
        // Slice 0: 0, 1, 2, 3
        CHECK_THAT(static_cast<double>(row0[0]), WithinAbs(0.0, 1e-6));
        CHECK_THAT(static_cast<double>(row0[3]), WithinAbs(3.0, 1e-6));
        // Slice 1: 100, 101, 102, 103
        CHECK_THAT(static_cast<double>(row0[4]), WithinAbs(100.0, 1e-6));
        CHECK_THAT(static_cast<double>(row0[7]), WithinAbs(103.0, 1e-6));
    }

    SECTION("sliceAlongAxis axis=2 (fix col)") {
        // Fix col=1 → [2 slices, 3 rows] in row-major
        auto col1 = storage.sliceAlongAxis(2, 1);
        REQUIRE(col1.size() == 6); // 2 * 3
        // Slice 0: rows 0,1,2 at col=1 → 1, 11, 21
        CHECK_THAT(static_cast<double>(col1[0]), WithinAbs(1.0, 1e-6));
        CHECK_THAT(static_cast<double>(col1[1]), WithinAbs(11.0, 1e-6));
        CHECK_THAT(static_cast<double>(col1[2]), WithinAbs(21.0, 1e-6));
        // Slice 1: rows 0,1,2 at col=1 → 101, 111, 121
        CHECK_THAT(static_cast<double>(col1[3]), WithinAbs(101.0, 1e-6));
        CHECK_THAT(static_cast<double>(col1[4]), WithinAbs(111.0, 1e-6));
        CHECK_THAT(static_cast<double>(col1[5]), WithinAbs(121.0, 1e-6));
    }

    SECTION("direct Armadillo access") {
        auto const & cb = storage.cube();
        CHECK(cb.n_rows == 3);
        CHECK(cb.n_cols == 4);
        CHECK(cb.n_slices == 2);

        CHECK_THROWS_AS(storage.vector(), std::logic_error);
        CHECK_THROWS_AS(storage.matrix(), std::logic_error);
    }

    SECTION("cache") {
        auto cache = storage.tryGetCache();
        CHECK(cache.isValid());
        CHECK(cache.total_elements == 24);
        REQUIRE(cache.shape.size() == 3);
        CHECK(cache.shape[0] == 2);
        CHECK(cache.shape[1] == 3);
        CHECK(cache.shape[2] == 4);
        // Column-major cube strides
        REQUIRE(cache.strides.size() == 3);
        CHECK(cache.strides[0] == 12); // n_rows * n_cols = 3*4
        CHECK(cache.strides[1] == 1);  // within slice, row stride=1
        CHECK(cache.strides[2] == 3);  // within slice, col stride=n_rows=3
    }
}

TEST_CASE("ArmadilloTensorStorage 3D error handling", "[ArmadilloTensorStorage]") {
    arma::fcube c(3, 4, 2, arma::fill::zeros);
    ArmadilloTensorStorage storage(c);

    SECTION("wrong index count") {
        std::vector<std::size_t> idx = {0, 0};
        CHECK_THROWS_AS(storage.getValueAt(idx), std::invalid_argument);
    }

    SECTION("slice index out of range") {
        std::vector<std::size_t> idx = {2, 0, 0};
        CHECK_THROWS_AS(storage.getValueAt(idx), std::out_of_range);
    }

    SECTION("row index out of range") {
        std::vector<std::size_t> idx = {0, 3, 0};
        CHECK_THROWS_AS(storage.getValueAt(idx), std::out_of_range);
    }

    SECTION("col index out of range") {
        std::vector<std::size_t> idx = {0, 0, 4};
        CHECK_THROWS_AS(storage.getValueAt(idx), std::out_of_range);
    }

    SECTION("getColumn out of range") {
        CHECK_THROWS_AS(storage.getColumn(4), std::out_of_range);
    }

    SECTION("sliceAlongAxis out of range axis") {
        CHECK_THROWS_AS(storage.sliceAlongAxis(3, 0), std::out_of_range);
    }

    SECTION("sliceAlongAxis axis=0 out of range") {
        CHECK_THROWS_AS(storage.sliceAlongAxis(0, 2), std::out_of_range);
    }

    SECTION("sliceAlongAxis axis=1 out of range") {
        CHECK_THROWS_AS(storage.sliceAlongAxis(1, 3), std::out_of_range);
    }

    SECTION("sliceAlongAxis axis=2 out of range") {
        CHECK_THROWS_AS(storage.sliceAlongAxis(2, 4), std::out_of_range);
    }
}

// =============================================================================
// CRTP Interface Tests (compile-time verification)
// =============================================================================

TEST_CASE("ArmadilloTensorStorage CRTP base interface accessible", "[ArmadilloTensorStorage]") {
    arma::fmat m(2, 3, arma::fill::ones);
    ArmadilloTensorStorage storage(m);

    // Ensure CRTP base methods are accessible through the derived class
    // These calls go through TensorStorageBase<ArmadilloTensorStorage> dispatch
    TensorStorageBase<ArmadilloTensorStorage> const & base = storage;

    CHECK(base.totalElements() == 6);
    CHECK(base.isContiguous());
    CHECK(base.getStorageType() == TensorStorageType::Armadillo);

    auto s = base.shape();
    REQUIRE(s.size() == 2);
    CHECK(s[0] == 2);
    CHECK(s[1] == 3);

    auto cache = base.tryGetCache();
    CHECK(cache.isValid());
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("ArmadilloTensorStorage empty vector", "[ArmadilloTensorStorage]") {
    arma::fvec v;  // empty vector
    ArmadilloTensorStorage storage(v);

    CHECK(storage.ndim() == 1);
    CHECK(storage.totalElements() == 0);

    auto s = storage.shape();
    REQUIRE(s.size() == 1);
    CHECK(s[0] == 0);

    auto flat = storage.flatData();
    CHECK(flat.empty());
}

TEST_CASE("ArmadilloTensorStorage single element", "[ArmadilloTensorStorage]") {
    arma::fvec v = {42.0f};
    ArmadilloTensorStorage storage(v);

    CHECK(storage.totalElements() == 1);
    std::size_t idx = 0;
    CHECK_THAT(static_cast<double>(storage.getValueAt({&idx, 1})), WithinAbs(42.0, 1e-6));
}

TEST_CASE("ArmadilloTensorStorage 1x1 matrix", "[ArmadilloTensorStorage]") {
    arma::fmat m(1, 1);
    m(0, 0) = 7.0f;
    ArmadilloTensorStorage storage(m);

    CHECK(storage.ndim() == 2);
    CHECK(storage.totalElements() == 1);

    std::vector<std::size_t> idx = {0, 0};
    CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(7.0, 1e-6));

    auto col = storage.getColumn(0);
    REQUIRE(col.size() == 1);
    CHECK_THAT(static_cast<double>(col[0]), WithinAbs(7.0, 1e-6));
}

TEST_CASE("ArmadilloTensorStorage 1x1x1 cube", "[ArmadilloTensorStorage]") {
    arma::fcube c(1, 1, 1);
    c(0, 0, 0) = 3.14f;
    ArmadilloTensorStorage storage(c);

    CHECK(storage.ndim() == 3);
    CHECK(storage.totalElements() == 1);

    std::vector<std::size_t> idx = {0, 0, 0};
    CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(3.14, 1e-4));
}

TEST_CASE("ArmadilloTensorStorage large matrix column extraction", "[ArmadilloTensorStorage]") {
    // Verify column extraction is correct for a non-trivial case
    std::size_t const nrows = 100;
    std::size_t const ncols = 50;
    arma::fmat m(nrows, ncols);
    for (std::size_t r = 0; r < nrows; ++r) {
        for (std::size_t c = 0; c < ncols; ++c) {
            m(r, c) = static_cast<float>(r * 1000 + c);
        }
    }
    ArmadilloTensorStorage storage(m);

    auto col25 = storage.getColumn(25);
    REQUIRE(col25.size() == nrows);
    for (std::size_t r = 0; r < nrows; ++r) {
        CHECK_THAT(static_cast<double>(col25[r]),
                   WithinAbs(static_cast<double>(r * 1000 + 25), 1e-4));
    }
}

TEST_CASE("ArmadilloTensorStorage row-major constructor preserves data", "[ArmadilloTensorStorage]") {
    // Verify round-trip: create from row-major, read back via getValueAt
    std::vector<float> data;
    std::size_t const nrows = 10;
    std::size_t const ncols = 5;
    data.reserve(nrows * ncols);
    for (std::size_t i = 0; i < nrows * ncols; ++i) {
        data.push_back(static_cast<float>(i));
    }

    ArmadilloTensorStorage storage(data, nrows, ncols);

    for (std::size_t r = 0; r < nrows; ++r) {
        for (std::size_t c = 0; c < ncols; ++c) {
            std::vector<std::size_t> idx = {r, c};
            float expected = static_cast<float>(r * ncols + c);
            CHECK_THAT(static_cast<double>(storage.getValueAt(idx)),
                       WithinAbs(static_cast<double>(expected), 1e-6));
        }
    }
}

TEST_CASE("ArmadilloTensorStorage mutable access reflects in reads", "[ArmadilloTensorStorage]") {
    arma::fcube c(2, 3, 2, arma::fill::zeros);
    ArmadilloTensorStorage storage(c);

    // Mutate through cube accessor
    auto & mut_cube = storage.mutableCube();
    mut_cube(1, 2, 0) = 999.0f; // arma(row=1, col=2, slice=0)

    // Read back through getValueAt: {slice=0, row=1, col=2}
    std::vector<std::size_t> idx = {0, 1, 2};
    CHECK_THAT(static_cast<double>(storage.getValueAt(idx)), WithinAbs(999.0, 1e-6));
}
