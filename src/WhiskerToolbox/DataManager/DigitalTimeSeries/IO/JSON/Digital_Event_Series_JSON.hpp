#ifndef DIGITAL_EVENT_SERIES_JSON_HPP
#define DIGITAL_EVENT_SERIES_JSON_HPP

#include "nlohmann/json.hpp"

#include <memory>
#include <string>
#include <vector>

class DigitalEventSeries;

enum class EventDataType {
    uint16,
    csv,
    Unknown
};

EventDataType stringToEventDataType(std::string const & data_type_str);

void scale_events(std::vector<float> & events, float scale, bool scale_divide);

std::vector<std::shared_ptr<DigitalEventSeries>> load_into_DigitalEventSeries(std::string const & file_path,
                                                                       nlohmann::basic_json<> const & item);

#endif// DIGITAL_EVENT_SERIES_JSON_HPP
