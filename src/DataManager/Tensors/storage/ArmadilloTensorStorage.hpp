#ifndef ARMADILLO_TENSOR_STORAGE_HPP
#define ARMADILLO_TENSOR_STORAGE_HPP

#include "TensorStorageBase.hpp"

#include <armadillo>

#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

/**
 * @brief Tensor storage backend using Armadillo (arma::fvec, arma::fmat, arma::fcube)
 *
 * This is the **default storage backend** for tensors with ≤3 dimensions.
 * Armadillo is a required project dependency, so this backend is always available.
 *
 * Wraps `arma::fvec` (1D), `arma::fmat` (2D), or `arma::fcube` (3D) and provides:
 * - Zero-copy access for mlpack interop via matrix()/cube() getters
 * - Transparent column-major to row-major translation in the CRTP interface
 * - Efficient column extraction leveraging Armadillo's native col() access
 *
 * ## Layout note
 *
 * Armadillo uses **column-major** storage. This class presents **row-major**
 * semantics to consumers through the TensorStorageBase interface:
 * - `flatData()` returns data in Armadillo's native column-major layout
 *   (consumers needing row-major should use `getValueAt()` or `getColumn()`)
 * - `getValueAt({r, c})` on a matrix accesses row r, column c regardless
 *   of internal layout
 * - `getColumn(c)` efficiently extracts column c using Armadillo's native `col()`
 *
 * For zero-copy Armadillo access (e.g., passing to mlpack), use `matrix()` or
 * `cube()` directly — these return const references to the underlying Armadillo
 * objects with no translation.
 */
class ArmadilloTensorStorage : public TensorStorageBase<ArmadilloTensorStorage> {
public:
    // ========== Construction ==========

    /**
     * @brief Construct 1D storage from an Armadillo column vector
     *
     * Stored internally as arma::fvec. Shape will be [N].
     */
    explicit ArmadilloTensorStorage(arma::fvec vector);

    /**
     * @brief Construct 2D storage from an Armadillo matrix
     *
     * Stored internally as arma::fmat. Shape will be [nrows, ncols].
     */
    explicit ArmadilloTensorStorage(arma::fmat matrix);

    /**
     * @brief Construct 3D storage from an Armadillo cube
     *
     * Stored internally as arma::fcube. Shape will be [nslices, nrows, ncols].
     *
     * Note on Armadillo cube layout: an fcube of size (n_rows, n_cols, n_slices)
     * is treated here as a 3D tensor with shape [n_slices, n_rows, n_cols],
     * where slices are the outermost dimension. This maps naturally to
     * "time × height × width" or "batch × rows × cols" semantics.
     */
    explicit ArmadilloTensorStorage(arma::fcube cube);

    /**
     * @brief Construct 2D storage from flat row-major data
     *
     * Converts from row-major input to Armadillo's column-major format.
     *
     * @param data Flat float data in row-major order
     * @param num_rows Number of rows
     * @param num_cols Number of columns
     * @throws std::invalid_argument if data.size() != num_rows * num_cols
     */
    ArmadilloTensorStorage(std::vector<float> const & data,
                           std::size_t num_rows,
                           std::size_t num_cols);

    // ========== Direct Armadillo Access (zero-copy) ==========

    /**
     * @brief Get const reference to the underlying vector (1D only)
     * @throws std::logic_error if storage is not 1D
     */
    [[nodiscard]] arma::fvec const & vector() const;

    /**
     * @brief Get mutable reference to the underlying vector (1D only)
     * @throws std::logic_error if storage is not 1D
     */
    [[nodiscard]] arma::fvec & mutableVector();

    /**
     * @brief Get const reference to the underlying matrix (2D only)
     * @throws std::logic_error if storage is not 2D
     */
    [[nodiscard]] arma::fmat const & matrix() const;

    /**
     * @brief Get mutable reference to the underlying matrix (2D only)
     * @throws std::logic_error if storage is not 2D
     */
    [[nodiscard]] arma::fmat & mutableMatrix();

    /**
     * @brief Get const reference to the underlying cube (3D only)
     * @throws std::logic_error if storage is not 3D
     */
    [[nodiscard]] arma::fcube const & cube() const;

    /**
     * @brief Get mutable reference to the underlying cube (3D only)
     * @throws std::logic_error if storage is not 3D
     */
    [[nodiscard]] arma::fcube & mutableCube();

    /**
     * @brief Get the number of dimensions (1, 2, or 3)
     */
    [[nodiscard]] std::size_t ndim() const noexcept;

    // ========== CRTP Implementation ==========

    [[nodiscard]] float getValueAtImpl(std::span<std::size_t const> indices) const;
    [[nodiscard]] std::span<float const> flatDataImpl() const;
    [[nodiscard]] std::vector<float> sliceAlongAxisImpl(std::size_t axis, std::size_t index) const;
    [[nodiscard]] std::vector<float> getColumnImpl(std::size_t col) const;
    [[nodiscard]] std::vector<std::size_t> shapeImpl() const;
    [[nodiscard]] std::size_t totalElementsImpl() const;
    [[nodiscard]] bool isContiguousImpl() const noexcept { return true; }
    [[nodiscard]] TensorStorageType getStorageTypeImpl() const noexcept {
        return TensorStorageType::Armadillo;
    }
    [[nodiscard]] TensorStorageCache tryGetCacheImpl() const;

private:
    std::variant<arma::fvec, arma::fmat, arma::fcube> _data;

    /// Helper: get a human-readable description of the current dimensionality
    [[nodiscard]] std::string dimLabel() const;
};

#endif // ARMADILLO_TENSOR_STORAGE_HPP
