/**
 * @file LibTorchTensorStorage.test.cpp
 * @brief Unit tests for LibTorchTensorStorage (torch::Tensor wrapper)
 *
 * Tests cover:
 * - Construction from torch::Tensor
 * - Construction from DenseTensorStorage (fromDense)
 * - Construction from flat data + shape (fromFlatData)
 * - Shape, totalElements, isContiguous, ndim metadata
 * - Element access via getValueAt (1D, 2D, 3D, 4D)
 * - Column extraction via getColumn (2D, 3D)
 * - Axis slicing via sliceAlongAxis (2D, 3D, 4D)
 * - flatData access and row-major layout verification
 * - Cache support (tryGetCache) with stride verification
 * - Storage type reporting
 * - Direct torch::Tensor access (tensor(), mutableTensor())
 * - Device queries (isCpu, isCuda)
 * - Error handling (wrong dtype, undefined tensor, out-of-range)
 * - Interop with TensorStorageWrapper (type erasure + recovery)
 */

#ifdef TENSOR_BACKEND_LIBTORCH

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Tensors/storage/LibTorchTensorStorage.hpp"
#include "Tensors/storage/DenseTensorStorage.hpp"
#include "Tensors/storage/TensorStorageWrapper.hpp"

#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

using Catch::Matchers::WithinAbs;

// =============================================================================
// Construction Tests
// =============================================================================

TEST_CASE("LibTorchTensorStorage construction from torch::Tensor", "[LibTorchTensorStorage]") {
    // 2x3 matrix: [[1,2,3],[4,5,6]]
    auto tensor = torch::tensor({{1.0f, 2.0f, 3.0f},
                                  {4.0f, 5.0f, 6.0f}});

    LibTorchTensorStorage storage(tensor);

    SECTION("metadata") {
        CHECK(storage.ndim() == 2);
        CHECK(storage.totalElements() == 6);
        CHECK(storage.isContiguous());
        CHECK(storage.getStorageType() == TensorStorageType::LibTorch);

        auto s = storage.shape();
        REQUIRE(s.size() == 2);
        CHECK(s[0] == 2);
        CHECK(s[1] == 3);
    }

    SECTION("direct tensor access") {
        auto const & t = storage.tensor();
        CHECK(t.size(0) == 2);
        CHECK(t.size(1) == 3);
        CHECK(t.scalar_type() == torch::kFloat32);
    }

    SECTION("device is CPU") {
        CHECK(storage.isCpu());
        CHECK_FALSE(storage.isCuda());
    }
}

TEST_CASE("LibTorchTensorStorage construction from 1D tensor", "[LibTorchTensorStorage]") {
    auto tensor = torch::tensor({10.0f, 20.0f, 30.0f, 40.0f});
    LibTorchTensorStorage storage(tensor);

    CHECK(storage.ndim() == 1);
    CHECK(storage.totalElements() == 4);
    CHECK(storage.isContiguous());

    auto s = storage.shape();
    REQUIRE(s.size() == 1);
    CHECK(s[0] == 4);
}

TEST_CASE("LibTorchTensorStorage construction from 3D tensor", "[LibTorchTensorStorage]") {
    // 2x3x4 tensor
    auto tensor = torch::arange(24.0f).reshape({2, 3, 4});
    LibTorchTensorStorage storage(tensor);

    CHECK(storage.ndim() == 3);
    CHECK(storage.totalElements() == 24);

    auto s = storage.shape();
    REQUIRE(s.size() == 3);
    CHECK(s[0] == 2);
    CHECK(s[1] == 3);
    CHECK(s[2] == 4);
}

TEST_CASE("LibTorchTensorStorage construction from 4D tensor", "[LibTorchTensorStorage]") {
    // batch × channel × height × width
    auto tensor = torch::arange(120.0f).reshape({2, 3, 4, 5});
    LibTorchTensorStorage storage(tensor);

    CHECK(storage.ndim() == 4);
    CHECK(storage.totalElements() == 120);

    auto s = storage.shape();
    REQUIRE(s.size() == 4);
    CHECK(s[0] == 2);
    CHECK(s[1] == 3);
    CHECK(s[2] == 4);
    CHECK(s[3] == 5);
}

