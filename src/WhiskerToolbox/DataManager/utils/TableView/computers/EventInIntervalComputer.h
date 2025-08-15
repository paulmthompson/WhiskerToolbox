#ifndef EVENT_IN_INTERVAL_COMPUTER_H
#define EVENT_IN_INTERVAL_COMPUTER_H

#include "utils/TableView/core/ExecutionPlan.h"
#include "utils/TableView/interfaces/IColumnComputer.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

class IEventSource;

/**
 * @brief Enumeration of operations that can be performed on events within intervals.
 */
enum class EventOperation : std::uint8_t {
    Presence,    ///< Returns bool: true if any events exist in the interval
    Count,       ///< Returns int: number of events in the interval
    Gather,      ///< Returns std::vector<float>: all events in the interval
    Gather_Center///< Returns std::vector<float>: all events in the interval, centered relative to interval center
};

/**
 * @brief Templated computer for processing events within time intervals.
 * 
 * Source type: IEventSource
 * Selector type: Interval
 * Output type: T
 * 
 * This computer analyzes event data from an IEventSource and performs statistical operations
 * on events that fall within specified time intervals. It supports different analysis modes
 * through the EventOperation enum, each requiring a specific template parameter type.
 * 
 * The computer efficiently processes events using binary search algorithms and handles
 * time frame conversions between source and destination time frames automatically.
 * 
 * @tparam T The return type for the computation. Must match the operation:
 *           - EventOperation::Presence requires T = bool
 *           - EventOperation::Count requires T = int  
 *           - EventOperation::Gather requires T = std::vector<float>
 *           - EventOperation::Gather_Center requires T = std::vector<float>
 * 
 * @par Usage Example:
 * @code
 * // Create an event source with spike data
 * auto spikeSource = std::make_shared<SpikeEventSource>("Neuron1", timeFrame, spikeTimes);
 * 
 * // Create intervals for analysis
 * std::vector<TimeFrameInterval> intervals = {
 *     TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(10)),  // 0-10ms
 *     TimeFrameInterval(TimeFrameIndex(10), TimeFrameIndex(20)), // 10-20ms
 *     TimeFrameInterval(TimeFrameIndex(20), TimeFrameIndex(30))  // 20-30ms
 * };
 * ExecutionPlan plan(intervals, timeFrame);
 * 
 * // Check for presence of events in each interval
 * EventInIntervalComputer<bool> presenceComputer(spikeSource, EventOperation::Presence, "Neuron1");
 * auto presenceResults = presenceComputer.compute(plan);
 * // Result: [true, false, true] - events present in intervals 0-10ms and 20-30ms
 * 
 * // Count events in each interval
 * EventInIntervalComputer<int> countComputer(spikeSource, EventOperation::Count, "Neuron1");
 * auto countResults = countComputer.compute(plan);
 * // Result: [3, 0, 2] - 3 events in 0-10ms, 0 in 10-20ms, 2 in 20-30ms
 * 
 * // Gather all events in each interval
 * EventInIntervalComputer<std::vector<float>> gatherComputer(spikeSource, EventOperation::Gather, "Neuron1");
 * auto gatherResults = gatherComputer.compute(plan);
 * // Result: [[1.2, 3.4, 8.9], [], [22.1, 25.7]] - actual event times
 * @endcode
 * 
 */
template<typename T>
class EventInIntervalComputer : public IColumnComputer<T> {
public:
    /**
     * @brief Constructor for EventInIntervalComputer.
     * 
     * Creates a computer that will analyze events from the specified source using
     * the given operation. The source name is used for dependency tracking in
     * the table view system.
     * 
     * @param source Shared pointer to the event source providing the event data.
     *               Must not be null.
     * @param operation The statistical operation to perform on events within intervals.
     *                 Must be compatible with the template parameter T.
     * @param sourceName The name of the data source for dependency tracking.
     *                  Used to identify data dependencies in the table view system.
     * 
     * @pre @p source is not null
     * @pre @p sourceName is not empty
     * @pre @p operation is compatible with template parameter T:
     *      - EventOperation::Presence requires T = bool
     *      - EventOperation::Count requires T = int
     *      - EventOperation::Gather requires T = std::vector<float>
     *      - EventOperation::Gather_Center requires T = std::vector<float>
     * 
     * @post The computer is ready to process events from the specified source
     * @post getSourceDependency() returns @p sourceName
     */
    EventInIntervalComputer(std::shared_ptr<IEventSource> source,
                            EventOperation operation,
                            std::string sourceName)
        : m_source(std::move(source)),
          m_operation(operation),
          m_sourceName(std::move(sourceName)) {}

