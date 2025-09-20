#ifndef IINTERVAL_SOURCE_H
#define IINTERVAL_SOURCE_H

#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "Entity/EntityTypes.hpp"
#include "DigitalTimeSeries/IntervalWithId.hpp"

#include <memory>
#include <string>
#include <vector>

/**
 * @brief Interface for data sources that consist of time intervals.
 * 
 * This interface is designed for data that represents intervals in time,
 * such as digital interval series or behavioral episodes. Each interval
 * is defined by a start and end time.
 */
class IIntervalSource {
public:
    virtual ~IIntervalSource() = default;
    
    // Make this class non-copyable and non-movable since it's a pure interface
    IIntervalSource(const IIntervalSource&) = delete;
    IIntervalSource& operator=(const IIntervalSource&) = delete;
    IIntervalSource(IIntervalSource&&) = delete;
    IIntervalSource& operator=(IIntervalSource&&) = delete;

    /**
     * @brief Gets the name of this data source.
     * 
     * This name is used for dependency tracking and ExecutionPlan caching
     * in the TableView system.
     * 
     * @return The name of the data source.
     */
    virtual std::string const & getName() const = 0;

    /**
     * @brief Gets the TimeFrame the data belongs to.
     * @return A shared pointer to the TimeFrame.
     */
    virtual std::shared_ptr<TimeFrame> getTimeFrame() const = 0;

    /**
     * @brief Gets the total number of intervals in the source.
     * @return The number of intervals.
     */
    virtual size_t size() const = 0;

    virtual std::vector<Interval> getIntervals() = 0;

    /**
     * @brief Gets the intervals within a specific time range.
     * 
     * This gets the intervals in the range [start, end] (inclusive) from the source timeframe
     * 
     * @param start The start index of the time range.
     * @param end The end index of the time range.
     * @param target_timeFrame The target time frame (from the caller) for the data.
     * @return A vector of Intervals representing the intervals in the specified range.
     */
    virtual std::vector<Interval> getIntervalsInRange(TimeFrameIndex start,
                                                      TimeFrameIndex end,
                                                      TimeFrame const * target_timeFrame) = 0;

    virtual std::vector<IntervalWithId> getIntervalsWithIdsInRange(TimeFrameIndex start,
                                                                   TimeFrameIndex end,
                                                                   TimeFrame const * target_timeFrame) = 0;

    /**
     * @brief Optional: get the EntityId for the k-th interval in the source ordering.
     * Implementors that don't support EntityIds may return 0.
     */
    [[nodiscard]] virtual auto getEntityIdAt(size_t index) const -> EntityId { (void)index; return 0; }

protected:
    // Protected constructor to prevent direct instantiation
    IIntervalSource() = default;
};

#endif // IINTERVAL_SOURCE_H
