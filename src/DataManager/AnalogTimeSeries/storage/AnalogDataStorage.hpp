#ifndef ANALOG_DATA_STORAGE_HPP
#define ANALOG_DATA_STORAGE_HPP

#include "AnalogDataStorageBase.hpp"
#include "AnalogDataStorageCache.hpp"
#include "VectorAnalogDataStorage.hpp"
#include "ViewAnalogDataStorage.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <type_traits>
#include <vector>

/**
 * @brief Type-erased wrapper for analog data storage.
 *
 * Provides a uniform interface to different storage backends (vector, mmap, view, etc.)
 * while enabling compile-time optimizations through template instantiation.
 *
 * Uses shared_ptr internally to enable zero-copy view creation via aliasing constructor.
 * When creating a view from a VectorAnalogDataStorage, the view can share ownership
 * of the source data without copying.
 */
class AnalogDataStorageWrapper {
public:
    template<typename DataStorageImpl>
    explicit AnalogDataStorageWrapper(DataStorageImpl storage)
        : _impl(std::make_shared<StorageModel<DataStorageImpl>>(std::move(storage))) {}

    // Default constructor creates empty vector storage
    AnalogDataStorageWrapper()
        : _impl(std::make_shared<StorageModel<VectorAnalogDataStorage>>(
                  VectorAnalogDataStorage{std::vector<float>{}})) {}

    // Copy and move semantics - shared_ptr allows sharing
    AnalogDataStorageWrapper(AnalogDataStorageWrapper &&) noexcept = default;
    AnalogDataStorageWrapper & operator=(AnalogDataStorageWrapper &&) noexcept = default;
    AnalogDataStorageWrapper(AnalogDataStorageWrapper const &) = default;
    AnalogDataStorageWrapper & operator=(AnalogDataStorageWrapper const &) = default;

    [[nodiscard]] std::size_t size() const { return _impl->size(); }

    [[nodiscard]] float getValueAt(std::size_t index) const {
        return _impl->getValueAt(index);
    }

    [[nodiscard]] std::span<float const> getSpan() const {
        return _impl->getSpan();
    }

    [[nodiscard]] std::span<float const> getSpanRange(std::size_t start, std::size_t end) const {
        return _impl->getSpanRange(start, end);
    }

    [[nodiscard]] bool isContiguous() const {
        return _impl->isContiguous();
    }

    [[nodiscard]] AnalogStorageType getStorageType() const {
        return _impl->getStorageType();
    }

    [[nodiscard]] bool isView() const {
        return getStorageType() == AnalogStorageType::View;
    }

    [[nodiscard]] bool isLazy() const {
        return getStorageType() == AnalogStorageType::LazyView;
    }

    /**
     * @brief Try to get a cache for fast-path contiguous access.
     */
    [[nodiscard]] AnalogDataStorageCache tryGetCache() const {
        return _impl->tryGetCache();
    }

    /**
     * @brief Get shared pointer to vector storage for creating views.
     *
     * If this wrapper contains vector storage, uses aliasing constructor to share
     * ownership. If this wrapper contains a view, returns the view's existing source.
     * Returns nullptr for other storage types (mmap, lazy).
     *
     * @return shared_ptr to VectorAnalogDataStorage, or nullptr if not available.
     */
    [[nodiscard]] std::shared_ptr<VectorAnalogDataStorage const> getSharedVectorStorage() const {
        // Check if we have view storage - return its existing source
        if (auto const * view_model = dynamic_cast<StorageModel<ViewAnalogDataStorage> const *>(_impl.get())) {
            return view_model->_storage.source();
        }

        // Check if we have vector storage - use aliasing constructor for zero-copy sharing
        if (auto vector_model = std::dynamic_pointer_cast<StorageModel<VectorAnalogDataStorage> const>(_impl)) {
            // Aliasing constructor: shares ownership with _impl but points to the inner storage
            return std::shared_ptr<VectorAnalogDataStorage const>(vector_model, &vector_model->_storage);
        }

        // Other storage types (mmap, lazy) - no shared vector storage available
        return nullptr;
    }

private:
    struct StorageConcept {
        virtual ~StorageConcept() = default;
        virtual std::size_t size() const = 0;
        virtual float getValueAt(std::size_t index) const = 0;
        virtual std::span<float const> getSpan() const = 0;
        virtual std::span<float const> getSpanRange(std::size_t start, std::size_t end) const = 0;
        virtual bool isContiguous() const = 0;
        virtual AnalogStorageType getStorageType() const = 0;
        virtual AnalogDataStorageCache tryGetCache() const = 0;
    };

    template<typename DataStorageImpl>
    struct StorageModel : StorageConcept {
        DataStorageImpl _storage;

        explicit StorageModel(DataStorageImpl storage)
            : _storage(std::move(storage)) {}

        std::size_t size() const override {
            return _storage.size();
        }

        float getValueAt(std::size_t index) const override {
            return _storage.getValueAt(index);
        }

        std::span<float const> getSpan() const override {
            return _storage.getSpan();
        }

        std::span<float const> getSpanRange(std::size_t start, std::size_t end) const override {
            return _storage.getSpanRange(start, end);
        }

        bool isContiguous() const override {
            return _storage.isContiguous();
        }

        AnalogStorageType getStorageType() const override {
            return _storage.getStorageTypeImpl();
        }

        AnalogDataStorageCache tryGetCache() const override {
            if constexpr (requires { _storage.tryGetCacheImpl(); }) {
                return _storage.tryGetCacheImpl();
            } else {
                return AnalogDataStorageCache{};
            }
        }
    };

    std::shared_ptr<StorageConcept> _impl;
};

#endif// ANALOG_DATA_STORAGE_HPP

