#ifndef DIGITAL_INTERVAL_STORAGE_BASE_HPP
#define DIGITAL_INTERVAL_STORAGE_BASE_HPP

#include "DigitalIntervalStorageCache.hpp"

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <optional>


// =============================================================================
// CRTP Base Class
// =============================================================================

/**
 * @brief CRTP base class for digital interval storage implementations
 * 
 * Uses Curiously Recurring Template Pattern to eliminate virtual function overhead
 * while maintaining a polymorphic interface. Intervals are stored as Interval
 * values (representing start/end times) with associated EntityIds.
 * 
 * The SoA (Structure of Arrays) layout stores parallel vectors:
 * - Interval intervals[] - sorted by start time
 * - EntityId entity_ids[] - corresponding entity identifiers
 * 
 * @tparam Derived The concrete storage implementation type
 */
template<typename Derived>
class DigitalIntervalStorageBase {
public:
    // ========== Size & Bounds ==========
    
    /**
     * @brief Get total number of intervals
     */
    [[nodiscard]] size_t size() const {
        return static_cast<Derived const*>(this)->sizeImpl();
    }
    
    /**
     * @brief Check if storage is empty
     */
    [[nodiscard]] bool empty() const {
        return size() == 0;
    }

    // ========== Element Access ==========
    
    /**
     * @brief Get the interval at a flat index
     * @param idx Flat index in [0, size())
     */
    [[nodiscard]] Interval const& getInterval(size_t idx) const {
        return static_cast<Derived const*>(this)->getIntervalImpl(idx);
    }
    
    /**
     * @brief Get the EntityId at a flat index
     * @param idx Flat index in [0, size())
     */
    [[nodiscard]] EntityId getEntityId(size_t idx) const {
        return static_cast<Derived const*>(this)->getEntityIdImpl(idx);
    }

    // ========== Lookup Operations ==========
    
    /**
     * @brief Find the index of an interval by its exact start/end times
     * @param interval The exact Interval to find
     * @return Index of the interval, or std::nullopt if not found
     */
    [[nodiscard]] std::optional<size_t> findByInterval(Interval const& interval) const {
        return static_cast<Derived const*>(this)->findByIntervalImpl(interval);
    }
    
    /**
     * @brief Find the index of an interval by its EntityId
     * @param id The EntityId to search for
     * @return Index of the interval, or std::nullopt if not found
     */
    [[nodiscard]] std::optional<size_t> findByEntityId(EntityId id) const {
        return static_cast<Derived const*>(this)->findByEntityIdImpl(id);
    }
    
    /**
     * @brief Check if any interval contains the specified time
     * @param time The time to check
     * @return true if any interval contains the time
     */
    [[nodiscard]] bool hasIntervalAtTime(int64_t time) const {
        return static_cast<Derived const*>(this)->hasIntervalAtTimeImpl(time);
    }
    
    /**
     * @brief Get range of indices for intervals overlapping [start, end]
     * @param start Start time (inclusive)
     * @param end End time (inclusive)
     * @return Pair of (start_idx, end_idx) where end is exclusive
     * 
     * @note Returns intervals where interval.start <= end && interval.end >= start
     */
    [[nodiscard]] std::pair<size_t, size_t> getOverlappingRange(int64_t start, int64_t end) const {
        return static_cast<Derived const*>(this)->getOverlappingRangeImpl(start, end);
    }
    
    /**
     * @brief Get range of indices for intervals fully contained in [start, end]
     * @param start Start time (inclusive)
     * @param end End time (inclusive)
     * @return Pair of (start_idx, end_idx) where end is exclusive
     * 
     * @note Returns intervals where interval.start >= start && interval.end <= end
     */
    [[nodiscard]] std::pair<size_t, size_t> getContainedRange(int64_t start, int64_t end) const {
        return static_cast<Derived const*>(this)->getContainedRangeImpl(start, end);
    }

    // ========== Storage Type ==========
    
    /**
     * @brief Get the storage type identifier
     */
    [[nodiscard]] DigitalIntervalStorageType getStorageType() const {
        return static_cast<Derived const*>(this)->getStorageTypeImpl();
    }
    
    /**
     * @brief Check if this is a view (doesn't own data)
     */
    [[nodiscard]] bool isView() const {
        return getStorageType() == DigitalIntervalStorageType::View;
    }
    
    /**
     * @brief Check if this is lazy storage
     */
    [[nodiscard]] bool isLazy() const {
        return getStorageType() == DigitalIntervalStorageType::Lazy;
    }

    // ========== Cache Optimization ==========
    
    /**
     * @brief Try to get cached pointers for fast-path access
     * 
     * @return DigitalIntervalStorageCache with valid pointers if contiguous, invalid otherwise
     */
    [[nodiscard]] DigitalIntervalStorageCache tryGetCache() const {
        return static_cast<Derived const*>(this)->tryGetCacheImpl();
    }

protected:
    ~DigitalIntervalStorageBase() = default;
};


#endif // DIGITAL_INTERVAL_STORAGE_BASE_HPP