#include "DigitalIntervalDataAdapter.h"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <stdexcept>

DigitalIntervalDataAdapter::DigitalIntervalDataAdapter(std::shared_ptr<DigitalIntervalSeries> digitalIntervalSeries,
                                                       std::shared_ptr<TimeFrame> timeFrame,
                                                       std::string name)
    : m_digitalIntervalSeries(std::move(digitalIntervalSeries)),
      m_timeFrame(std::move(timeFrame)),
      m_name(std::move(name)) {
    if (!m_digitalIntervalSeries) {
        throw std::invalid_argument("DigitalIntervalSeries cannot be null");
    }
}

std::string const & DigitalIntervalDataAdapter::getName() const {
    return m_name;
}

std::shared_ptr<TimeFrame> DigitalIntervalDataAdapter::getTimeFrame() const {
    return m_timeFrame;
}

size_t DigitalIntervalDataAdapter::size() const {
    return m_digitalIntervalSeries->size();
}

std::vector<Interval> DigitalIntervalDataAdapter::getIntervals() {
    return m_digitalIntervalSeries->getDigitalIntervalSeries();
}

std::vector<Interval> DigitalIntervalDataAdapter::getIntervalsInRange(TimeFrameIndex start,
                                                                      TimeFrameIndex end,
                                                                      TimeFrame const * target_timeFrame) {
    // Use the DigitalIntervalSeries' built-in method to get intervals in the time range
    // This method handles the time frame conversion internally
    // Using OVERLAPPING mode to get any intervals that overlap with the range
    auto intervals_view = m_digitalIntervalSeries->getIntervalsInRange<DigitalIntervalSeries::RangeMode::OVERLAPPING>(
            start, end, *target_timeFrame);

    // Convert the view to a vector
    std::vector<Interval> result;
    for (Interval const & interval: intervals_view) {
        result.push_back(interval);
    }

    return result;
}

std::vector<IntervalWithId> DigitalIntervalDataAdapter::getIntervalsWithIdsInRange(TimeFrameIndex start,
                                                                                   TimeFrameIndex end,
                                                                                   TimeFrame const * target_timeFrame) {
    return m_digitalIntervalSeries->getIntervalsWithIdsInRange(start, end, *target_timeFrame);
}

EntityId DigitalIntervalDataAdapter::getEntityIdAt(size_t index) const {
    auto const & ids = m_digitalIntervalSeries->getEntityIds();
    return (index < ids.size()) ? ids[index] : EntityId(0);
}