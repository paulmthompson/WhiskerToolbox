#ifndef DIGITAL_INTERVAL_SERIES_LOADER_HPP
#define DIGITAL_INTERVAL_SERIES_LOADER_HPP

#include "DigitalTimeSeries/interval_data.hpp"

#include <string>
#include <vector>

std::vector<Interval> load_digital_series_from_csv(std::string const & filename, char delimiter = ' ');

void save_intervals(std::vector<Interval> const & intervals,
                    std::string const & block_output);

#endif// DIGITAL_INTERVAL_SERIES_LOADER_HPP