TEST_CASE("LibTorchTensorStorage construction errors", "[LibTorchTensorStorage]") {
    SECTION("undefined tensor throws") {
        torch::Tensor undefined;
        CHECK_THROWS_AS(LibTorchTensorStorage(undefined), std::invalid_argument);
    }

    SECTION("wrong dtype throws") {
        auto int_tensor = torch::tensor({1, 2, 3}); // kInt
        CHECK_THROWS_AS(LibTorchTensorStorage(int_tensor), std::invalid_argument);
    }

    SECTION("double dtype throws") {
        auto double_tensor = torch::tensor({1.0, 2.0, 3.0}, torch::kFloat64);
        CHECK_THROWS_AS(LibTorchTensorStorage(double_tensor), std::invalid_argument);
    }

    SECTION("scalar tensor throws") {
        auto scalar = torch::tensor(42.0f);
        CHECK_THROWS_AS(LibTorchTensorStorage(scalar), std::invalid_argument);
    }
}

// =============================================================================
// fromDense Construction
// =============================================================================

TEST_CASE("LibTorchTensorStorage fromDense", "[LibTorchTensorStorage]") {
    // 2x3 row-major: [[1,2,3],[4,5,6]]
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    std::vector<std::size_t> shape = {2, 3};
    DenseTensorStorage dense(data, shape);

    auto storage = LibTorchTensorStorage::fromDense(dense);

    CHECK(storage.ndim() == 2);
    CHECK(storage.totalElements() == 6);

    auto s = storage.shape();
    REQUIRE(s.size() == 2);
    CHECK(s[0] == 2);
    CHECK(s[1] == 3);

    SECTION("data matches") {
        std::vector<std::size_t> idx = {0, 0};
        CHECK_THAT(storage.getValueAt(idx), WithinAbs(1.0f, 1e-5f));
        idx = {0, 2};
        CHECK_THAT(storage.getValueAt(idx), WithinAbs(3.0f, 1e-5f));
        idx = {1, 0};
        CHECK_THAT(storage.getValueAt(idx), WithinAbs(4.0f, 1e-5f));
        idx = {1, 2};
        CHECK_THAT(storage.getValueAt(idx), WithinAbs(6.0f, 1e-5f));
    }
}

TEST_CASE("LibTorchTensorStorage fromDense 4D", "[LibTorchTensorStorage]") {
    // 2x2x2x2 tensor
    std::vector<float> data(16);
    std::iota(data.begin(), data.end(), 0.0f);
    std::vector<std::size_t> shape = {2, 2, 2, 2};
    DenseTensorStorage dense(data, shape);

    auto storage = LibTorchTensorStorage::fromDense(dense);

    CHECK(storage.ndim() == 4);
    CHECK(storage.totalElements() == 16);

    // Verify element [1, 0, 1, 0] = 1*8 + 0*4 + 1*2 + 0 = 10
    std::vector<std::size_t> idx = {1, 0, 1, 0};
    CHECK_THAT(storage.getValueAt(idx), WithinAbs(10.0f, 1e-5f));
}

// =============================================================================
// fromFlatData Construction
// =============================================================================

TEST_CASE("LibTorchTensorStorage fromFlatData", "[LibTorchTensorStorage]") {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    std::vector<std::size_t> shape = {2, 3};

    auto storage = LibTorchTensorStorage::fromFlatData(data, shape);

    CHECK(storage.ndim() == 2);
    CHECK(storage.totalElements() == 6);

    SECTION("data matches") {
        std::vector<std::size_t> idx = {0, 0};
        CHECK_THAT(storage.getValueAt(idx), WithinAbs(1.0f, 1e-5f));
        idx = {1, 1};
        CHECK_THAT(storage.getValueAt(idx), WithinAbs(5.0f, 1e-5f));
    }

    SECTION("size mismatch throws") {
        std::vector<float> bad_data = {1.0f, 2.0f};
        CHECK_THROWS_AS(
            LibTorchTensorStorage::fromFlatData(bad_data, shape),
            std::invalid_argument);
    }
}

// =============================================================================
// Element Access Tests
// =============================================================================

