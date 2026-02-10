#ifndef TENSOR_STORAGE_WRAPPER_HPP
#define TENSOR_STORAGE_WRAPPER_HPP

#include "TensorStorageBase.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

/**
 * @brief Sean Parent type-erasure wrapper for tensor storage backends
 *
 * Wraps any concrete storage that inherits from TensorStorageBase<Derived>
 * behind a uniform non-templated interface. This allows TensorData to hold
 * any storage backend (Armadillo, Dense, LibTorch, View, Lazy) without
 * exposing the CRTP type parameter to consumers.
 *
 * ## Pattern
 *
 * Uses the same Sean Parent type-erasure pattern as:
 * - AnalogTimeSeries::DataStorageWrapper (shared_ptr, copyable)
 * - RaggedStorageWrapper<TData> (unique_ptr, move-only)
 *
 * This wrapper uses `std::shared_ptr` to enable zero-copy view sharing:
 * when a ViewTensorStorage is created from a TensorStorageWrapper, both
 * share ownership of the underlying concept (via the shared_ptr aliasing
 * constructor or direct shared_ptr copy).
 *
 * ## Ownership
 *
 * The wrapper is **copyable** (shared ownership via shared_ptr).
 * - Copies share the same underlying storage (cheap, O(1))
 * - This enables ViewTensorStorage to reference the source without
 *   worrying about lifetime
 * - For exclusive ownership, create a new storage and wrap it
 *
 * ## Type Recovery
 *
 * `tryGetAs<ConcreteStorage>()` uses `dynamic_cast` on the internal
 * concept/model hierarchy to recover the concrete type. This is the
 * same approach as RaggedStorageWrapper::tryGet<T>() and enables
 * zero-copy backend-specific access (e.g., Armadillo matrix for mlpack,
 * torch::Tensor for model inference).
 *
 * ## Thread Safety
 *
 * Same as the underlying storage. The wrapper itself adds no
 * synchronization. shared_ptr reference counting is thread-safe,
 * but concurrent mutation of the pointee is not.
 */
class TensorStorageWrapper {
public:
    // ========== Construction ==========

    /**
     * @brief Construct from any concrete tensor storage implementation
     *
     * @tparam StorageImpl A type inheriting from TensorStorageBase<StorageImpl>
     * @param impl The concrete storage to wrap (moved into the wrapper)
     */
    template<typename StorageImpl>
    explicit TensorStorageWrapper(StorageImpl impl);

    /**
     * @brief Default constructor — creates an empty wrapper
     *
     * The wrapper is in a "null" state. All accessor methods will throw
     * std::runtime_error if called. Use `isValid()` to check.
     */
    TensorStorageWrapper() = default;

    /// Copy constructor (shared ownership, O(1))
    TensorStorageWrapper(TensorStorageWrapper const &) = default;
    /// Copy assignment (shared ownership, O(1))
    TensorStorageWrapper & operator=(TensorStorageWrapper const &) = default;
    /// Move constructor
    TensorStorageWrapper(TensorStorageWrapper &&) noexcept = default;
    /// Move assignment
    TensorStorageWrapper & operator=(TensorStorageWrapper &&) noexcept = default;

    ~TensorStorageWrapper() = default;

    // ========== Validity ==========

    /**
     * @brief Check whether the wrapper holds a valid storage backend
     * @return true if a storage backend is present, false if default-constructed
     */
    [[nodiscard]] bool isValid() const noexcept;

    // ========== Delegated Interface (mirrors TensorStorageBase) ==========

    /**
     * @brief Get a single float value by multi-dimensional index
     * @see TensorStorageBase::getValueAt
     */
    [[nodiscard]] float getValueAt(std::span<std::size_t const> indices) const;

    /**
     * @brief Get a span over the flat (row-major) data
     * @see TensorStorageBase::flatData
     */
    [[nodiscard]] std::span<float const> flatData() const;

    /**
     * @brief Extract a sub-tensor by fixing one axis to a single index
     * @see TensorStorageBase::sliceAlongAxis
     */
    [[nodiscard]] std::vector<float> sliceAlongAxis(std::size_t axis, std::size_t index) const;

    /**
     * @brief Get a single column (axis 1 slice) across all rows
     * @see TensorStorageBase::getColumn
     */
    [[nodiscard]] std::vector<float> getColumn(std::size_t col) const;

    /**
     * @brief Get the shape as a vector of sizes (one per axis)
     * @see TensorStorageBase::shape
     */
    [[nodiscard]] std::vector<std::size_t> shape() const;

    /**
     * @brief Get the total number of elements (product of shape)
     * @see TensorStorageBase::totalElements
     */
    [[nodiscard]] std::size_t totalElements() const;

    /**
     * @brief Whether the underlying data is contiguous in memory
     * @see TensorStorageBase::isContiguous
     */
    [[nodiscard]] bool isContiguous() const;

    /**
     * @brief Get the storage type identifier
     * @see TensorStorageBase::getStorageType
     */
    [[nodiscard]] TensorStorageType getStorageType() const;

    /**
     * @brief Try to get cached pointers for fast-path access
     * @see TensorStorageBase::tryGetCache
     */
    [[nodiscard]] TensorStorageCache tryGetCache() const;

    // ========== Backend-Specific Access (Type Recovery) ==========

