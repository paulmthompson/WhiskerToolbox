/**
 * @file DigitalEventStorageCache.hpp
 * @brief Cache and enum types for digital event storage backends.
 */

#ifndef DIGITAL_EVENT_STORAGE_CACHE_HPP
#define DIGITAL_EVENT_STORAGE_CACHE_HPP

#include "Entity/EntityId.hpp"         // EntityId
#include "TimeFrame/ClockTicks.hpp"    // ClockTicks
#include "TimeFrame/TimeFrameIndex.hpp"// TimeFrameIndex

#include <cassert>
#include <cstddef>

/**
 * @brief Time coordinate domain for digital event storage.
 */
enum class DigitalEventTimeDomain {
    TimeFrameIndex,   ///< Event times stored as TimeFrameIndex (absolute index space)
    RelativeClockTicks///< Event times stored as relative ClockTicks
};

/**
 * @brief Storage type enumeration for digital event storage.
 */
enum class DigitalEventStorageType {
    Owning,       ///< Owns the data in SoA layout
    View,         ///< References another storage via indices
    Lazy,         ///< Lazy-evaluated transform
    RelativeOwning///< Immutable owning storage of relative ClockTicks
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
    DigitalEventTimeDomain time_domain = DigitalEventTimeDomain::TimeFrameIndex;
    TimeFrameIndex const * events_ptr = nullptr;
    ClockTicks const * relative_events_ptr = nullptr;
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
        assert(time_domain == DigitalEventTimeDomain::TimeFrameIndex);
        return events_ptr[idx];
    }

    [[nodiscard]] ClockTicks getRelativeEvent(size_t idx) const noexcept {
        assert(time_domain == DigitalEventTimeDomain::RelativeClockTicks);
        return relative_events_ptr[idx];
    }

    [[nodiscard]] EntityId getEntityId(size_t idx) const noexcept {
        return entity_ids_ptr[idx];
    }
};

#endif// DIGITAL_EVENT_STORAGE_CACHE_HPP
