#ifndef INTERVAL_PROPERTY_COMPUTER_H
#define INTERVAL_PROPERTY_COMPUTER_H

#include "utils/TableView/interfaces/IColumnComputer.h"
#include "utils/TableView/interfaces/IIntervalSource.h"
#include "utils/TableView/core/ExecutionPlan.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Enumeration of operations that can be performed on interval properties.
 */
enum class IntervalProperty : std::uint8_t {
    Start,     ///< Returns the start time/index of the interval
    End,       ///< Returns the end time/index of the interval
    Duration   ///< Returns the duration (end - start) of the interval
};

/**
 * @brief Templated computer for extracting properties from time intervals.
 * 
 * Source type: IIntervalSource
 * Selector type: Interval
 * Output type: T
 * 
 * This computer works with IIntervalSource data and can extract different properties
 * from intervals that are used as row selectors. The template parameter T
 * determines the return type based on the property being extracted:
 * - IntervalProperty::Start requires T = int64_t or float
 * - IntervalProperty::End requires T = int64_t or float
 * - IntervalProperty::Duration requires T = int64_t or float
 */
template<typename T>
class IntervalPropertyComputer : public IColumnComputer<T> {
public:
    /**
     * @brief Constructor for IntervalPropertyComputer.
     * @param source Shared pointer to the interval source.
     * @param property The property to extract from intervals.
     * @param sourceName The name of the data source (for dependency tracking).
     */
    IntervalPropertyComputer(std::shared_ptr<IIntervalSource> source, 
                            IntervalProperty property,
                            std::string sourceName)
        : m_source(std::move(source)), m_property(property), m_sourceName(std::move(sourceName)) {}

    /**
     * @brief Computes the result for all intervals in the execution plan.
     * @param plan The execution plan containing interval boundaries.
     * @return Vector of computed results for each interval.
     */
    [[nodiscard]] auto compute(const ExecutionPlan& plan) const -> std::vector<T> override {
        if (!plan.hasIntervals()) {
            throw std::runtime_error("IntervalPropertyComputer requires an ExecutionPlan with intervals");
        }
        
        auto intervals = plan.getIntervals();
        auto destinationTimeFrame = plan.getTimeFrame();
        
        std::vector<T> results;
        results.reserve(intervals.size());
        
        for (const auto& interval : intervals) {
            switch (m_property) {
                case IntervalProperty::Start:
                    results.push_back(interval.start.getValue());
                    break;
                case IntervalProperty::End:
                    results.push_back(interval.end.getValue());
                    break;
                case IntervalProperty::Duration:
                    results.push_back(interval.end.getValue() - interval.start.getValue());
                    break;
                default:
                    throw std::runtime_error("Unknown IntervalProperty");
            }
        }
        
        return results;
    }

    [[nodiscard]] auto getSourceDependency() const -> std::string override {
        return m_sourceName;
    }

private:
    std::shared_ptr<IIntervalSource> m_source;
    IntervalProperty m_property;
    std::string m_sourceName;
};



#endif // INTERVAL_PROPERTY_COMPUTER_H
