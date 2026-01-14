#ifndef BEHAVIORTOOLBOX_INTERVAL_WITH_ID_HPP
#define BEHAVIORTOOLBOX_INTERVAL_WITH_ID_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/interval_data.hpp"

/**
 * @brief Structure to hold an interval with its associated EntityID
 * 
 * Represents a single time interval in a DigitalIntervalSeries.
 * Satisfies the TimeSeriesElement and EntityElement concepts.
 * 
 * Note: The `.time()` accessor returns the start time of the interval,
 * which is the canonical time point for this element. Use `.value()`
 * or `.interval` to access the full interval data.
 * 
 * @see TimeSeriesConcepts.hpp for concept definitions
 */
struct IntervalWithId {
    Interval interval;
    EntityId entity_id;

    IntervalWithId(Interval const & interval_data, EntityId id)
        : interval(interval_data),
          entity_id(id) {}

    // ========== Standardized Accessors (for TimeSeriesElement/EntityElement concepts) ==========

    /**
     * @brief Get the time of this interval (returns start time)
     * @return TimeFrameIndex The interval start time
     */
    [[nodiscard]] constexpr TimeFrameIndex time() const noexcept { 
        return TimeFrameIndex(interval.start); 
    }

    /**
     * @brief Get the EntityId of this interval
     * @return EntityId The entity identifier
     */
    [[nodiscard]] constexpr EntityId id() const noexcept { return entity_id; }

    /**
     * @brief Get the value of this interval (the Interval itself)
     * @return Interval const& The interval data
     */
    [[nodiscard]] constexpr Interval const& value() const noexcept { return interval; }
};

#endif// BEHAVIORTOOLBOX_INTERVAL_WITH_ID_HPP