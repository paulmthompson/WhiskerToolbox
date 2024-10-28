#include "Digital_Event_Series.hpp"
#include <fstream>
#include <sstream>

DigitalEventSeries::DigitalEventSeries(std::vector<float> event_vector) : _data(std::move(event_vector)) {}

std::vector<float> const& DigitalEventSeries::getEventSeries() const {
    return _data;
}

std::vector<float> load_event_series_from_csv(std::string const& filename) {
    std::vector<float> event_series;
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        float timestamp;
        ss >> timestamp;
        event_series.push_back(timestamp);
    }

    return event_series;
}