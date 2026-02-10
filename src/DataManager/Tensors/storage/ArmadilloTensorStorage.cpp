#include "ArmadilloTensorStorage.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <string>

// =============================================================================
// Construction
// =============================================================================

ArmadilloTensorStorage::ArmadilloTensorStorage(arma::fvec vector)
    : _data(std::move(vector)) {}

ArmadilloTensorStorage::ArmadilloTensorStorage(arma::fmat matrix)
    : _data(std::move(matrix)) {}

ArmadilloTensorStorage::ArmadilloTensorStorage(arma::fcube cube)
    : _data(std::move(cube)) {}

ArmadilloTensorStorage::ArmadilloTensorStorage(
    std::vector<float> const & data,
    std::size_t num_rows,
    std::size_t num_cols) {
    if (data.size() != num_rows * num_cols) {
        throw std::invalid_argument(
            "ArmadilloTensorStorage: data size (" + std::to_string(data.size()) +
            ") != num_rows * num_cols (" + std::to_string(num_rows) + " * " +
            std::to_string(num_cols) + " = " + std::to_string(num_rows * num_cols) + ")");
    }
    // Input is row-major, Armadillo is column-major.
    // Construct a matrix and fill from row-major data.
    arma::fmat mat(num_rows, num_cols);
    for (std::size_t r = 0; r < num_rows; ++r) {
        for (std::size_t c = 0; c < num_cols; ++c) {
            mat(r, c) = data[r * num_cols + c];
        }
    }
    _data = std::move(mat);
}

// =============================================================================
// Direct Armadillo Access
// =============================================================================

arma::fvec const & ArmadilloTensorStorage::vector() const {
    if (auto const * v = std::get_if<arma::fvec>(&_data)) {
        return *v;
    }
    throw std::logic_error(
        "ArmadilloTensorStorage::vector(): storage is " + dimLabel() + ", not 1D");
}

arma::fvec & ArmadilloTensorStorage::mutableVector() {
    if (auto * v = std::get_if<arma::fvec>(&_data)) {
        return *v;
    }
    throw std::logic_error(
        "ArmadilloTensorStorage::mutableVector(): storage is " + dimLabel() + ", not 1D");
}

arma::fmat const & ArmadilloTensorStorage::matrix() const {
    if (auto const * m = std::get_if<arma::fmat>(&_data)) {
        return *m;
    }
    throw std::logic_error(
        "ArmadilloTensorStorage::matrix(): storage is " + dimLabel() + ", not 2D");
}

arma::fmat & ArmadilloTensorStorage::mutableMatrix() {
    if (auto * m = std::get_if<arma::fmat>(&_data)) {
        return *m;
    }
    throw std::logic_error(
        "ArmadilloTensorStorage::mutableMatrix(): storage is " + dimLabel() + ", not 2D");
}

arma::fcube const & ArmadilloTensorStorage::cube() const {
    if (auto const * c = std::get_if<arma::fcube>(&_data)) {
        return *c;
    }
    throw std::logic_error(
        "ArmadilloTensorStorage::cube(): storage is " + dimLabel() + ", not 3D");
}

arma::fcube & ArmadilloTensorStorage::mutableCube() {
    if (auto * c = std::get_if<arma::fcube>(&_data)) {
        return *c;
    }
    throw std::logic_error(
        "ArmadilloTensorStorage::mutableCube(): storage is " + dimLabel() + ", not 3D");
}

std::size_t ArmadilloTensorStorage::ndim() const noexcept {
    return std::visit([](auto const & d) -> std::size_t {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, arma::fvec>) {
            return 1;
        } else if constexpr (std::is_same_v<T, arma::fmat>) {
            return 2;
        } else {
            return 3;
        }
    }, _data);
}

// =============================================================================
// CRTP Implementation
// =============================================================================

