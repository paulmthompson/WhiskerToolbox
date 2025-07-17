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
    [[nodiscard]] auto compute(const ExecutionPlan& plan) const -> std::vector<T> override;

    [[nodiscard]] auto getSourceDependency() const -> std::string override {
        return m_sourceName;
    }

private:
    std::shared_ptr<IIntervalSource> m_source;
    IntervalProperty m_property;
    std::string m_sourceName;
};

// Template specializations for different data types
template<>
[[nodiscard]] auto IntervalPropertyComputer<int64_t>::compute(const ExecutionPlan& plan) const -> std::vector<int64_t>;

template<>
[[nodiscard]] auto IntervalPropertyComputer<float>::compute(const ExecutionPlan& plan) const -> std::vector<float>;

template<>
[[nodiscard]] auto IntervalPropertyComputer<double>::compute(const ExecutionPlan& plan) const -> std::vector<double>;

#endif // INTERVAL_PROPERTY_COMPUTER_H
