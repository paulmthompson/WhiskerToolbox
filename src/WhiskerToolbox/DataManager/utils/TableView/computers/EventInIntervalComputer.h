#ifndef EVENT_IN_INTERVAL_COMPUTER_H
#define EVENT_IN_INTERVAL_COMPUTER_H

#include "utils/TableView/interfaces/IColumnComputer.h"
#include "utils/TableView/interfaces/IEventSource.h"
#include "utils/TableView/core/ExecutionPlan.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Enumeration of operations that can be performed on events within intervals.
 */
enum class EventOperation : std::uint8_t {
    Presence,  ///< Returns bool: true if any events exist in the interval
    Count,     ///< Returns int: number of events in the interval
    Gather,     ///< Returns std::vector<TimeFrameIndex>: all events in the interval
    Gather_Center
};

/**
 * @brief Templated computer for processing events within time intervals.
 * 
 * This computer works with IEventSource data and can perform different operations
 * on events that fall within specified intervals. The template parameter T
 * determines the return type based on the operation:
 * - EventOperation::Presence requires T = bool
 * - EventOperation::Count requires T = int
 * - EventOperation::Gather requires T = std::vector<TimeFrameIndex>
 */
template<typename T>
class EventInIntervalComputer : public IColumnComputer<T> {
public:
    /**
     * @brief Constructor for EventInIntervalComputer.
     * @param source Shared pointer to the event source.
     * @param operation The operation to perform on events within intervals.
     * @param sourceName The name of the data source (for dependency tracking).
     */
    EventInIntervalComputer(std::shared_ptr<IEventSource> source, 
                           EventOperation operation,
                           std::string sourceName)
        : m_source(std::move(source)), m_operation(operation), m_sourceName(std::move(sourceName)) {}

    /**
     * @brief Computes the result for all intervals in the execution plan.
     * @param plan The execution plan containing interval boundaries.
     * @return Vector of computed results for each interval.
     */
    [[nodiscard]] auto compute(const ExecutionPlan& plan) const -> std::vector<T> override;

    [[nodiscard]] auto getSourceDependency() const -> std::string override {
        return m_sourceName;
    }

private:
    std::shared_ptr<IEventSource> m_source;
    EventOperation m_operation;
    std::string m_sourceName;

    /**
     * @brief Finds events within a specific interval using binary search.
     * @param events Span of all events (sorted).
     * @param startIdx Start index of the interval.
     * @param endIdx End index of the interval.
     * @return Vector of events within the interval.
     */
    [[nodiscard]] auto findEventsInInterval(std::span<const TimeFrameIndex> events,
                                           TimeFrameIndex startIdx,
                                           TimeFrameIndex endIdx) const -> std::vector<TimeFrameIndex>;
};

// Template specializations for different operation types
template<>
[[nodiscard]] auto EventInIntervalComputer<bool>::compute(const ExecutionPlan& plan) const -> std::vector<bool>;

template<>
[[nodiscard]] auto EventInIntervalComputer<int>::compute(const ExecutionPlan& plan) const -> std::vector<int>;

template<>
[[nodiscard]] auto EventInIntervalComputer<std::vector<float>>::compute(const ExecutionPlan& plan) const -> std::vector<std::vector<float>>;

#endif // EVENT_IN_INTERVAL_COMPUTER_H