    /**
     * @brief Computes the result for all intervals in the execution plan.
     * 
     * Processes each interval in the execution plan and applies the configured
     * operation to events that fall within each interval. The computation handles
     * time frame conversions automatically between the source and destination time frames.
     * 
     * @param plan The execution plan containing interval boundaries and destination time frame.
     *             Must contain valid intervals and a non-null time frame.
     * 
     * @return Vector of computed results for each interval in the same order as the plan.
     *         The size of the result vector equals the number of intervals in the plan.
     * 
     * @pre @p plan contains valid intervals (start <= end for each interval)
     * @pre @p plan.getTimeFrame() is not null
     * @pre The template parameter T matches the operation type:
     *      - EventOperation::Presence requires T = bool
     *      - EventOperation::Count requires T = int
     *      - EventOperation::Gather requires T = std::vector<float>
     *      - EventOperation::Gather_Center requires T = std::vector<float>
     * 
     * @post Result vector size equals plan.getIntervals().size()
     * @post For Presence operation: each result is true if any events exist in the interval, false otherwise
     * @post For Count operation: each result is the number of events in the corresponding interval
     * @post For Gather operation: each result is a vector of event times within the interval
     * @post For Gather_Center operation: each result is a vector of event times relative to interval center
     * 
     * @throws std::runtime_error if the operation type doesn't match the template parameter T
     * @throws std::runtime_error if the source time frame is incompatible with the destination time frame
     */
    [[nodiscard]] std::vector<T> compute(ExecutionPlan const & plan) const;

    /**
     * @brief Returns the name of the data source this computer depends on.
     * 
     * Used by the table view system to track data dependencies and determine
     * when recomputation is needed.
     * 
     * @return The name of the source dependency as specified in the constructor.
     */
    [[nodiscard]] std::string getSourceDependency() const {
        return m_sourceName;
    }

private:
    std::shared_ptr<IEventSource> m_source;
    EventOperation m_operation;
    std::string m_sourceName;

    /**
     * @brief Finds events within a specific interval using binary search.
     * 
     * Efficiently locates all events that fall within the specified interval
     * using binary search algorithms. This method assumes the events are
     * sorted in ascending order.
     * 
     * @param events Span of all events, must be sorted in ascending order.
     * @param startIdx Start index of the interval (inclusive).
     * @param endIdx End index of the interval (inclusive).
     * 
     * @return Vector of TimeFrameIndex values representing events within the interval.
     *         Events are returned in ascending order.
     * 
     * @pre @p events is sorted in ascending order
     * @pre @p startIdx <= @p endIdx
     * 
     * @post Result contains only events where startIdx <= event <= endIdx
     * @post Result is sorted in ascending order
     * 
     * @note Time complexity: O(log n) where n is the number of events
     */
    [[nodiscard]] std::vector<TimeFrameIndex> findEventsInInterval(std::span<TimeFrameIndex const> events,
                                                                   TimeFrameIndex startIdx,
                                                                   TimeFrameIndex endIdx) const;
};

// Template specializations for different operation types
template<>
[[nodiscard]] std::vector<bool> EventInIntervalComputer<bool>::compute(ExecutionPlan const & plan) const;

template<>
[[nodiscard]] std::vector<int> EventInIntervalComputer<int>::compute(ExecutionPlan const & plan) const;

template<>
[[nodiscard]] std::vector<std::vector<float>> EventInIntervalComputer<std::vector<float>>::compute(ExecutionPlan const & plan) const;

#endif// EVENT_IN_INTERVAL_COMPUTER_H
