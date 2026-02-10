#ifndef TENSOR_STORAGE_BASE_HPP
#define TENSOR_STORAGE_BASE_HPP

#include <cstddef>
#include <span>
#include <vector>

/**
 * @brief Storage type enumeration for tensor storage backends
 *
 * Each concrete tensor storage implementation reports its type via
 * getStorageType(). This is used for logging, debugging, and by the
 * type-erasure wrapper's tryGetAs<T>() method.
 */
enum class TensorStorageType {
    Armadillo, ///< arma::fvec / arma::fmat / arma::fcube (≤3D, always available)
    Dense,     ///< Flat std::vector<float> + shape (>3D fallback)
    LibTorch,  ///< torch::Tensor (optional, behind #ifdef)
    View,      ///< Zero-copy slice of another storage
    Lazy       ///< Lazily computed columns (transforms v2 pipelines)
};

/**
 * @brief Cache structure for fast-path access to tensor storage
 *
 * Provides raw pointers and stride information for consumers that can
 * exploit contiguous layout (plotting, bulk export, Armadillo/LibTorch
 * interop). Follows the same cache pattern as RaggedAnalogStorageCache
 * and AnalogTimeSeries' storage cache.
 *
 * Usage:
 * @code
 * auto cache = storage.tryGetCache();
 * if (cache.isValid()) {
 *     // Fast path: direct pointer access with strides
 *     for (size_t i = 0; i < cache.total_elements; ++i) {
 *         float val = cache.data_ptr[i];
 *     }
 * } else {
 *     // Slow path: element-by-element via virtual dispatch
 * }
 * @endcode
 */
struct TensorStorageCache {
    float const * data_ptr = nullptr;       ///< Raw pointer to contiguous float data
    std::size_t total_elements = 0;         ///< Total number of elements
    std::vector<std::size_t> shape;         ///< Shape of the tensor
    std::vector<std::size_t> strides;       ///< Row-major strides (elements, not bytes)
    bool is_valid = false;                  ///< True if contiguous and data_ptr is usable

    /**
     * @brief Check if cache is usable for fast-path access
     */
    [[nodiscard]] constexpr bool isValid() const noexcept { return is_valid; }
};

/**
 * @brief CRTP base class for tensor storage implementations
 *
 * Uses the Curiously Recurring Template Pattern to eliminate virtual function
 * overhead while maintaining a polymorphic interface. All concrete storage
 * backends (Armadillo, Dense, LibTorch, View, Lazy) inherit from this and
 * implement the `*Impl()` methods.
 *
 * The interface is read-oriented. Mutation is handled at the TensorData level,
 * which may replace the entire storage wrapper. This matches the project
 * convention where views are immutable (same as ViewAnalogDataStorage,
 * ViewRaggedStorage).
 *
 * ## Row-major contract
 *
 * All storage backends present a **row-major** interface to consumers:
 * - `flatData()` returns data in row-major order
 * - `getValueAt(indices)` interprets indices in row-major order
 * - `getColumn(col)` returns column `col` across all rows (axis 0)
 *
 * Backends that store data in a different layout (e.g., Armadillo is
 * column-major) handle the translation internally.
 *
 * @tparam Derived The concrete storage implementation type
 */
template<typename Derived>
class TensorStorageBase {
public:
    // ========== Element Access ==========

    /**
     * @brief Get a single float value by multi-dimensional index
     * @param indices One index per dimension, in row-major order
     * @return The float value at that position
     * @throws std::out_of_range if any index is out of bounds
     * @throws std::invalid_argument if indices.size() != ndim
     */
    [[nodiscard]] float getValueAt(std::span<std::size_t const> indices) const {
        return self().getValueAtImpl(indices);
    }

    // ========== Bulk Access ==========

    /**
     * @brief Get a span over the flat (row-major) data
     *
     * Only available for contiguous storage backends. Non-contiguous backends
     * (views, lazy) throw std::runtime_error.
     *
     * @return Span over the underlying float buffer
     * @throws std::runtime_error if storage is not contiguous
     */
    [[nodiscard]] std::span<float const> flatData() const {
        return self().flatDataImpl();
    }

    // ========== Slicing ==========

    /**
     * @brief Extract a sub-tensor by fixing one axis to a single index
     *
     * Returns a new contiguous vector of floats. For example, on a 3D tensor
     * with shape [T, C, F]:
     * - sliceAlongAxis(0, 5) returns the 2D slice at time=5 (shape [C, F])
     * - sliceAlongAxis(1, 2) returns all time/frequency data for channel 2
     *
     * The returned data is always in row-major order for the remaining axes.
     *
     * @param axis Which axis to slice along
     * @param index The index along that axis
     * @return Flat vector of floats for the sub-tensor
     * @throws std::out_of_range if axis >= ndim or index >= shape[axis]
     */
    [[nodiscard]] std::vector<float> sliceAlongAxis(
        std::size_t axis, std::size_t index) const {
        return self().sliceAlongAxisImpl(axis, index);
    }

    /**
     * @brief Get a single column (axis 1 slice) across all rows (axis 0)
     *
     * Equivalent to sliceAlongAxis(1, col) for 2D tensors, but optimized
     * for the common case of extracting named feature columns.
     *
     * For tensors with >2 dimensions, this extracts along axis 1 and
     * flattens the remaining trailing dimensions.
     *
     * @param col Column index
     * @return Vector of floats (one per row for 2D, flattened for higher dims)
     * @throws std::out_of_range if col >= shape[1]
     * @throws std::invalid_argument if tensor is 0D or 1D (no column axis)
     */
    [[nodiscard]] std::vector<float> getColumn(std::size_t col) const {
        return self().getColumnImpl(col);
    }

    // ========== Metadata ==========

    /**
     * @brief Get the shape as a vector of sizes (one per axis)
     */
    [[nodiscard]] std::vector<std::size_t> shape() const {
        return self().shapeImpl();
    }

    /**
     * @brief Get the total number of elements (product of shape)
     */
    [[nodiscard]] std::size_t totalElements() const {
        return self().totalElementsImpl();
    }

    /**
     * @brief Whether the underlying data is contiguous in memory
     *
     * When true, flatData() and tryGetCache() are available.
     * When false, element access goes through getValueAt() or getColumn().
     */
    [[nodiscard]] bool isContiguous() const {
        return self().isContiguousImpl();
    }

    // ========== Storage Type ==========

    /**
     * @brief Get the storage type identifier
     */
    [[nodiscard]] TensorStorageType getStorageType() const {
        return self().getStorageTypeImpl();
    }

    // ========== Cache Optimization ==========

    /**
     * @brief Try to get cached pointers for fast-path access
     *
     * @return TensorStorageCache with valid=true if contiguous, invalid otherwise
     */
    [[nodiscard]] TensorStorageCache tryGetCache() const {
        return self().tryGetCacheImpl();
    }

protected:
    ~TensorStorageBase() = default;

private:
    [[nodiscard]] Derived const & self() const {
        return static_cast<Derived const &>(*this);
    }
    [[nodiscard]] Derived & self() {
        return static_cast<Derived &>(*this);
    }
};

#endif // TENSOR_STORAGE_BASE_HPP
