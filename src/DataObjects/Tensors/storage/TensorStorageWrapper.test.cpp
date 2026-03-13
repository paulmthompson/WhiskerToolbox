/**
 * @file TensorStorageWrapper.test.cpp
 * @brief Unit tests for TensorStorageWrapper (Sean Parent type-erasure layer)
 *
 * Tests cover:
 * - Construction from ArmadilloTensorStorage and DenseTensorStorage
 * - Default construction (null/invalid state)
 * - Copy semantics (shared ownership)
 * - Move semantics
 * - Delegated interface (getValueAt, flatData, sliceAlongAxis, getColumn, etc.)
 * - Metadata delegation (shape, totalElements, isContiguous, getStorageType)
 * - Cache delegation (tryGetCache)
 * - Type recovery via tryGetAs<T> and tryGetMutableAs<T>
 * - Error handling (null wrapper throws)
 * - isValid / sharedStorage
 * - Polymorphic usage (same wrapper type for different backends)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Tensors/storage/TensorStorageWrapper.hpp"

#include "Tensors/storage/ArmadilloTensorStorage.hpp"
#include "Tensors/storage/DenseTensorStorage.hpp"

#include <cstddef>
#include <stdexcept>
#include <vector>

using Catch::Matchers::WithinAbs;

// =============================================================================
// Construction Tests
// =============================================================================

TEST_CASE("TensorStorageWrapper default construction is null", "[TensorStorageWrapper]") {
    TensorStorageWrapper wrapper;

    CHECK_FALSE(wrapper.isValid());
    CHECK(wrapper.sharedStorage() == nullptr);
}

TEST_CASE("TensorStorageWrapper from ArmadilloTensorStorage 1D", "[TensorStorageWrapper]") {
    arma::fvec v = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    TensorStorageWrapper wrapper{ArmadilloTensorStorage{v}};

    CHECK(wrapper.isValid());
    CHECK(wrapper.getStorageType() == TensorStorageType::Armadillo);
    CHECK(wrapper.totalElements() == 5);
    CHECK(wrapper.isContiguous());

    auto s = wrapper.shape();
    REQUIRE(s.size() == 1);
    CHECK(s[0] == 5);
}

TEST_CASE("TensorStorageWrapper from ArmadilloTensorStorage 2D", "[TensorStorageWrapper]") {
    // 3x4 matrix: value = row * 4 + col + 1
    arma::fmat m(3, 4);
    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 4; ++c) {
            m(r, c) = static_cast<float>(r * 4 + c + 1);
        }
    }
    TensorStorageWrapper wrapper{ArmadilloTensorStorage{m}};

    CHECK(wrapper.isValid());
    CHECK(wrapper.getStorageType() == TensorStorageType::Armadillo);
    CHECK(wrapper.totalElements() == 12);

    auto s = wrapper.shape();
    REQUIRE(s.size() == 2);
    CHECK(s[0] == 3);
    CHECK(s[1] == 4);
}

TEST_CASE("TensorStorageWrapper from DenseTensorStorage 2D", "[TensorStorageWrapper]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    std::vector<std::size_t> shape = {2, 3};
    TensorStorageWrapper wrapper(DenseTensorStorage(data, shape));

    CHECK(wrapper.isValid());
    CHECK(wrapper.getStorageType() == TensorStorageType::Dense);
    CHECK(wrapper.totalElements() == 6);
    CHECK(wrapper.isContiguous());

    auto s = wrapper.shape();
    REQUIRE(s.size() == 2);
    CHECK(s[0] == 2);
    CHECK(s[1] == 3);
}

TEST_CASE("TensorStorageWrapper from DenseTensorStorage 4D", "[TensorStorageWrapper]") {
    // 2x3x4x5 tensor
    std::vector<std::size_t> shape = {2, 3, 4, 5};
    std::size_t total = 2 * 3 * 4 * 5;
    std::vector<float> data(total);
    for (std::size_t i = 0; i < total; ++i) {
        data[i] = static_cast<float>(i);
    }
    TensorStorageWrapper wrapper(DenseTensorStorage(data, shape));

    CHECK(wrapper.isValid());
    CHECK(wrapper.getStorageType() == TensorStorageType::Dense);
    CHECK(wrapper.totalElements() == total);

    auto s = wrapper.shape();
    REQUIRE(s.size() == 4);
    CHECK(s[0] == 2);
    CHECK(s[1] == 3);
    CHECK(s[2] == 4);
    CHECK(s[3] == 5);
}

// =============================================================================
// Element Access Tests
// =============================================================================

TEST_CASE("TensorStorageWrapper getValueAt delegates to Armadillo", "[TensorStorageWrapper]") {
    arma::fmat m(3, 4);
    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 4; ++c) {
            m(r, c) = static_cast<float>(r * 4 + c + 1);
        }
    }
    TensorStorageWrapper wrapper{ArmadilloTensorStorage{m}};

    std::vector<std::size_t> idx;

    idx = {0, 0};
    CHECK_THAT(static_cast<double>(wrapper.getValueAt(idx)), WithinAbs(1.0, 1e-6));

    idx = {0, 3};
    CHECK_THAT(static_cast<double>(wrapper.getValueAt(idx)), WithinAbs(4.0, 1e-6));

    idx = {1, 0};
    CHECK_THAT(static_cast<double>(wrapper.getValueAt(idx)), WithinAbs(5.0, 1e-6));

    idx = {2, 3};
    CHECK_THAT(static_cast<double>(wrapper.getValueAt(idx)), WithinAbs(12.0, 1e-6));
}

TEST_CASE("TensorStorageWrapper getValueAt delegates to Dense", "[TensorStorageWrapper]") {
    // 2x3 row-major: [[1,2,3],[4,5,6]]
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    std::vector<std::size_t> shape = {2, 3};
    TensorStorageWrapper wrapper(DenseTensorStorage(data, shape));

    std::vector<std::size_t> idx;

    idx = {0, 0};
    CHECK_THAT(static_cast<double>(wrapper.getValueAt(idx)), WithinAbs(1.0, 1e-6));

    idx = {0, 2};
    CHECK_THAT(static_cast<double>(wrapper.getValueAt(idx)), WithinAbs(3.0, 1e-6));

    idx = {1, 0};
    CHECK_THAT(static_cast<double>(wrapper.getValueAt(idx)), WithinAbs(4.0, 1e-6));

    idx = {1, 2};
    CHECK_THAT(static_cast<double>(wrapper.getValueAt(idx)), WithinAbs(6.0, 1e-6));
}

// =============================================================================
// Bulk Access Tests
// =============================================================================

TEST_CASE("TensorStorageWrapper flatData delegates correctly", "[TensorStorageWrapper]") {
    std::vector<float> data = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f};
    std::vector<std::size_t> shape = {2, 3};
    TensorStorageWrapper wrapper(DenseTensorStorage(data, shape));

    auto flat = wrapper.flatData();
    REQUIRE(flat.size() == 6);
    CHECK_THAT(static_cast<double>(flat[0]), WithinAbs(10.0, 1e-6));
    CHECK_THAT(static_cast<double>(flat[5]), WithinAbs(60.0, 1e-6));
}

TEST_CASE("TensorStorageWrapper getColumn delegates correctly", "[TensorStorageWrapper]") {
    arma::fmat m(3, 4);
    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 4; ++c) {
            m(r, c) = static_cast<float>(r * 4 + c + 1);
        }
    }
    TensorStorageWrapper wrapper{ArmadilloTensorStorage{m}};

    auto col0 = wrapper.getColumn(0);
    REQUIRE(col0.size() == 3);
    CHECK_THAT(static_cast<double>(col0[0]), WithinAbs(1.0, 1e-6));
    CHECK_THAT(static_cast<double>(col0[1]), WithinAbs(5.0, 1e-6));
    CHECK_THAT(static_cast<double>(col0[2]), WithinAbs(9.0, 1e-6));

    auto col3 = wrapper.getColumn(3);
    REQUIRE(col3.size() == 3);
    CHECK_THAT(static_cast<double>(col3[0]), WithinAbs(4.0, 1e-6));
    CHECK_THAT(static_cast<double>(col3[1]), WithinAbs(8.0, 1e-6));
    CHECK_THAT(static_cast<double>(col3[2]), WithinAbs(12.0, 1e-6));
}

TEST_CASE("TensorStorageWrapper sliceAlongAxis delegates correctly", "[TensorStorageWrapper]") {
    arma::fmat m(3, 4);
    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 4; ++c) {
            m(r, c) = static_cast<float>(r * 4 + c + 1);
        }
    }
    TensorStorageWrapper wrapper{ArmadilloTensorStorage{m}};

    SECTION("row slice (axis=0)") {
        auto row1 = wrapper.sliceAlongAxis(0, 1);
        REQUIRE(row1.size() == 4);
        CHECK_THAT(static_cast<double>(row1[0]), WithinAbs(5.0, 1e-6));
        CHECK_THAT(static_cast<double>(row1[1]), WithinAbs(6.0, 1e-6));
        CHECK_THAT(static_cast<double>(row1[2]), WithinAbs(7.0, 1e-6));
        CHECK_THAT(static_cast<double>(row1[3]), WithinAbs(8.0, 1e-6));
    }

    SECTION("column slice (axis=1)") {
        auto col2 = wrapper.sliceAlongAxis(1, 2);
        REQUIRE(col2.size() == 3);
        CHECK_THAT(static_cast<double>(col2[0]), WithinAbs(3.0, 1e-6));
        CHECK_THAT(static_cast<double>(col2[1]), WithinAbs(7.0, 1e-6));
        CHECK_THAT(static_cast<double>(col2[2]), WithinAbs(11.0, 1e-6));
    }
}

// =============================================================================
// Cache Tests
// =============================================================================

TEST_CASE("TensorStorageWrapper tryGetCache from Armadillo", "[TensorStorageWrapper]") {
    arma::fmat m(3, 4, arma::fill::ones);
    TensorStorageWrapper wrapper{ArmadilloTensorStorage{m}};

    auto cache = wrapper.tryGetCache();
    CHECK(cache.isValid());
    CHECK(cache.total_elements == 12);
    CHECK(cache.data_ptr != nullptr);
    REQUIRE(cache.shape.size() == 2);
    CHECK(cache.shape[0] == 3);
    CHECK(cache.shape[1] == 4);
}

TEST_CASE("TensorStorageWrapper tryGetCache from Dense", "[TensorStorageWrapper]") {
    std::vector<float> data(24, 1.0f);
    std::vector<std::size_t> shape = {2, 3, 4};
    TensorStorageWrapper wrapper(DenseTensorStorage(data, shape));

    auto cache = wrapper.tryGetCache();
    CHECK(cache.isValid());
    CHECK(cache.total_elements == 24);
    CHECK(cache.data_ptr != nullptr);
    REQUIRE(cache.shape.size() == 3);
    CHECK(cache.shape[0] == 2);
    CHECK(cache.shape[1] == 3);
    CHECK(cache.shape[2] == 4);
    // Dense uses row-major strides
    REQUIRE(cache.strides.size() == 3);
    CHECK(cache.strides[0] == 12); // 3*4
    CHECK(cache.strides[1] == 4);  // 4
    CHECK(cache.strides[2] == 1);
}

// =============================================================================
// Type Recovery Tests (tryGetAs / tryGetMutableAs)
// =============================================================================

TEST_CASE("TensorStorageWrapper tryGetAs recovers Armadillo", "[TensorStorageWrapper]") {
    arma::fmat m(3, 4);
    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 4; ++c) {
            m(r, c) = static_cast<float>(r * 4 + c + 1);
        }
    }
    TensorStorageWrapper wrapper{ArmadilloTensorStorage{m}};

    auto const * arma_storage = wrapper.tryGetAs<ArmadilloTensorStorage>();
    REQUIRE(arma_storage != nullptr);

    auto const & mat = arma_storage->matrix();
    CHECK(mat.n_rows == 3);
    CHECK(mat.n_cols == 4);
    CHECK_THAT(static_cast<double>(mat(0, 0)), WithinAbs(1.0, 1e-6));
    CHECK_THAT(static_cast<double>(mat(2, 3)), WithinAbs(12.0, 1e-6));

    // Wrong type returns nullptr
    auto const * dense_storage = wrapper.tryGetAs<DenseTensorStorage>();
    CHECK(dense_storage == nullptr);
}

TEST_CASE("TensorStorageWrapper tryGetAs recovers Dense", "[TensorStorageWrapper]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<std::size_t> shape = {2, 2};
    TensorStorageWrapper wrapper(DenseTensorStorage(data, shape));

    auto const * dense_storage = wrapper.tryGetAs<DenseTensorStorage>();
    REQUIRE(dense_storage != nullptr);
    CHECK(dense_storage->ndim() == 2);

    auto flat = dense_storage->flatData();
    REQUIRE(flat.size() == 4);
    CHECK_THAT(static_cast<double>(flat[0]), WithinAbs(1.0, 1e-6));
    CHECK_THAT(static_cast<double>(flat[3]), WithinAbs(4.0, 1e-6));

    // Wrong type returns nullptr
    auto const * arma_storage = wrapper.tryGetAs<ArmadilloTensorStorage>();
    CHECK(arma_storage == nullptr);
}

TEST_CASE("TensorStorageWrapper tryGetMutableAs allows mutation", "[TensorStorageWrapper]") {
    arma::fmat m(2, 3, arma::fill::zeros);
    TensorStorageWrapper wrapper{ArmadilloTensorStorage{m}};

    auto * arma_storage = wrapper.tryGetMutableAs<ArmadilloTensorStorage>();
    REQUIRE(arma_storage != nullptr);

    arma_storage->mutableMatrix()(0, 0) = 42.0f;

    // Verify mutation is visible through the wrapper
    std::vector<std::size_t> idx = {0, 0};
    CHECK_THAT(static_cast<double>(wrapper.getValueAt(idx)), WithinAbs(42.0, 1e-6));
}

TEST_CASE("TensorStorageWrapper tryGetAs on null returns nullptr", "[TensorStorageWrapper]") {
    TensorStorageWrapper wrapper;

    CHECK(wrapper.tryGetAs<ArmadilloTensorStorage>() == nullptr);
    CHECK(wrapper.tryGetAs<DenseTensorStorage>() == nullptr);
    CHECK(wrapper.tryGetMutableAs<ArmadilloTensorStorage>() == nullptr);
}

// =============================================================================
// Copy Semantics Tests (shared ownership)
// =============================================================================

TEST_CASE("TensorStorageWrapper copy shares storage", "[TensorStorageWrapper]") {
    arma::fmat m(2, 3, arma::fill::ones);
    TensorStorageWrapper original{ArmadilloTensorStorage{m}};

    // Copy
    TensorStorageWrapper copy = original; // NOLINT(performance-unnecessary-copy-initialization)

    CHECK(copy.isValid());
    CHECK(copy.getStorageType() == TensorStorageType::Armadillo);
    CHECK(copy.totalElements() == 6);

    // Both share the same underlying storage
    CHECK(copy.sharedStorage() == original.sharedStorage());

    // Mutation through one is visible in the other
    auto * mut = original.tryGetMutableAs<ArmadilloTensorStorage>();
    REQUIRE(mut != nullptr);
    mut->mutableMatrix()(0, 0) = 99.0f;

    std::vector<std::size_t> idx = {0, 0};
    CHECK_THAT(static_cast<double>(copy.getValueAt(idx)), WithinAbs(99.0, 1e-6));
}

TEST_CASE("TensorStorageWrapper copy assignment works", "[TensorStorageWrapper]") {
    arma::fvec v1 = {1.0f, 2.0f};
    arma::fvec v2 = {10.0f, 20.0f, 30.0f};

    TensorStorageWrapper w1{ArmadilloTensorStorage{v1}};
    TensorStorageWrapper w2{ArmadilloTensorStorage{v2}};

    CHECK(w1.totalElements() == 2);
    CHECK(w2.totalElements() == 3);

    w1 = w2;
    CHECK(w1.totalElements() == 3);
    CHECK(w1.sharedStorage() == w2.sharedStorage());
}

// =============================================================================
// Move Semantics Tests
// =============================================================================

TEST_CASE("TensorStorageWrapper move construction", "[TensorStorageWrapper]") {
    arma::fmat m(2, 3, arma::fill::ones);
    TensorStorageWrapper original{ArmadilloTensorStorage{m}};

    TensorStorageWrapper moved(std::move(original));

    CHECK(moved.isValid());
    CHECK(moved.totalElements() == 6);
    CHECK(moved.getStorageType() == TensorStorageType::Armadillo);

    // Original is in moved-from state (shared_ptr was moved)
    CHECK_FALSE(original.isValid()); // NOLINT(bugprone-use-after-move)
}

TEST_CASE("TensorStorageWrapper move assignment", "[TensorStorageWrapper]") {
    arma::fvec v = {1.0f, 2.0f, 3.0f};
    TensorStorageWrapper wrapper{ArmadilloTensorStorage{v}};

    TensorStorageWrapper target;
    CHECK_FALSE(target.isValid());

    target = std::move(wrapper);
    CHECK(target.isValid());
    CHECK(target.totalElements() == 3);
}

// =============================================================================
// Null Wrapper Error Handling
// =============================================================================

TEST_CASE("TensorStorageWrapper null throws on access", "[TensorStorageWrapper]") {
    TensorStorageWrapper wrapper;

    CHECK_THROWS_AS(wrapper.getValueAt({}), std::runtime_error);
    CHECK_THROWS_AS(wrapper.flatData(), std::runtime_error);
    CHECK_THROWS_AS(wrapper.sliceAlongAxis(0, 0), std::runtime_error);
    CHECK_THROWS_AS(wrapper.getColumn(0), std::runtime_error);
    CHECK_THROWS_AS(wrapper.shape(), std::runtime_error);
    CHECK_THROWS_AS(wrapper.totalElements(), std::runtime_error);
    CHECK_THROWS_AS(wrapper.isContiguous(), std::runtime_error);
    CHECK_THROWS_AS(wrapper.getStorageType(), std::runtime_error);
    CHECK_THROWS_AS(wrapper.tryGetCache(), std::runtime_error);
}

// =============================================================================
// Polymorphic Usage (same interface, different backends)
// =============================================================================

TEST_CASE("TensorStorageWrapper polymorphic: same data, different backends", "[TensorStorageWrapper]") {
    // Same 2x3 data stored in both Armadillo and Dense
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};

    TensorStorageWrapper arma_wrapper{ArmadilloTensorStorage{data, 2, 3}};
    TensorStorageWrapper dense_wrapper(DenseTensorStorage(data, {2, 3}));

    // Both have same metadata
    CHECK(arma_wrapper.totalElements() == dense_wrapper.totalElements());
    CHECK(arma_wrapper.shape() == dense_wrapper.shape());
    CHECK(arma_wrapper.isContiguous() == dense_wrapper.isContiguous());

    // Both return same element values
    for (std::size_t r = 0; r < 2; ++r) {
        for (std::size_t c = 0; c < 3; ++c) {
            std::vector<std::size_t> idx = {r, c};
            CHECK_THAT(
                static_cast<double>(arma_wrapper.getValueAt(idx)),
                WithinAbs(static_cast<double>(dense_wrapper.getValueAt(idx)), 1e-6));
        }
    }

    // Both return same columns
    for (std::size_t c = 0; c < 3; ++c) {
        auto arma_col = arma_wrapper.getColumn(c);
        auto dense_col = dense_wrapper.getColumn(c);
        REQUIRE(arma_col.size() == dense_col.size());
        for (std::size_t i = 0; i < arma_col.size(); ++i) {
            CHECK_THAT(
                static_cast<double>(arma_col[i]),
                WithinAbs(static_cast<double>(dense_col[i]), 1e-6));
        }
    }

    // Storage types differ
    CHECK(arma_wrapper.getStorageType() == TensorStorageType::Armadillo);
    CHECK(dense_wrapper.getStorageType() == TensorStorageType::Dense);
}

TEST_CASE("TensorStorageWrapper used in a container (vector)", "[TensorStorageWrapper]") {
    // Demonstrate that different backends can coexist in a single collection
    std::vector<TensorStorageWrapper> wrappers;

    arma::fvec v = {1.0f, 2.0f};
    wrappers.emplace_back(ArmadilloTensorStorage(v));

    std::vector<float> data = {10.0f, 20.0f, 30.0f};
    wrappers.emplace_back(DenseTensorStorage(data, {3}));

    arma::fmat m(4, 5, arma::fill::ones);
    wrappers.emplace_back(ArmadilloTensorStorage(m));

    CHECK(wrappers.size() == 3);
    CHECK(wrappers[0].getStorageType() == TensorStorageType::Armadillo);
    CHECK(wrappers[1].getStorageType() == TensorStorageType::Dense);
    CHECK(wrappers[2].getStorageType() == TensorStorageType::Armadillo);

    CHECK(wrappers[0].totalElements() == 2);
    CHECK(wrappers[1].totalElements() == 3);
    CHECK(wrappers[2].totalElements() == 20);
}

// =============================================================================
// Shared Ownership Access Tests
// =============================================================================

TEST_CASE("TensorStorageWrapper sharedStorage returns non-null for valid wrapper", "[TensorStorageWrapper]") {
    arma::fvec v = {1.0f};
    TensorStorageWrapper wrapper{ArmadilloTensorStorage{v}};

    auto storage = wrapper.sharedStorage();
    CHECK(storage != nullptr);
}

TEST_CASE("TensorStorageWrapper sharedStorage use_count reflects copies", "[TensorStorageWrapper]") {
    arma::fvec v = {1.0f};
    TensorStorageWrapper w1{ArmadilloTensorStorage{v}};

    auto s1 = w1.sharedStorage();
    CHECK(s1.use_count() >= 2); // w1's _impl + s1

    {
        TensorStorageWrapper w2 = w1; // NOLINT(performance-unnecessary-copy-initialization)
        auto s2 = w1.sharedStorage();
        CHECK(s2.use_count() >= 3); // w1's _impl + w2's _impl + s2
    }

    // After w2 goes out of scope, count goes back down
    auto s3 = w1.sharedStorage();
    CHECK(s3.use_count() >= 2);
}

// =============================================================================
// Error Propagation Tests
// =============================================================================

TEST_CASE("TensorStorageWrapper propagates out-of-range from backend", "[TensorStorageWrapper]") {
    arma::fmat m(3, 4, arma::fill::zeros);
    TensorStorageWrapper wrapper{ArmadilloTensorStorage{m}};

    SECTION("getValueAt wrong index count") {
        std::vector<std::size_t> idx = {0};
        CHECK_THROWS_AS(wrapper.getValueAt(idx), std::invalid_argument);
    }

    SECTION("getValueAt out of range") {
        std::vector<std::size_t> idx = {3, 0};
        CHECK_THROWS_AS(wrapper.getValueAt(idx), std::out_of_range);
    }

    SECTION("getColumn out of range") {
        CHECK_THROWS_AS(wrapper.getColumn(4), std::out_of_range);
    }

    SECTION("sliceAlongAxis out of range axis") {
        CHECK_THROWS_AS(wrapper.sliceAlongAxis(2, 0), std::out_of_range);
    }

    SECTION("getColumn throws on 1D") {
        arma::fvec v = {1.0f, 2.0f};
        TensorStorageWrapper w1d{ArmadilloTensorStorage{v}};
        CHECK_THROWS_AS(w1d.getColumn(0), std::invalid_argument);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("TensorStorageWrapper single element tensor", "[TensorStorageWrapper]") {
    std::vector<float> data = {42.0f};
    std::vector<std::size_t> shape = {1};
    TensorStorageWrapper wrapper(DenseTensorStorage(data, shape));

    CHECK(wrapper.totalElements() == 1);

    std::size_t idx = 0;
    CHECK_THAT(static_cast<double>(wrapper.getValueAt({&idx, 1})), WithinAbs(42.0, 1e-6));
}

TEST_CASE("TensorStorageWrapper reassignment to different backend", "[TensorStorageWrapper]") {
    // Start with Armadillo
    arma::fvec v = {1.0f, 2.0f};
    TensorStorageWrapper wrapper{ArmadilloTensorStorage{v}};
    CHECK(wrapper.getStorageType() == TensorStorageType::Armadillo);
    CHECK(wrapper.totalElements() == 2);

    // Reassign to Dense
    std::vector<float> data = {10.0f, 20.0f, 30.0f, 40.0f};
    wrapper = TensorStorageWrapper(DenseTensorStorage(data, {4}));
    CHECK(wrapper.getStorageType() == TensorStorageType::Dense);
    CHECK(wrapper.totalElements() == 4);
}

TEST_CASE("TensorStorageWrapper self-assignment is safe", "[TensorStorageWrapper]") {
    arma::fvec v = {1.0f, 2.0f, 3.0f};
    TensorStorageWrapper wrapper{ArmadilloTensorStorage{v}};

    wrapper = wrapper; // NOLINT(clang-diagnostic-self-assign-overloaded)

    CHECK(wrapper.isValid());
    CHECK(wrapper.totalElements() == 3);
}
