#include "TimestampInIntervalComputer.h"

#include "TimeFrame/interval_data.hpp"

#include <stdexcept>

std::pair<std::vector<bool>, ColumnEntityIds> TimestampInIntervalComputer::compute(ExecutionPlan const & plan) const {
    if (!m_source) {
        throw std::runtime_error("TimestampInIntervalComputer: interval source is null");
    }
    auto tf = plan.getTimeFrame();
    if (!tf) {
        throw std::runtime_error("TimestampInIntervalComputer requires non-null plan TimeFrame");
    }

    // Determine timestamps to evaluate
    std::vector<TimeFrameIndex> times;
    if (!plan.getRows().empty()) {
        times.reserve(plan.getRows().size());
        for (auto const & r : plan.getRows()) times.push_back(r.timeIndex);
    } else if (plan.hasIndices()) {
        times = plan.getIndices();
    } else if (plan.hasIntervals()) {
        auto const & itvs = plan.getIntervals();
        times.reserve(itvs.size());
        for (auto const & iv : itvs) times.push_back(iv.start);
    } else {
        throw std::runtime_error("TimestampInIntervalComputer: plan has no indices/rows/intervals");
    }

    std::vector<bool> result(times.size(), false);

    // Query intervals per timestamp using adapter timeframe conversion
    for (size_t i = 0; i < times.size(); ++i) {
        TimeFrameIndex t = times[i];
        auto intervals = m_source->getIntervalsInRange(t, t, tf.get());
        bool inside = false;
        for (auto const & iv : intervals) {
            if (is_contained(iv, t.getValue())) { inside = true; break; }
        }
        result[i] = inside;
    }

    return {result, std::monostate{}};
}


