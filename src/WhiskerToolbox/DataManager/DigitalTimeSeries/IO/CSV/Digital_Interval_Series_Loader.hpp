#ifndef DIGITAL_INTERVAL_SERIES_LOADER_HPP
#define DIGITAL_INTERVAL_SERIES_LOADER_HPP

#include "../../interval_data.hpp"

#include "nlohmann/json.hpp"

#include <memory>
#include <string>

enum class IntervalDataType {
    uint16,
    csv,
    Unknown
};

class DigitalIntervalSeries;

IntervalDataType stringToIntervalDataType(std::string const & data_type_str);

std::shared_ptr<DigitalIntervalSeries> load_into_DigitalIntervalSeries(std::string const & file_path,
                                                                       nlohmann::basic_json<> const & item);

std::vector<Interval> load_digital_series_from_csv(std::string const & filename, char delimiter = ' ');

void save_intervals(std::vector<Interval> const & intervals,
                    std::string const & block_output);

#endif// DIGITAL_INTERVAL_SERIES_LOADER_HPP
