#include "Digital_Event_Series.hpp"
#include "loaders/CSV_Loaders.hpp"
#include <fstream>
#include <sstream>

DigitalEventSeries::DigitalEventSeries(std::vector<float> event_vector)
    : _data(std::move(event_vector)) {}

std::vector<float> const & DigitalEventSeries::getEventSeries() const {
    return _data;
}

namespace DigitalEventSeriesLoader {

DigitalEventSeries loadFromCSV(std::string const & filename) {
    return DigitalEventSeries(CSVLoader::loadSingleColumnCSV(filename));
}

}// namespace DigitalEventSeriesLoader