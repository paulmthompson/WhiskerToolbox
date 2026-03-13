/**
 * @file DigitalEventStorageBase.hpp
 * @brief CRTP base for digital event storage backends.
 */

#ifndef DIGITAL_EVENT_STORAGE_BASE_HPP
#define DIGITAL_EVENT_STORAGE_BASE_HPP

#include "DigitalEventStorageCache.hpp"     // DigitalEventStorageCache

#include "Entity/EntityId.hpp"              // EntityId
#include "TimeFrame/TimeFrameIndex.hpp"     // TimeFrameIndex

#include <optional>
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
