#ifndef ANALOG_TIME_SERIES_JSON_HPP
#define ANALOG_TIME_SERIES_JSON_HPP

#include "nlohmann/json.hpp"

#include <memory>
#include <string>
#include <vector>

enum class AnalogDataType {
    Binary,  // Binary file format (uses binary_data_type for actual data type: int16, float32, etc.)
    csv,     // CSV text format
    Unknown
};

class AnalogTimeSeries;

AnalogDataType stringToAnalogDataType(std::string const & data_type_str);

std::vector<std::shared_ptr<AnalogTimeSeries>> load_into_AnalogTimeSeries(std::string const & file_path,
                                                                          nlohmann::basic_json<> const & item);

#endif// ANALOG_TIME_SERIES_JSON_HPP
