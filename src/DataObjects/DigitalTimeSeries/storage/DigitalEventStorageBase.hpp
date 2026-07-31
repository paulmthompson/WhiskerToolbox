/**
 * @file DigitalEventStorageBase.hpp
 * @brief CRTP base for digital event storage backends.
 */

#ifndef DIGITAL_EVENT_STORAGE_BASE_HPP
#define DIGITAL_EVENT_STORAGE_BASE_HPP

#include "DigitalEventStorageCache.hpp"// DigitalEventStorageCache

#include "Entity/EntityId.hpp"         // EntityId
#include "TimeFrame/ClockTicks.hpp"    // ClockTicks
#include "TimeFrame/TimeFrameIndex.hpp"// TimeFrameIndex

#include <optional>
#include <stdexcept>
#include <utility>

// =============================================================================
// CRTP Base Class
// =============================================================================

/**
 * @brief CRTP base class for digital event storage implementations.
 *
 * Uses Curiously Recurring Template Pattern to eliminate virtual function
 * overhead while maintaining a polymorphic-like interface. Events are stored
 * as TimeFrameIndex values (representing event times) with associated EntityIds.
 *
 * The SoA (Structure of Arrays) layout stores parallel vectors:
 * - TimeFrameIndex events[] - sorted event times
 * - EntityId entity_ids[]   - corresponding entity identifiers
 *
 * @tparam Derived The concrete storage implementation type.
 */
template<typename Derived>
class DigitalEventStorageBase {
public:
    // ========== Size & Bounds ==========

    /**
     * @brief Get total number of events.
     */
    [[nodiscard]] size_t size() const {
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
     * @brief Get the event time at a flat index.
     * @param idx Flat index in [0, size()).
     */
    [[nodiscard]] TimeFrameIndex getEvent(size_t idx) const {
        return static_cast<Derived const *>(this)->getEventImpl(idx);
    }

    /**
     * @brief Get the EntityId at a flat index.
     * @param idx Flat index in [0, size()).
     */
    [[nodiscard]] EntityId getEntityId(size_t idx) const {
        return static_cast<Derived const *>(this)->getEntityIdImpl(idx);
    }

    // ========== Lookup Operations ==========

    /**
     * @brief Find the index of an event by its exact time.
     * @param time The exact TimeFrameIndex to find.
     * @return Index of the event, or std::nullopt if not found.
     */
    [[nodiscard]] std::optional<size_t> findByTime(TimeFrameIndex time) const {
        return static_cast<Derived const *>(this)->findByTimeImpl(time);
    }

    /**
     * @brief Find the index of an event by its EntityId.
     * @param id The EntityId to search for.
     * @return Index of the event, or std::nullopt if not found.
     */
    [[nodiscard]] std::optional<size_t> findByEntityId(EntityId id) const {
        return static_cast<Derived const *>(this)->findByEntityIdImpl(id);
    }

    /**
     * @brief Check if an event exists at a specific time.
     */
    [[nodiscard]] bool hasEventAtTime(TimeFrameIndex time) const {
        return findByTime(time).has_value();
    }

    /**
     * @brief Get range of indices for events in [start, end] inclusive.
     * @param start Start time (inclusive).
     * @param end   End time (inclusive).
     * @return Pair of (start_idx, end_idx) where end is exclusive.
     */
    [[nodiscard]] std::pair<size_t, size_t> getTimeRange(TimeFrameIndex start, TimeFrameIndex end) const {
        return static_cast<Derived const *>(this)->getTimeRangeImpl(start, end);
    }

    // ========== Relative Time Access ==========

    /**
     * @brief Get the time coordinate domain for this storage.
     */
    [[nodiscard]] DigitalEventTimeDomain getTimeDomain() const {
        return static_cast<Derived const *>(this)->getTimeDomainImpl();
    }

    /**
     * @brief Check if events are stored as relative ClockTicks.
     */
    [[nodiscard]] bool isRelative() const {
        return getTimeDomain() == DigitalEventTimeDomain::RelativeClockTicks;
    }

    /**
     * @brief Get the relative event time at a flat index.
     * @pre getTimeDomain() == DigitalEventTimeDomain::RelativeClockTicks
     * @param idx Flat index in [0, size()).
     */
    [[nodiscard]] ClockTicks getRelativeEvent(size_t idx) const {
        if constexpr (requires(Derived const & d, size_t i) { d.getRelativeEventImpl(i); }) {
            return static_cast<Derived const *>(this)->getRelativeEventImpl(idx);
        }
        throw std::runtime_error("getRelativeEvent() not supported for absolute-time storage");
    }

    /**
     * @brief Find the index of an event by its exact relative time.
     * @param time The exact ClockTicks to find.
     * @return Index of the event, or std::nullopt if not found.
     */
    [[nodiscard]] std::optional<size_t> findByRelativeTime(ClockTicks time) const {
        if constexpr (requires(Derived const & d, ClockTicks t) { d.findByRelativeTimeImpl(t); }) {
            return static_cast<Derived const *>(this)->findByRelativeTimeImpl(time);
        }
        throw std::runtime_error("findByRelativeTime() not supported for absolute-time storage");
    }

    /**
     * @brief Get range of indices for events in [start, end] inclusive (relative time).
     * @param start Start time (inclusive).
     * @param end   End time (inclusive).
     * @return Pair of (start_idx, end_idx) where end is exclusive.
     */
    [[nodiscard]] std::pair<size_t, size_t> getRelativeTimeRange(ClockTicks start, ClockTicks end) const {
        if constexpr (requires(Derived const & d, ClockTicks s, ClockTicks e) {
                          d.getRelativeTimeRangeImpl(s, e);
                      }) {
            return static_cast<Derived const *>(this)->getRelativeTimeRangeImpl(start, end);
        }
        throw std::runtime_error("getRelativeTimeRange() not supported for absolute-time storage");
    }

    // ========== Storage Type ==========

    /**
     * @brief Get the storage type identifier.
     */
    [[nodiscard]] DigitalEventStorageType getStorageType() const {
        return static_cast<Derived const *>(this)->getStorageTypeImpl();
    }

    /**
     * @brief Check if this is a view (doesn't own data).
     */
    [[nodiscard]] bool isView() const {
        return getStorageType() == DigitalEventStorageType::View;
    }

    /**
     * @brief Check if this is lazy storage.
     */
    [[nodiscard]] bool isLazy() const {
        return getStorageType() == DigitalEventStorageType::Lazy;
    }

    /**
     * @brief Check if this is relative owning storage.
     */
    [[nodiscard]] bool isRelativeOwning() const {
        return getStorageType() == DigitalEventStorageType::RelativeOwning;
    }

    // ========== Cache Optimization ==========

    /**
     * @brief Try to get cached pointers for fast-path access.
     *
     * @return DigitalEventStorageCache with valid pointers if contiguous, invalid otherwise.
     */
    [[nodiscard]] DigitalEventStorageCache tryGetCache() const {
        return static_cast<Derived const *>(this)->tryGetCacheImpl();
    }

protected:
    ~DigitalEventStorageBase() = default;
};

#endif// DIGITAL_EVENT_STORAGE_BASE_HPP
