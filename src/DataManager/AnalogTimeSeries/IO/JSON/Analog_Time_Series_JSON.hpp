#ifndef ANALOG_TIME_SERIES_JSON_HPP
#define ANALOG_TIME_SERIES_JSON_HPP

#include "IO/core/IOFormats.hpp"
#include "nlohmann/json.hpp"

#include <memory>
#include <string>
#include <vector>

/**
 * @deprecated Use IOFormat instead. This enum is kept for backward compatibility.
 */
enum class AnalogDataType {
    Binary,  // Binary file format (uses binary_data_type for actual data type: int16, float32, etc.)
    csv,     // CSV text format
    Unknown
};

class AnalogTimeSeries;

/**
 * @deprecated Use parseFormat() from IOFormats.hpp instead
 */
AnalogDataType stringToAnalogDataType(std::string const & data_type_str);

std::vector<std::shared_ptr<AnalogTimeSeries>> load_into_AnalogTimeSeries(std::string const & file_path,
                                                                          nlohmann::basic_json<> const & item);

#endif// ANALOG_TIME_SERIES_JSON_HPP