    /**
     * @brief Attempt to retrieve the concrete storage implementation
     *
     * Uses dynamic_cast to test whether the erased storage is of type T.
     * Returns a const pointer to the concrete storage, or nullptr if the
     * backend is a different type.
     *
     * Usage:
     * @code
     * if (auto* arma = wrapper.tryGetAs<ArmadilloTensorStorage>()) {
     *     arma::fmat const& m = arma->matrix();
     *     // zero-copy Armadillo access
     * }
     * @endcode
     *
     * @tparam T The concrete storage type to cast to
     * @return Pointer to the concrete storage, or nullptr if wrong type
     */
    template<typename T>
    [[nodiscard]] T const * tryGetAs() const;

    /**
     * @brief Mutable version of tryGetAs
     *
     * Returns a non-const pointer to the concrete storage. Use with care —
     * mutation through this pointer bypasses any observer notifications.
     *
     * @tparam T The concrete storage type to cast to
     * @return Pointer to the concrete storage, or nullptr if wrong type
     */
    template<typename T>
    [[nodiscard]] T * tryGetMutableAs();

    // ========== Shared Ownership Access ==========

    /**
     * @brief Get a shared_ptr to the internal concept (for ViewTensorStorage)
     *
     * ViewTensorStorage needs to share ownership of the source storage to
     * prevent dangling references. This method exposes the internal shared_ptr
     * for that purpose.
     *
     * @return shared_ptr to the internal StorageConcept (may be null)
     */
    [[nodiscard]] std::shared_ptr<void const> sharedStorage() const;

private:
    // ========== Sean Parent Type-Erasure Machinery ==========

    /**
     * @brief Abstract interface for all storage backends (virtual dispatch)
     *
     * Every method in TensorStorageBase has a corresponding pure virtual here.
     */
    struct StorageConcept {
        virtual ~StorageConcept() = default;

        [[nodiscard]] virtual float getValueAt(std::span<std::size_t const> indices) const = 0;
        [[nodiscard]] virtual std::span<float const> flatData() const = 0;
        [[nodiscard]] virtual std::vector<float> sliceAlongAxis(
            std::size_t axis, std::size_t index) const = 0;
        [[nodiscard]] virtual std::vector<float> getColumn(std::size_t col) const = 0;
        [[nodiscard]] virtual std::vector<std::size_t> shape() const = 0;
        [[nodiscard]] virtual std::size_t totalElements() const = 0;
        [[nodiscard]] virtual bool isContiguous() const = 0;
        [[nodiscard]] virtual TensorStorageType getStorageType() const = 0;
        [[nodiscard]] virtual TensorStorageCache tryGetCache() const = 0;
    };

    /**
     * @brief Concrete model holding a storage implementation of type T
     *
     * Forwards all virtual calls to the CRTP methods on T.
     *
     * @tparam T A concrete type inheriting TensorStorageBase<T>
     */
    template<typename T>
    struct StorageModel final : StorageConcept {
        explicit StorageModel(T storage) : _storage(std::move(storage)) {}

        [[nodiscard]] float getValueAt(std::span<std::size_t const> indices) const override {
            return _storage.getValueAt(indices);
        }
        [[nodiscard]] std::span<float const> flatData() const override {
            return _storage.flatData();
        }
        [[nodiscard]] std::vector<float> sliceAlongAxis(
            std::size_t axis, std::size_t index) const override {
            return _storage.sliceAlongAxis(axis, index);
        }
        [[nodiscard]] std::vector<float> getColumn(std::size_t col) const override {
            return _storage.getColumn(col);
        }
        [[nodiscard]] std::vector<std::size_t> shape() const override {
            return _storage.shape();
        }
        [[nodiscard]] std::size_t totalElements() const override {
            return _storage.totalElements();
        }
        [[nodiscard]] bool isContiguous() const override {
            return _storage.isContiguous();
        }
        [[nodiscard]] TensorStorageType getStorageType() const override {
            return _storage.getStorageType();
        }
        [[nodiscard]] TensorStorageCache tryGetCache() const override {
            return _storage.tryGetCache();
        }

        T _storage; // The actual concrete storage
    };

    std::shared_ptr<StorageConcept> _impl;

    /// Throw if the wrapper is in a null state
    void ensureValid() const;
};

// =============================================================================
// Template Implementations (must be in header)
// =============================================================================

template<typename StorageImpl>
TensorStorageWrapper::TensorStorageWrapper(StorageImpl impl)
    : _impl(std::make_shared<StorageModel<StorageImpl>>(std::move(impl)))
{
}

template<typename T>
T const * TensorStorageWrapper::tryGetAs() const
{
    if (!_impl) {
        return nullptr;
    }
    auto const * model = dynamic_cast<StorageModel<T> const *>(_impl.get());
    if (model == nullptr) {
        return nullptr;
    }
    return &model->_storage;
}

template<typename T>
T * TensorStorageWrapper::tryGetMutableAs()
{
    if (!_impl) {
        return nullptr;
    }
    auto * model = dynamic_cast<StorageModel<T> *>(_impl.get());
    if (model == nullptr) {
        return nullptr;
    }
    return &model->_storage;
}

#endif // TENSOR_STORAGE_WRAPPER_HPP
