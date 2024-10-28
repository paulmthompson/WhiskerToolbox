#ifndef BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
#define BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP

#include <string>
#include <vector>

class DigitalEventSeries {
public:
    DigitalEventSeries() = default;
    DigitalEventSeries(std::vector<float> event_vector);

    void setData(std::vector<float> event_vector) { _data = event_vector; }
    std::vector<float> const& getEventSeries() const;

private:
    std::vector<float> _data {};
};

std::vector<float> load_event_series_from_csv(std::string const& filename);


#endif //BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
