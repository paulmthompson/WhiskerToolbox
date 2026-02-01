#ifndef DIGITAL_EVENT_SERIES_JSON_HPP
#define DIGITAL_EVENT_SERIES_JSON_HPP

#include "IO/core/IOFormats.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "nlohmann/json.hpp"

#include <memory>
#include <string>
#include <vector>

class DigitalEventSeries;

/**
 * @deprecated Use IOFormat instead. This enum is kept for backward compatibility.
 */
enum class EventDataType {
    uint16,
    csv,
    Unknown
};

/**
 * @deprecated Use parseFormat() from IOFormats.hpp instead
 */
EventDataType stringToEventDataType(std::string const & data_type_str);

void scale_events(std::vector<TimeFrameIndex> & events, float scale, bool scale_divide);

std::vector<std::shared_ptr<DigitalEventSeries>> load_into_DigitalEventSeries(std::string const & file_path,
                                                                       nlohmann::basic_json<> const & item);

#endif// DIGITAL_EVENT_SERIES_JSON_HPP
