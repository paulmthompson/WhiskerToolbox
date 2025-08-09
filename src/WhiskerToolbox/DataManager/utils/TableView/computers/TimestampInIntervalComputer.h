#ifndef TIMESTAMP_IN_INTERVAL_COMPUTER_H
#define TIMESTAMP_IN_INTERVAL_COMPUTER_H

#include "utils/TableView/interfaces/IColumnComputer.h"
#include "utils/TableView/interfaces/IIntervalSource.h"
#include "utils/TableView/core/ExecutionPlan.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @brief Computer that returns true if each timestamp lies within any digital interval.
 *
 * Works with a TimestampSelector-backed ExecutionPlan. For each row timestamp t,
 * returns true if there exists an interval [start, end] from the interval source
 * such that start <= t <= end (with timeframe conversion handled by the adapter).
 */
class TimestampInIntervalComputer : public IColumnComputer<bool> {
public:
    /**
     * @brief Construct with interval source and optional dependency name.
     * @param source Interval source to query.
     * @param sourceName Name used for dependency tracking; defaults to source->getName().
     */
    explicit TimestampInIntervalComputer(std::shared_ptr<IIntervalSource> source,
                                         std::string sourceName = "")
        : m_source(std::move(source)),
          m_sourceName(sourceName.empty() && m_source ? m_source->getName() : std::move(sourceName)) {}

    /**
     * @pre plan.getTimeFrame() is not null
     * @post result.size() equals number of timestamps implied by plan
     */
    [[nodiscard]] auto compute(ExecutionPlan const & plan) const -> std::vector<bool> override;

    [[nodiscard]] auto getSourceDependency() const -> std::string override { return m_sourceName; }

private:
    std::shared_ptr<IIntervalSource> m_source;
    std::string m_sourceName;
};

#endif // TIMESTAMP_IN_INTERVAL_COMPUTER_H


