#ifndef BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
#define BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP

#include "Observer/Observer_Data.hpp"

#include <string>
#include <vector>

/**
 * @brief Digital Event Series class
 *
 * A digital event series is a series of events where each event is defined by a time.
 * (Compare to DigitalIntervalSeries which is a series of intervals with start and end times)
 *
 * Use digital events where you wish to specify a time for each event.
 *
 *
 */
class DigitalEventSeries : public ObserverData {
public:
    DigitalEventSeries() = default;
    DigitalEventSeries(std::vector<float> event_vector);

    void setData(std::vector<float> event_vector) { _data = event_vector; }
    std::vector<float> const& getEventSeries() const;

private:
    std::vector<float> _data {};
};

namespace DigitalEventSeriesLoader {
    DigitalEventSeries loadFromCSV(const std::string& filename);
} // namespace DigitalEventSeriesLoader


#endif //BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
