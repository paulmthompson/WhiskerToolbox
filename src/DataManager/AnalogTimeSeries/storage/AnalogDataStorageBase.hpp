/**
 * @file AnalogDataStorageBase.hpp
 * @brief CRTP base class for analog data storage backends.
 */

#ifndef ANALOG_DATA_STORAGE_BASE_HPP
#define ANALOG_DATA_STORAGE_BASE_HPP

#include "AnalogDataStorageCache.hpp"

#include <span>

/**
 * @brief CRTP base class for analog data storage implementations.
 *
 * Uses the Curiously Recurring Template Pattern to eliminate virtual function
 * overhead while still providing a uniform interface that higher-level code
 * (such as the type-erased wrapper) can rely on.
 *
 * @tparam Derived The concrete storage implementation type.
 */
template<typename Derived>
class AnalogDataStorageBase {
public:
    // ========== Size & Bounds ==========

    /**
     * @brief Get the number of samples in storage.
     */
    [[nodiscard]] std::size_t size() const {
        return static_cast<Derived const *>(this)->sizeImpl();
    }

    /**
     * @brief Check if storage is empty.
     */
    [[nodiscard]] bool empty() const {
        return size() == 0;
    }

    // ========== Element Access ==========

    /**
     * @brief Get the value at a specific index.
     *
     * @param index Index into [0, size()).
     * @return float value at the given index.
     */
    [[nodiscard]] float getValueAt(std::size_t index) const {
        return static_cast<Derived const *>(this)->getValueAtImpl(index);
    }

    /**
     * @brief Get a span over all data (if contiguous).
     *
     * Returns a non-empty span only if the underlying storage is truly
     * contiguous in memory. Non-contiguous storage returns an empty span.
     */
    [[nodiscard]] std::span<float const> getSpan() const {
        return static_cast<Derived const *>(this)->getSpanImpl();
    }

    /**
     * @brief Get a span over a sub-range of data (if contiguous).
     *
     * @param start Starting index (inclusive).
     * @param end Ending index (exclusive).
     * @return std::span<float const> over the requested range, or empty span
     *         if storage is not contiguous.
     */
    [[nodiscard]] std::span<float const> getSpanRange(std::size_t start, std::size_t end) const {
        return static_cast<Derived const *>(this)->getSpanRangeImpl(start, end);
    }

    /**
     * @brief Check if data is stored contiguously in memory.
     */
    [[nodiscard]] bool isContiguous() const {
        return static_cast<Derived const *>(this)->isContiguousImpl();
    }

    // ========== Storage Type ==========

    /**
     * @brief Get the storage type identifier.
     */
    [[nodiscard]] AnalogStorageType getStorageType() const {
        return static_cast<Derived const *>(this)->getStorageTypeImpl();
    }

    /**
     * @brief Check if this storage is a view (non-owning).
     */
    [[nodiscard]] bool isView() const {
        return getStorageType() == AnalogStorageType::View;
    }

    /**
     * @brief Check if this storage is lazy-evaluated.
     */
    [[nodiscard]] bool isLazy() const {
        return getStorageType() == AnalogStorageType::LazyView;
    }

    // ========== Cache Optimization ==========

    /**
     * @brief Try to get a cache for fast-path contiguous access.
     *
     * @return AnalogDataStorageCache with valid pointer if storage is contiguous,
     *         invalid cache otherwise.
     */
    [[nodiscard]] AnalogDataStorageCache tryGetCache() const {
        return static_cast<Derived const *>(this)->tryGetCacheImpl();
    }

protected:
    ~AnalogDataStorageBase() = default;
};

#endif// ANALOG_DATA_STORAGE_BASE_HPP

