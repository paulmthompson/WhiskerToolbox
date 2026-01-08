#ifndef BEHAVIORTOOLBOX_EVENT_WITH_ID_HPP
#define BEHAVIORTOOLBOX_EVENT_WITH_ID_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

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

#endif// BEHAVIORTOOLBOX_EVENT_WITH_ID_HPP