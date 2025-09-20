#ifndef IEVENT_SOURCE_H
#define IEVENT_SOURCE_H

#include "DigitalTimeSeries/EventWithId.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

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
    IEventSource(IEventSource const &) = delete;
    IEventSource & operator=(IEventSource const &) = delete;
    IEventSource(IEventSource &&) = delete;
    IEventSource & operator=(IEventSource &&) = delete;

    /** 
     *@brief Gets the name of this data source.
     * 
     * This name is used for dependency tracking and ExecutionPlan caching
     * in the TableView system.
     * 
     * @return The name of the data source.
     */
    virtual auto getName() const -> std::string const & = 0;

    /**
     * @brief Gets the total number of events in the source.
     * @return The number of events.
     */
    virtual auto size() const -> size_t = 0;


    /**
     * @brief Gets the TimeFrame the data belongs to.
     * @return A shared pointer to the TimeFrame.
     */
    virtual std::shared_ptr<TimeFrame> getTimeFrame() const = 0;

    /**
     * @brief Gets the data within a specific time range.
     * 
     * This gets the data in the range [start, end] (inclusive) from the source timeframe
     * 
     * @param start The start index of the time range.
     * @param end The end index of the time range.
     * @param target_timeFrame The target time frame (from the caller) for the data.
     * @return A vector of floats representing the data in the specified range.
     */
    virtual std::vector<float> getDataInRange(TimeFrameIndex start,
                                              TimeFrameIndex end,
                                              TimeFrame const * target_timeFrame) = 0;

    virtual std::vector<EventWithId> getDataInRangeWithEntityIds(TimeFrameIndex start,
                                                                 TimeFrameIndex end,
                                                                 TimeFrame const * target_timeFrame) = 0;

    /**
     * @brief Optional: get the EntityId for the k-th event in the source ordering.
     * Implementors that don't support EntityIds may return 0.
     */
    [[nodiscard]] virtual auto getEntityIdAt(size_t index) const -> EntityId {
        (void) index;
        return 0;
    }

protected:
    // Protected constructor to prevent direct instantiation
    IEventSource() = default;
};

#endif// IEVENT_SOURCE_H