TEST_CASE("LibTorchTensorStorage element access 2D", "[LibTorchTensorStorage]") {
    auto tensor = torch::tensor({{1.0f, 2.0f, 3.0f},
                                  {4.0f, 5.0f, 6.0f}});
    LibTorchTensorStorage storage(tensor);

    CHECK_THAT(storage.getValueAt(std::vector<std::size_t>{0, 0}), WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(storage.getValueAt(std::vector<std::size_t>{0, 1}), WithinAbs(2.0f, 1e-5f));
    CHECK_THAT(storage.getValueAt(std::vector<std::size_t>{0, 2}), WithinAbs(3.0f, 1e-5f));
    CHECK_THAT(storage.getValueAt(std::vector<std::size_t>{1, 0}), WithinAbs(4.0f, 1e-5f));
    CHECK_THAT(storage.getValueAt(std::vector<std::size_t>{1, 1}), WithinAbs(5.0f, 1e-5f));
    CHECK_THAT(storage.getValueAt(std::vector<std::size_t>{1, 2}), WithinAbs(6.0f, 1e-5f));
}

TEST_CASE("LibTorchTensorStorage element access 3D", "[LibTorchTensorStorage]") {
    // 2x3x4 tensor with sequential values
    auto tensor = torch::arange(24.0f).reshape({2, 3, 4});
    LibTorchTensorStorage storage(tensor);

    // [0,0,0] = 0, [0,0,3] = 3, [0,2,3] = 11, [1,0,0] = 12, [1,2,3] = 23
    CHECK_THAT(storage.getValueAt(std::vector<std::size_t>{0, 0, 0}), WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(storage.getValueAt(std::vector<std::size_t>{0, 0, 3}), WithinAbs(3.0f, 1e-5f));
    CHECK_THAT(storage.getValueAt(std::vector<std::size_t>{0, 2, 3}), WithinAbs(11.0f, 1e-5f));
    CHECK_THAT(storage.getValueAt(std::vector<std::size_t>{1, 0, 0}), WithinAbs(12.0f, 1e-5f));
    CHECK_THAT(storage.getValueAt(std::vector<std::size_t>{1, 2, 3}), WithinAbs(23.0f, 1e-5f));
}

TEST_CASE("LibTorchTensorStorage element access errors", "[LibTorchTensorStorage]") {
    auto tensor = torch::tensor({{1.0f, 2.0f}, {3.0f, 4.0f}});
    LibTorchTensorStorage storage(tensor);

    SECTION("wrong number of indices") {
        CHECK_THROWS_AS(
            storage.getValueAt(std::vector<std::size_t>{0}),
            std::invalid_argument);
        CHECK_THROWS_AS(
            storage.getValueAt(std::vector<std::size_t>{0, 0, 0}),
            std::invalid_argument);
    }

    SECTION("out of range") {
        CHECK_THROWS_AS(
            storage.getValueAt(std::vector<std::size_t>{2, 0}),
            std::out_of_range);
        CHECK_THROWS_AS(
            storage.getValueAt(std::vector<std::size_t>{0, 2}),
            std::out_of_range);
    }
}

// =============================================================================
// flatData Tests
// =============================================================================

TEST_CASE("LibTorchTensorStorage flatData row-major", "[LibTorchTensorStorage]") {
    // torch default is row-major (C contiguous)
    auto tensor = torch::tensor({{1.0f, 2.0f, 3.0f},
                                  {4.0f, 5.0f, 6.0f}});
    LibTorchTensorStorage storage(tensor);

    auto flat = storage.flatData();
    REQUIRE(flat.size() == 6);
    CHECK_THAT(flat[0], WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(flat[1], WithinAbs(2.0f, 1e-5f));
    CHECK_THAT(flat[2], WithinAbs(3.0f, 1e-5f));
    CHECK_THAT(flat[3], WithinAbs(4.0f, 1e-5f));
    CHECK_THAT(flat[4], WithinAbs(5.0f, 1e-5f));
    CHECK_THAT(flat[5], WithinAbs(6.0f, 1e-5f));
}

TEST_CASE("LibTorchTensorStorage flatData 1D", "[LibTorchTensorStorage]") {
    auto tensor = torch::tensor({10.0f, 20.0f, 30.0f});
    LibTorchTensorStorage storage(tensor);

    auto flat = storage.flatData();
    REQUIRE(flat.size() == 3);
    CHECK_THAT(flat[0], WithinAbs(10.0f, 1e-5f));
    CHECK_THAT(flat[1], WithinAbs(20.0f, 1e-5f));
    CHECK_THAT(flat[2], WithinAbs(30.0f, 1e-5f));
}

// =============================================================================
// Column Extraction Tests
// =============================================================================

TEST_CASE("LibTorchTensorStorage getColumn 2D", "[LibTorchTensorStorage]") {
    // 3x4 matrix: row i, col j = i*4+j
    auto tensor = torch::arange(12.0f).reshape({3, 4});
    LibTorchTensorStorage storage(tensor);

    SECTION("column 0") {
        auto col = storage.getColumn(0);
        REQUIRE(col.size() == 3);
        CHECK_THAT(col[0], WithinAbs(0.0f, 1e-5f));  // [0,0]
        CHECK_THAT(col[1], WithinAbs(4.0f, 1e-5f));  // [1,0]
        CHECK_THAT(col[2], WithinAbs(8.0f, 1e-5f));  // [2,0]
    }

    SECTION("column 3") {
        auto col = storage.getColumn(3);
        REQUIRE(col.size() == 3);
        CHECK_THAT(col[0], WithinAbs(3.0f, 1e-5f));  // [0,3]
        CHECK_THAT(col[1], WithinAbs(7.0f, 1e-5f));  // [1,3]
        CHECK_THAT(col[2], WithinAbs(11.0f, 1e-5f)); // [2,3]
    }
}

TEST_CASE("LibTorchTensorStorage getColumn 3D", "[LibTorchTensorStorage]") {
    // 2x3x4 tensor — column is slice along last axis
    auto tensor = torch::arange(24.0f).reshape({2, 3, 4});
    LibTorchTensorStorage storage(tensor);

    // Column 2: all elements where last index == 2
    // [0,0,2]=2, [0,1,2]=6, [0,2,2]=10, [1,0,2]=14, [1,1,2]=18, [1,2,2]=22
    auto col = storage.getColumn(2);
    REQUIRE(col.size() == 6); // 2*3 = 6 elements
    CHECK_THAT(col[0], WithinAbs(2.0f, 1e-5f));
    CHECK_THAT(col[1], WithinAbs(6.0f, 1e-5f));
    CHECK_THAT(col[2], WithinAbs(10.0f, 1e-5f));
    CHECK_THAT(col[3], WithinAbs(14.0f, 1e-5f));
    CHECK_THAT(col[4], WithinAbs(18.0f, 1e-5f));
    CHECK_THAT(col[5], WithinAbs(22.0f, 1e-5f));
}

TEST_CASE("LibTorchTensorStorage getColumn errors", "[LibTorchTensorStorage]") {
    SECTION("1D tensor throws") {
        auto tensor = torch::tensor({1.0f, 2.0f, 3.0f});
        LibTorchTensorStorage storage(tensor);
        CHECK_THROWS_AS(storage.getColumn(0), std::logic_error);
    }

    SECTION("column out of range") {
        auto tensor = torch::tensor({{1.0f, 2.0f}, {3.0f, 4.0f}});
        LibTorchTensorStorage storage(tensor);
        CHECK_THROWS_AS(storage.getColumn(2), std::out_of_range);
    }
}

// =============================================================================
// sliceAlongAxis Tests
// =============================================================================

TEST_CASE("LibTorchTensorStorage sliceAlongAxis 2D", "[LibTorchTensorStorage]") {
    // 3x4: row i, col j = i*4+j
    auto tensor = torch::arange(12.0f).reshape({3, 4});
    LibTorchTensorStorage storage(tensor);

    SECTION("slice axis 0 (row)") {
        auto row1 = storage.sliceAlongAxis(0, 1);
        REQUIRE(row1.size() == 4);
        CHECK_THAT(row1[0], WithinAbs(4.0f, 1e-5f));
        CHECK_THAT(row1[1], WithinAbs(5.0f, 1e-5f));
        CHECK_THAT(row1[2], WithinAbs(6.0f, 1e-5f));
        CHECK_THAT(row1[3], WithinAbs(7.0f, 1e-5f));
    }

    SECTION("slice axis 1 (column)") {
        auto col2 = storage.sliceAlongAxis(1, 2);
        REQUIRE(col2.size() == 3);
        CHECK_THAT(col2[0], WithinAbs(2.0f, 1e-5f));
        CHECK_THAT(col2[1], WithinAbs(6.0f, 1e-5f));
        CHECK_THAT(col2[2], WithinAbs(10.0f, 1e-5f));
    }
}

TEST_CASE("LibTorchTensorStorage sliceAlongAxis 3D", "[LibTorchTensorStorage]") {
    // 2x3x4 tensor
    auto tensor = torch::arange(24.0f).reshape({2, 3, 4});
    LibTorchTensorStorage storage(tensor);

    SECTION("slice axis 0 (depth slice)") {
        // [1, :, :] = values 12..23
        auto slice = storage.sliceAlongAxis(0, 1);
        REQUIRE(slice.size() == 12);
        CHECK_THAT(slice[0], WithinAbs(12.0f, 1e-5f));
        CHECK_THAT(slice[11], WithinAbs(23.0f, 1e-5f));
    }

    SECTION("slice axis 1 (row within each depth)") {
        // [:, 2, :] = [8,9,10,11, 20,21,22,23]
        auto slice = storage.sliceAlongAxis(1, 2);
        REQUIRE(slice.size() == 8);
        CHECK_THAT(slice[0], WithinAbs(8.0f, 1e-5f));
        CHECK_THAT(slice[3], WithinAbs(11.0f, 1e-5f));
        CHECK_THAT(slice[4], WithinAbs(20.0f, 1e-5f));
        CHECK_THAT(slice[7], WithinAbs(23.0f, 1e-5f));
    }
}

TEST_CASE("LibTorchTensorStorage sliceAlongAxis errors", "[LibTorchTensorStorage]") {
    auto tensor = torch::tensor({{1.0f, 2.0f}, {3.0f, 4.0f}});
    LibTorchTensorStorage storage(tensor);

    CHECK_THROWS_AS(storage.sliceAlongAxis(2, 0), std::out_of_range); // axis out of range
    CHECK_THROWS_AS(storage.sliceAlongAxis(0, 2), std::out_of_range); // index out of range
}

// =============================================================================
// Cache Tests
// =============================================================================

TEST_CASE("LibTorchTensorStorage cache for contiguous CPU tensor", "[LibTorchTensorStorage]") {
    auto tensor = torch::tensor({{1.0f, 2.0f, 3.0f},
                                  {4.0f, 5.0f, 6.0f}});
    LibTorchTensorStorage storage(tensor);

    auto cache = storage.tryGetCache();
    REQUIRE(cache.isValid());
    CHECK(cache.data_ptr != nullptr);
    CHECK(cache.total_elements == 6);

    REQUIRE(cache.shape.size() == 2);
    CHECK(cache.shape[0] == 2);
    CHECK(cache.shape[1] == 3);

    // Row-major strides: stride[0]=3, stride[1]=1
    REQUIRE(cache.strides.size() == 2);
    CHECK(cache.strides[0] == 3);
    CHECK(cache.strides[1] == 1);

    // Verify data pointer matches torch data
    CHECK(cache.data_ptr == tensor.data_ptr<float>());
}

TEST_CASE("LibTorchTensorStorage cache for non-contiguous tensor", "[LibTorchTensorStorage]") {
    // Transposed tensor is typically not contiguous
    auto tensor = torch::arange(6.0f).reshape({2, 3}).t();
    REQUIRE_FALSE(tensor.is_contiguous());

    LibTorchTensorStorage storage(tensor);
    auto cache = storage.tryGetCache();
    CHECK_FALSE(cache.isValid());
    CHECK(cache.total_elements == 6);
}

// =============================================================================
// Mutable Tensor Access
// =============================================================================

TEST_CASE("LibTorchTensorStorage mutableTensor", "[LibTorchTensorStorage]") {
    auto tensor = torch::zeros({2, 3});
    LibTorchTensorStorage storage(tensor);

    // Modify via mutable access
    storage.mutableTensor()[0][1] = 42.0f;

    std::vector<std::size_t> idx = {0, 1};
    CHECK_THAT(storage.getValueAt(idx), WithinAbs(42.0f, 1e-5f));
}

// =============================================================================
// Device Management Tests
// =============================================================================

TEST_CASE("LibTorchTensorStorage toCpu is no-op for CPU tensor", "[LibTorchTensorStorage]") {
    auto tensor = torch::tensor({1.0f, 2.0f, 3.0f});
    LibTorchTensorStorage storage(tensor);

    CHECK(storage.isCpu());
    storage.toCpu(); // Should not throw or change anything
    CHECK(storage.isCpu());
}

// Note: CUDA tests require a CUDA-capable GPU at runtime. We test the
// interface but cannot guarantee CUDA availability in CI.

// =============================================================================
// TensorStorageWrapper Integration
// =============================================================================

TEST_CASE("LibTorchTensorStorage works with TensorStorageWrapper", "[LibTorchTensorStorage]") {
    auto tensor = torch::tensor({{1.0f, 2.0f, 3.0f},
                                  {4.0f, 5.0f, 6.0f}});
    LibTorchTensorStorage storage(tensor);

    TensorStorageWrapper wrapper(std::move(storage));

    SECTION("type erasure preserves interface") {
        CHECK(wrapper.isValid());
        CHECK(wrapper.totalElements() == 6);
        CHECK(wrapper.isContiguous());
        CHECK(wrapper.getStorageType() == TensorStorageType::LibTorch);

        auto s = wrapper.shape();
        REQUIRE(s.size() == 2);
        CHECK(s[0] == 2);
        CHECK(s[1] == 3);
    }

    SECTION("element access through wrapper") {
        std::vector<std::size_t> idx = {1, 2};
        CHECK_THAT(wrapper.getValueAt(idx), WithinAbs(6.0f, 1e-5f));
    }

    SECTION("type recovery via tryGetAs") {
        auto const * recovered = wrapper.tryGetAs<LibTorchTensorStorage>();
        REQUIRE(recovered != nullptr);
        CHECK(recovered->isCpu());

        auto const & t = recovered->tensor();
        CHECK(t.size(0) == 2);
        CHECK(t.size(1) == 3);
    }

    SECTION("wrong type returns nullptr") {
        auto const * wrong = wrapper.tryGetAs<DenseTensorStorage>();
        CHECK(wrong == nullptr);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("LibTorchTensorStorage single element", "[LibTorchTensorStorage]") {
    auto tensor = torch::tensor({42.0f});
    LibTorchTensorStorage storage(tensor);

    CHECK(storage.ndim() == 1);
    CHECK(storage.totalElements() == 1);

    auto flat = storage.flatData();
    REQUIRE(flat.size() == 1);
    CHECK_THAT(flat[0], WithinAbs(42.0f, 1e-5f));
}

TEST_CASE("LibTorchTensorStorage dimension with size 1", "[LibTorchTensorStorage]") {
    // 1x5 matrix
    auto tensor = torch::arange(5.0f).reshape({1, 5});
    LibTorchTensorStorage storage(tensor);

    CHECK(storage.ndim() == 2);
    auto s = storage.shape();
    CHECK(s[0] == 1);
    CHECK(s[1] == 5);

    // Row 0 should be all elements
    auto row = storage.sliceAlongAxis(0, 0);
    REQUIRE(row.size() == 5);
    for (std::size_t i = 0; i < 5; ++i) {
        CHECK_THAT(row[i], WithinAbs(static_cast<float>(i), 1e-5f));
    }
}

TEST_CASE("LibTorchTensorStorage large tensor", "[LibTorchTensorStorage]") {
    // 100x200 tensor
    auto tensor = torch::arange(20000.0f).reshape({100, 200});
    LibTorchTensorStorage storage(tensor);

    CHECK(storage.totalElements() == 20000);

    // Spot check a few elements
    // [50, 100] = 50*200 + 100 = 10100
    std::vector<std::size_t> idx = {50, 100};
    CHECK_THAT(storage.getValueAt(idx), WithinAbs(10100.0f, 1e-5f));
}

#endif // TENSOR_BACKEND_LIBTORCH
