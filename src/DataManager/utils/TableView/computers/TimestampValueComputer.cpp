#include "TimestampValueComputer.h"

#include <limits>
#include <stdexcept>

TimestampValueComputer::TimestampValueComputer(std::shared_ptr<IAnalogSource> source)
    : IColumnComputer()
    , m_source(std::move(source))
    , m_sourceName(m_source ? m_source->getName() : "null_source")
{
    if (!m_source) {
        throw std::invalid_argument("IAnalogSource cannot be null");
    }
}

TimestampValueComputer::TimestampValueComputer(std::shared_ptr<IAnalogSource> source, std::string sourceName)
    : IColumnComputer()
    , m_source(std::move(source))
    , m_sourceName(std::move(sourceName))
{
    if (!m_source) {
        throw std::invalid_argument("IAnalogSource cannot be null");
    }
}

std::pair<std::vector<double>, ColumnEntityIds> TimestampValueComputer::compute(const ExecutionPlan& plan) const {
    if (!plan.hasIndices()) {
        throw std::runtime_error("TimestampValueComputer requires an ExecutionPlan with indices");
    }

    auto timeFrame = plan.getTimeFrame();
    if (!timeFrame) {
        throw std::runtime_error("TimestampValueComputer requires a non-null TimeFrame");
    }

    auto const & indices = plan.getIndices();
    std::vector<double> results;
    results.reserve(indices.size());
    std::vector<std::vector<EntityId>> entity_ids;

    for (auto const & index : indices) {
        // Get a single value at the specified timestamp
        // We use a range of size 1 to get just the value at this index
        auto sliceView = m_source->getDataInRange(index, index, timeFrame.get());
        
        if (sliceView.empty()) {
            // If no data available at this timestamp, use NaN
            results.push_back(std::numeric_limits<double>::quiet_NaN());
        } else {
            // Take the first (and only) value from the slice
            results.push_back(static_cast<double>(sliceView[0]));
        }
    }

    return {results, entity_ids};
}

std::string TimestampValueComputer::getSourceDependency() const {
    return m_sourceName;
} 