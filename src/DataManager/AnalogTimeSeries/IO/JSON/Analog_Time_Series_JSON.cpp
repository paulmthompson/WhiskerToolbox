
#include "Analog_Time_Series_JSON.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/IO/Binary/Analog_Time_Series_Binary.hpp"
#include "AnalogTimeSeries/IO/CSV/Analog_Time_Series_CSV.hpp"
#include "utils/json_helpers.hpp"
#include "utils/json_reflection.hpp"

#include <iostream>

using namespace WhiskerToolbox::Reflection;

AnalogDataType stringToAnalogDataType(std::string const & data_type_str) {
    // "binary" is the preferred format name, "int16" kept for backward compatibility
    if (data_type_str == "binary") return AnalogDataType::Binary;
    if (data_type_str == "csv") return AnalogDataType::csv;
    return AnalogDataType::Unknown;
}
