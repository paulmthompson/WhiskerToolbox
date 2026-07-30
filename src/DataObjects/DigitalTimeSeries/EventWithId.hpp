#ifndef BEHAVIORTOOLBOX_EVENT_WITH_ID_HPP
#define BEHAVIORTOOLBOX_EVENT_WITH_ID_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/ClockTicks.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

/**
 * @brief Structure to hold a digital event with its associated EntityID
 * 
 * Represents a single timestamped event in a DigitalEventSeries.
 * Satisfies the TimeSeriesElement and EntityElement concepts.
 * 
 * @see TimeSeriesConcepts.hpp for concept definitions
 */
struct EventWithId {
    TimeFrameIndex event_time;
    EntityId entity_id;

    EventWithId(TimeFrameIndex time, EntityId id)
        : event_time(time),
          entity_id(id) {}

    // ========== Standardized Accessors (for TimeSeriesElement/EntityElement concepts) ==========

    /**
     * @brief Get the time of this event
     * @return TimeFrameIndex The event timestamp
     */
    [[nodiscard]] constexpr TimeFrameIndex time() const noexcept { return event_time; }

    /**
     * @brief Get the EntityId of this event
     * @return EntityId The entity identifier
     */
    [[nodiscard]] constexpr EntityId id() const noexcept { return entity_id; }

    /**
     * @brief Get the value of this event (for events, the time IS the value)
     * @return TimeFrameIndex The event timestamp
     */
    [[nodiscard]] constexpr TimeFrameIndex value() const noexcept { return event_time; }
};

/**
 * @brief Structure to hold a clock-tick event with its associated EntityID
 *
 * API-facing element type for iteration and range queries on DigitalEventSeries
 * where event times are expressed in absolute physical time (ClockTicks).
 *
 * @see EventWithId for index-based storage element type
 */
struct ClockTicksWithId {
    ClockTicks event_time;
    EntityId entity_id;

    ClockTicksWithId(ClockTicks time, EntityId id)
        : event_time(time),
          entity_id(id) {}

    /**
     * @brief Get the time of this event in clock ticks
     */
    [[nodiscard]] ClockTicks time() const noexcept { return event_time; }

    /**
     * @brief Get the EntityId of this event
     */
    [[nodiscard]] constexpr EntityId id() const noexcept { return entity_id; }

    /**
     * @brief Get the clock-tick event time
     */
    [[nodiscard]] ClockTicks value() const noexcept { return event_time; }
};

#endif// BEHAVIORTOOLBOX_EVENT_WITH_ID_HPP