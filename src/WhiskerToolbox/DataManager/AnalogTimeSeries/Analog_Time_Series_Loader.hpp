#ifndef ANALOG_TIME_SERIES_LOADER_HPP
#define ANALOG_TIME_SERIES_LOADER_HPP

#include "nlohmann/json.hpp"

#include <memory>
#include <string>
#include <vector>

enum class AnalogDataType {
    int16,
    Unknown
};

class AnalogTimeSeries;

AnalogDataType stringToAnalogDataType(std::string const & data_type_str);

std::vector<std::shared_ptr<AnalogTimeSeries>> load_into_AnalogTimeSeries(std::string const & file_path,
                                                                          nlohmann::basic_json<> const & item);

/**
 * @brief load_analog_series_from_csv
 *
 *
 *
 * @param filename - the name of the file to load
 * @return a vector of floats representing the analog time series
 */
std::vector<float> load_analog_series_from_csv(std::string const & filename);

#endif// ANALOG_TIME_SERIES_LOADER_HPP
