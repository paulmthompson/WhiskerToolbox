#ifndef DIGITAL_INTERVAL_SERIES_JSON_HPP
#define DIGITAL_INTERVAL_SERIES_JSON_HPP

#include "nlohmann/json.hpp"

#include <memory>
#include <string>

class DigitalIntervalSeries;

enum class IntervalDataType {
    uint16,
    csv,
    Unknown
};

IntervalDataType stringToIntervalDataType(std::string const & data_type_str);

std::shared_ptr<DigitalIntervalSeries> load_into_DigitalIntervalSeries(std::string const & file_path,
                                                                       nlohmann::basic_json<> const & item);

#endif// DIGITAL_INTERVAL_SERIES_JSON_HPP
