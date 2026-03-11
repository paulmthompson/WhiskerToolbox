/**
 * @file DigitalEventStorageCache.hpp
 * @brief Cache and enum types for digital event storage backends.
 */

#ifndef DIGITAL_EVENT_STORAGE_CACHE_HPP
#define DIGITAL_EVENT_STORAGE_CACHE_HPP

#include "Entity/EntityId.hpp"          // EntityId
#include "TimeFrame/TimeFrameIndex.hpp" // TimeFrameIndex

#include <cstddef>

/**
 * @brief Storage type enumeration for digital event storage.
 */
enum class DigitalEventStorageType {
    Owning,///< Owns the data in SoA layout
    View,  ///< References another storage via indices
    Lazy   ///< Lazy-evaluated transform
};

// =============================================================================
// Cache Optimization Structure
// =============================================================================

/**
 * @brief Cache structure for fast-path access to contiguous digital event storage.
 *
 * Digital events store TimeFrameIndex values (event times) with associated
 * EntityIds. The storage is organized as parallel arrays:
 * - events[i] - TimeFrameIndex for event i
 * - entity_ids[i] - EntityId for event i
 *
 * @note Digital events are always sorted by time.
 */
struct DigitalEventStorageCache {
    TimeFrameIndex const * events_ptr = nullptr;
    EntityId const * entity_ids_ptr = nullptr;
    size_t cache_size = 0;
    bool is_contiguous = false;///< True if storage is contiguous (owning)

    /**
     * @brief Check if the cache is valid for fast-path access.
     *
     * @return true if storage is contiguous (can use direct pointer access)
     * @return false if storage is non-contiguous (must use polymorphic access)
     */
    [[nodiscard]] constexpr bool isValid() const noexcept {
        return is_contiguous;
    }

    [[nodiscard]] TimeFrameIndex getEvent(size_t idx) const noexcept {
        return events_ptr[idx];
    }

    [[nodiscard]] EntityId getEntityId(size_t idx) const noexcept {
        return entity_ids_ptr[idx];
    }
};

#endif// DIGITAL_EVENT_STORAGE_CACHE_HPP
