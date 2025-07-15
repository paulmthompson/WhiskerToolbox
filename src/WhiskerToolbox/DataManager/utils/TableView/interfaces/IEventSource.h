#ifndef IEVENT_SOURCE_H
#define IEVENT_SOURCE_H

#include "TimeFrame.hpp"


#include <span>

/**
 * @brief Interface for data sources that consist of sorted event timestamps/indices.
 * 
 * This interface is designed for data that represents discrete events in time,
 * such as digital event series or spike trains. The events are assumed to be
 * sorted in ascending order.
 */
class IEventSource {
public:
    virtual ~IEventSource() = default;
    
    // Make this class non-copyable and non-movable since it's a pure interface
    IEventSource(const IEventSource&) = delete;
    IEventSource& operator=(const IEventSource&) = delete;
    IEventSource(IEventSource&&) = delete;
    IEventSource& operator=(IEventSource&&) = delete;

    /**
     * @brief Gets the ID of the TimeFrame the events belong to.
     * @return The TimeFrame ID.
     */
    [[nodiscard]] virtual auto getTimeFrameId() const -> int = 0;

    /**
     * @brief Returns a span over the sorted event indices/timestamps.
     * 
     * The returned span contains all events in ascending order. This allows
     * for efficient binary search operations when looking for events within
     * specific time intervals.
     * 
     * @return Span over the sorted event indices/timestamps.
     */
    [[nodiscard]] virtual auto getEvents() const -> std::span<const TimeFrameIndex> = 0;

protected:
    // Protected constructor to prevent direct instantiation
    IEventSource() = default;
};

#endif // IEVENT_SOURCE_H
