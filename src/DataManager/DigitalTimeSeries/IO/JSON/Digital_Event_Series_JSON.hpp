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

/**
 * @brief Load DigitalEventSeries from JSON configuration
 * 
 * @note This function supports multi-series loading (e.g., CSV files with multiple columns)
 *       which the plugin system's single-series return cannot handle. It is kept for
 *       this capability and used as a fallback when the plugin system cannot handle
 *       the format.
 * 
 * @param file_path Path to the data file
 * @param item JSON configuration
 * @return Vector of loaded DigitalEventSeries (one per series/column)
 */
std::vector<std::shared_ptr<DigitalEventSeries>> load_into_DigitalEventSeries(std::string const & file_path,
                                                                       nlohmann::basic_json<> const & item);

#endif// DIGITAL_EVENT_SERIES_JSON_HPP