float ArmadilloTensorStorage::getValueAtImpl(std::span<std::size_t const> indices) const {
    return std::visit([&](auto const & d) -> float {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, arma::fvec>) {
            if (indices.size() != 1) {
                throw std::invalid_argument(
                    "ArmadilloTensorStorage::getValueAt: expected 1 index for 1D tensor, got " +
                    std::to_string(indices.size()));
            }
            if (indices[0] >= d.n_elem) {
                throw std::out_of_range(
                    "ArmadilloTensorStorage::getValueAt: index " +
                    std::to_string(indices[0]) + " >= size " + std::to_string(d.n_elem));
            }
            return d(indices[0]);
        } else if constexpr (std::is_same_v<T, arma::fmat>) {
            if (indices.size() != 2) {
                throw std::invalid_argument(
                    "ArmadilloTensorStorage::getValueAt: expected 2 indices for 2D tensor, got " +
                    std::to_string(indices.size()));
            }
            if (indices[0] >= d.n_rows) {
                throw std::out_of_range(
                    "ArmadilloTensorStorage::getValueAt: row index " +
                    std::to_string(indices[0]) + " >= n_rows " + std::to_string(d.n_rows));
            }
            if (indices[1] >= d.n_cols) {
                throw std::out_of_range(
                    "ArmadilloTensorStorage::getValueAt: col index " +
                    std::to_string(indices[1]) + " >= n_cols " + std::to_string(d.n_cols));
            }
            return d(indices[0], indices[1]);
        } else { // arma::fcube
            if (indices.size() != 3) {
                throw std::invalid_argument(
                    "ArmadilloTensorStorage::getValueAt: expected 3 indices for 3D tensor, got " +
                    std::to_string(indices.size()));
            }
            // Our shape is [n_slices, n_rows, n_cols]
            // indices[0] = slice, indices[1] = row, indices[2] = col
            if (indices[0] >= d.n_slices) {
                throw std::out_of_range(
                    "ArmadilloTensorStorage::getValueAt: slice index " +
                    std::to_string(indices[0]) + " >= n_slices " + std::to_string(d.n_slices));
            }
            if (indices[1] >= d.n_rows) {
                throw std::out_of_range(
                    "ArmadilloTensorStorage::getValueAt: row index " +
                    std::to_string(indices[1]) + " >= n_rows " + std::to_string(d.n_rows));
            }
            if (indices[2] >= d.n_cols) {
                throw std::out_of_range(
                    "ArmadilloTensorStorage::getValueAt: col index " +
                    std::to_string(indices[2]) + " >= n_cols " + std::to_string(d.n_cols));
            }
            return d(indices[1], indices[2], indices[0]); // arma: (row, col, slice)
        }
    }, _data);
}

std::span<float const> ArmadilloTensorStorage::flatDataImpl() const {
    return std::visit([](auto const & d) -> std::span<float const> {
        return {d.memptr(), d.n_elem};
    }, _data);
}

std::vector<float> ArmadilloTensorStorage::sliceAlongAxisImpl(
    std::size_t axis, std::size_t index) const {
    return std::visit([&](auto const & d) -> std::vector<float> {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, arma::fvec>) {
            // 1D: slicing axis 0 at index → single element
            if (axis != 0) {
                throw std::out_of_range(
                    "ArmadilloTensorStorage::sliceAlongAxis: axis " +
                    std::to_string(axis) + " out of range for 1D tensor");
            }
            if (index >= d.n_elem) {
                throw std::out_of_range(
                    "ArmadilloTensorStorage::sliceAlongAxis: index " +
                    std::to_string(index) + " >= size " + std::to_string(d.n_elem));
            }
            return {d(index)};
        } else if constexpr (std::is_same_v<T, arma::fmat>) {
            if (axis == 0) {
                // Slice row → returns a vector of length n_cols
                if (index >= d.n_rows) {
                    throw std::out_of_range(
                        "ArmadilloTensorStorage::sliceAlongAxis: row index " +
                        std::to_string(index) + " >= n_rows " + std::to_string(d.n_rows));
                }
                arma::frowvec row_vec = d.row(index);
                return {row_vec.begin(), row_vec.end()};
            } else if (axis == 1) {
                // Slice column → returns a vector of length n_rows
                if (index >= d.n_cols) {
                    throw std::out_of_range(
                        "ArmadilloTensorStorage::sliceAlongAxis: col index " +
                        std::to_string(index) + " >= n_cols " + std::to_string(d.n_cols));
                }
                arma::fvec col_vec = d.col(index);
                return {col_vec.begin(), col_vec.end()};
            } else {
                throw std::out_of_range(
                    "ArmadilloTensorStorage::sliceAlongAxis: axis " +
                    std::to_string(axis) + " out of range for 2D tensor");
            }
        } else { // arma::fcube — shape [n_slices, n_rows, n_cols]
            if (axis == 0) {
                // Fix slice → returns [n_rows, n_cols] in row-major
                if (index >= d.n_slices) {
                    throw std::out_of_range(
                        "ArmadilloTensorStorage::sliceAlongAxis: slice index " +
                        std::to_string(index) + " >= n_slices " + std::to_string(d.n_slices));
                }
                arma::fmat slice_mat = d.slice(index);
                std::vector<float> result;
                result.reserve(slice_mat.n_rows * slice_mat.n_cols);
                for (std::size_t r = 0; r < slice_mat.n_rows; ++r) {
                    for (std::size_t c = 0; c < slice_mat.n_cols; ++c) {
                        result.push_back(slice_mat(r, c));
                    }
                }
                return result;
            } else if (axis == 1) {
                // Fix row → returns [n_slices, n_cols] in row-major
                if (index >= d.n_rows) {
                    throw std::out_of_range(
                        "ArmadilloTensorStorage::sliceAlongAxis: row index " +
                        std::to_string(index) + " >= n_rows " + std::to_string(d.n_rows));
                }
                std::vector<float> result;
                result.reserve(d.n_slices * d.n_cols);
                for (std::size_t s = 0; s < d.n_slices; ++s) {
                    for (std::size_t c = 0; c < d.n_cols; ++c) {
                        result.push_back(d(index, c, s));
                    }
                }
                return result;
            } else if (axis == 2) {
                // Fix col → returns [n_slices, n_rows] in row-major
                if (index >= d.n_cols) {
                    throw std::out_of_range(
                        "ArmadilloTensorStorage::sliceAlongAxis: col index " +
                        std::to_string(index) + " >= n_cols " + std::to_string(d.n_cols));
                }
                std::vector<float> result;
                result.reserve(d.n_slices * d.n_rows);
                for (std::size_t s = 0; s < d.n_slices; ++s) {
                    for (std::size_t r = 0; r < d.n_rows; ++r) {
                        result.push_back(d(r, index, s));
                    }
                }
                return result;
            } else {
                throw std::out_of_range(
                    "ArmadilloTensorStorage::sliceAlongAxis: axis " +
                    std::to_string(axis) + " out of range for 3D tensor");
            }
        }
    }, _data);
}

