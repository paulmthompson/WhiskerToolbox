#ifndef DIGITAL_INTERVAL_SERIES_HPP
#define DIGITAL_INTERVAL_SERIES_HPP

#include "Observer/Observer_Data.hpp"

#include <string>
#include <vector>
#include <utility>

class DigitalIntervalSeries : public ObserverData {
public:
    DigitalIntervalSeries() = default;
    DigitalIntervalSeries(std::vector<std::pair<float, float>> digital_vector);

    void setData(std::vector<std::pair<float, float>> digital_vector)
    {
        _data = digital_vector;
        _sortData();
        notifyObservers();
    };

    void addEvent(float start, float end)
    {
        _data.emplace_back(start, end);
        _sortData();
        notifyObservers();
    }
    std::vector<std::pair<float, float>> const& getDigitalIntervalSeries() const;

    bool isEventAtTime(int time) const;
    void setEventAtTime(int time, bool event);

    void createIntervalsFromBool(std::vector<uint8_t> const& bool_vector);

    size_t size() {return _data.size();};

private:
    std::vector<std::pair<float, float>> _data {};

    void _sortData();

};

std::vector<std::pair<float, float>> load_digital_series_from_csv(std::string const& filename);

int find_closest_preceding_event(DigitalIntervalSeries * digital_series, int time);

#endif // DIGITAL_INTERVAL_SERIES_HPP
