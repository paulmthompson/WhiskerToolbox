#include "DigitalEventDataAdapter.h"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"

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

EntityId DigitalEventDataAdapter::getEntityIdAt(size_t index) const {
    auto const & ids = m_digitalEventSeries->getEntityIds();
    return (index < ids.size()) ? ids[index] : 0;
}

std::vector<std::pair<float, size_t>> DigitalEventDataAdapter::getDataInRangeWithIndices(TimeFrameIndex start,
                                                                                         TimeFrameIndex end,
                                                                                         TimeFrame const * target_timeFrame) {
    // Get all events from the source
    auto const & all_events = m_digitalEventSeries->getEventSeries();
    
    // Get events in the specified range using the existing method
    auto events_in_range = getDataInRange(start, end, target_timeFrame);
    
    std::vector<std::pair<float, size_t>> result;
    result.reserve(events_in_range.size());
    
    // Match events in range back to their original indices
    // This assumes events are returned in the same order as they appear in the source
    size_t result_index = 0;
    constexpr float epsilon = 1e-6f;
    
    for (size_t source_index = 0; source_index < all_events.size() && result_index < events_in_range.size(); ++source_index) {
        if (std::abs(all_events[source_index] - events_in_range[result_index]) < epsilon) {
            result.emplace_back(events_in_range[result_index], source_index);
            ++result_index;
        }
    }
    
    return result;
}
