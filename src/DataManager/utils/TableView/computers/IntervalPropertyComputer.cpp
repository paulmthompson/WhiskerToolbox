#include "IntervalPropertyComputer.h"

#include <algorithm>
#include <stdexcept>

// Template specializations for different data types
template std::pair<std::vector<int64_t>, ColumnEntityIds>
IntervalPropertyComputer<int64_t>::compute(ExecutionPlan const & plan) const;

template std::pair<std::vector<float>, ColumnEntityIds>
IntervalPropertyComputer<float>::compute(ExecutionPlan const & plan) const;

template std::pair<std::vector<double>, ColumnEntityIds>
IntervalPropertyComputer<double>::compute(ExecutionPlan const & plan) const;

// Template specializations for validation method
template void IntervalPropertyComputer<int64_t>::_validateRowIntervalsAreSubset(std::vector<TimeFrameInterval> const &, TimeFrame const *) const;
template void IntervalPropertyComputer<float>::_validateRowIntervalsAreSubset(std::vector<TimeFrameInterval> const &, TimeFrame const *) const;
template void IntervalPropertyComputer<double>::_validateRowIntervalsAreSubset(std::vector<TimeFrameInterval> const &, TimeFrame const *) const;

template<typename T>
void IntervalPropertyComputer<T>::_validateRowIntervalsAreSubset(std::vector<TimeFrameInterval> const & rowIntervals,
                                                                 TimeFrame const * destinationTimeFrame) const {
    // Get all intervals from the source
    auto sourceIntervals = m_source->view();

    auto sourceTimeFrame = m_source->getTimeFrame();
    if (sourceTimeFrame.get() != destinationTimeFrame) {
        throw std::runtime_error("Source interval source has different timeframe than destination timeframe");
    }

    // Check that each row interval exactly matches a source interval
    for (auto const & rowInterval: rowIntervals) {
        bool found = false;
        for (auto const & sourceInterval: sourceIntervals) {
            if (rowInterval.start.getValue() == sourceInterval.value().start && rowInterval.end.getValue() == sourceInterval.value().end) {
                found = true;
                break;
            }
        }

        if (!found) {
            throw std::runtime_error("Row interval [" + std::to_string(rowInterval.start.getValue()) +
                                     ", " + std::to_string(rowInterval.end.getValue()) +
                                     "] is not found in source intervals. "
                                     "IntervalPropertyComputer requires row intervals to be a subset of source intervals.");
        }
    }
}