std::vector<float> ArmadilloTensorStorage::getColumnImpl(std::size_t col) const {
    return std::visit([&](auto const & d) -> std::vector<float> {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, arma::fvec>) {
            throw std::invalid_argument(
                "ArmadilloTensorStorage::getColumn: not supported for 1D tensor (no column axis)");
        } else if constexpr (std::is_same_v<T, arma::fmat>) {
            if (col >= d.n_cols) {
                throw std::out_of_range(
                    "ArmadilloTensorStorage::getColumn: col " +
                    std::to_string(col) + " >= n_cols " + std::to_string(d.n_cols));
            }
            arma::fvec col_vec = d.col(col);
            return {col_vec.begin(), col_vec.end()};
        } else { // arma::fcube — shape [n_slices, n_rows, n_cols]
            // getColumn on 3D: extract column along axis 1 (n_cols dimension),
            // flattening [n_slices, n_rows] into contiguous output
            if (col >= d.n_cols) {
                throw std::out_of_range(
                    "ArmadilloTensorStorage::getColumn: col " +
                    std::to_string(col) + " >= n_cols " + std::to_string(d.n_cols));
            }
            std::vector<float> result;
            result.reserve(d.n_slices * d.n_rows);
            for (std::size_t s = 0; s < d.n_slices; ++s) {
                for (std::size_t r = 0; r < d.n_rows; ++r) {
                    result.push_back(d(r, col, s));
                }
            }
            return result;
        }
    }, _data);
}

std::vector<std::size_t> ArmadilloTensorStorage::shapeImpl() const {
    return std::visit([](auto const & d) -> std::vector<std::size_t> {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, arma::fvec>) {
            return {d.n_elem};
        } else if constexpr (std::is_same_v<T, arma::fmat>) {
            return {d.n_rows, d.n_cols};
        } else { // arma::fcube — shape [n_slices, n_rows, n_cols]
            return {d.n_slices, d.n_rows, d.n_cols};
        }
    }, _data);
}

std::size_t ArmadilloTensorStorage::totalElementsImpl() const {
    return std::visit([](auto const & d) -> std::size_t {
        return d.n_elem;
    }, _data);
}

TensorStorageCache ArmadilloTensorStorage::tryGetCacheImpl() const {
    return std::visit([](auto const & d) -> TensorStorageCache {
        using T = std::decay_t<decltype(d)>;
        TensorStorageCache cache;
        cache.data_ptr = d.memptr();
        cache.total_elements = d.n_elem;
        cache.is_valid = true;

        if constexpr (std::is_same_v<T, arma::fvec>) {
            cache.shape = {d.n_elem};
            cache.strides = {1};
        } else if constexpr (std::is_same_v<T, arma::fmat>) {
            cache.shape = {d.n_rows, d.n_cols};
            // Armadillo is column-major: stride along row=1, stride along col=n_rows
            cache.strides = {1, d.n_rows};
        } else { // arma::fcube
            cache.shape = {d.n_slices, d.n_rows, d.n_cols};
            // Armadillo cube: column-major within each slice, slices contiguous
            // stride: (n_rows*n_cols for slice, 1 for row, n_rows for col)
            cache.strides = {d.n_rows * d.n_cols, 1, d.n_rows};
        }
        return cache;
    }, _data);
}

// =============================================================================
// Private Helpers
// =============================================================================

std::string ArmadilloTensorStorage::dimLabel() const {
    return std::visit([](auto const & d) -> std::string {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, arma::fvec>) {
            return "1D (fvec)";
        } else if constexpr (std::is_same_v<T, arma::fmat>) {
            return "2D (fmat)";
        } else {
            return "3D (fcube)";
        }
    }, _data);
}
