#ifndef DIGITAL_INTERVAL_STORAGE_CACHE_HPP
#define DIGITAL_INTERVAL_STORAGE_CACHE_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/interval_data.hpp"

/**
 * @brief Storage type enumeration for digital interval storage
 */
enum class DigitalIntervalStorageType {
    Owning,  ///< Owns the data in SoA layout
    View,    ///< References another storage via indices
    Lazy     ///< Lazy-evaluated transform
};

// =============================================================================
// Cache Optimization Structure
// =============================================================================

/**
 * @brief Cache structure for fast-path access to contiguous digital interval storage
 * 
 * Digital intervals store Interval values (start/end times) with associated
 * EntityIds. The storage is organized as parallel arrays:
 * - intervals[i] - Interval for entry i
 * - entity_ids[i] - EntityId for entry i
 * 
 * @note Digital intervals are always sorted by start time.
 */
struct DigitalIntervalStorageCache {
    Interval const* intervals_ptr = nullptr;
    EntityId const* entity_ids_ptr = nullptr;
    size_t cache_size = 0;
    bool is_contiguous = false;  ///< True if storage is contiguous (owning)
    
    /**
     * @brief Check if the cache is valid for fast-path access
     * 
     * @return true if storage is contiguous (can use direct pointer access)
     * @return false if storage is non-contiguous (must use virtual dispatch)
     */
    [[nodiscard]] constexpr bool isValid() const noexcept {
        return is_contiguous;
    }
    
    // Convenience accessors for cached data
    [[nodiscard]] Interval const& getInterval(size_t idx) const noexcept {
        return intervals_ptr[idx];
    }
    
    [[nodiscard]] EntityId getEntityId(size_t idx) const noexcept {
        return entity_ids_ptr[idx];
    }
};

#endif // DIGITAL_INTERVAL_STORAGE_CACHE_HPP