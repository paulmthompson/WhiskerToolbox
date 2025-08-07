#ifndef TIMESTAMP_VALUE_COMPUTER_H
#define TIMESTAMP_VALUE_COMPUTER_H

#include "utils/TableView/interfaces/IColumnComputer.h"
#include "utils/TableView/interfaces/IAnalogSource.h"
#include "utils/TableView/core/ExecutionPlan.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @brief Computer for extracting values from analog sources at specific timestamps.
 * 
 * This computer works with timestamp-based row selectors (TimestampSelector) and
 * extracts single values from an analog source at each specified timestamp. It's
 * designed for cases where you want to sample analog data at specific time points
 * rather than computing statistics over intervals.
 * 
 * The computer takes an analog source and returns a vector of values corresponding
 * to the timestamps defined by the row selector. Each value represents the analog
 * signal value at that specific timestamp.
 */
class TimestampValueComputer : public IColumnComputer<double> {
public:
    /**
     * @brief Constructor for TimestampValueComputer.
     * @param source Shared pointer to the analog source to extract values from.
     */
    explicit TimestampValueComputer(std::shared_ptr<IAnalogSource> source);

    /**
     * @brief Constructor with custom source name.
     * @param source Shared pointer to the analog source to extract values from.
     * @param sourceName Custom name for the source dependency.
     */
    TimestampValueComputer(std::shared_ptr<IAnalogSource> source, std::string sourceName);

    /**
     * @brief Computes values at specific timestamps.
     * 
     * This method extracts values from the analog source at each timestamp
     * specified in the execution plan. The execution plan must contain indices
     * (not intervals) for timestamp-based operations.
     * 
     * @param plan The execution plan containing timestamp indices.
     * @return Vector of analog values at each timestamp.
     * 
     * @pre plan.hasIndices() returns true
     * @pre plan.getTimeFrame() is not null
     * @post Result vector size equals plan.getIndices().size()
     * 
     * @throws std::runtime_error if the execution plan doesn't contain indices
     * @throws std::runtime_error if the time frame is null
     */
    [[nodiscard]] auto compute(const ExecutionPlan& plan) const -> std::vector<double> override;

    /**
     * @brief Returns the source dependency name.
     * @return The name of the analog source this computer depends on.
     */
    [[nodiscard]] auto getSourceDependency() const -> std::string override;

private:
    std::shared_ptr<IAnalogSource> m_source;
    std::string m_sourceName;
};

#endif // TIMESTAMP_VALUE_COMPUTER_H 