#include "DigitalEventDataAdapter.h"

#include <stdexcept>

DigitalEventDataAdapter::DigitalEventDataAdapter(std::shared_ptr<DigitalEventSeries> digitalEventSeries,
                                                 std::shared_ptr<TimeFrame> timeFrame,
                                                 std::string name)
    : m_digitalEventSeries(std::move(digitalEventSeries)),
      m_timeFrame(std::move(timeFrame)),
      m_name(std::move(name)) {
    if (!m_digitalEventSeries) {
        throw std::invalid_argument("DigitalEventSeries cannot be null");
    }
}

std::string const & DigitalEventDataAdapter::getName() const {
    return m_name;
}

std::shared_ptr<TimeFrame> DigitalEventDataAdapter::getTimeFrame() const {
    return m_timeFrame;
}

size_t DigitalEventDataAdapter::size() const {
    return m_digitalEventSeries->size();
}

std::vector<float> DigitalEventDataAdapter::getDataInRange(TimeFrameIndex start,
                                                           TimeFrameIndex end,
                                                           TimeFrame const * target_timeFrame) {
    // Use the DigitalEventSeries' built-in method to get events in the time range
    // This method handles the time frame conversion internally
    auto events_view = m_digitalEventSeries->getEventsInRange(start, end, target_timeFrame, m_timeFrame.get());

    // Convert the view to a vector
    std::vector<float> result;
    for (float event_time: events_view) {
        result.push_back(event_time);
    }

    return result;
}
