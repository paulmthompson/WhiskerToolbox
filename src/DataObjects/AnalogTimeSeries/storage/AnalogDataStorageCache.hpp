/**
 * @file AnalogDataStorageCache.hpp
 * @brief Cache structure and storage type enum for analog data storage backends.
 */

#ifndef ANALOG_DATA_STORAGE_CACHE_HPP
#define ANALOG_DATA_STORAGE_CACHE_HPP

#include <cstddef> // size_t

/**
 * @brief Storage type enumeration for runtime type identification.
 */
enum class AnalogStorageType {
    Vector,     ///< Contiguous in-memory vector storage
    MemoryMapped,///< Memory-mapped file storage
    View,       ///< View-based storage referencing another series
    LazyView,   ///< Lazy view-based storage (computed on demand)
    Custom      ///< Custom user-defined storage type
};

/**
 * @brief Cache structure for fast-path access to contiguous analog storage.
 *
 * Analog samples are stored as contiguous `float` values when possible. This cache
 * exposes a raw pointer and size for zero-overhead iteration when the underlying
 * storage is contiguous. For non-contiguous backends (lazy views, strided mmap),
 * the cache is marked invalid and callers must fall back to virtual dispatch.
 */
struct AnalogDataStorageCache {
    float const * data_ptr = nullptr;
    std::size_t cache_size = 0;
    bool is_contiguous = false;

    /**
     * @brief Check if the cache is valid for fast-path access.
     *
     * @return true if underlying storage is contiguous and data_ptr is non-null.
     * @return false otherwise.
     */
    [[nodiscard]] constexpr bool isValid() const noexcept {
        return is_contiguous && data_ptr != nullptr && cache_size > 0;
    }

    /**
     * @brief Convenience accessor for cached data.
     *
     * @param idx Index into the cached data in [0, cache_size).
     * @return const reference to the value at the given index.
     */
    [[nodiscard]] float const & getValue(std::size_t idx) const noexcept {
        return data_ptr[idx];
    }
};

#endif// ANALOG_DATA_STORAGE_CACHE_HPP

