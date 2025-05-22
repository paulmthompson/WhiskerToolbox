#ifndef ANALOG_TIME_SERIES_CSV_HPP
#define ANALOG_TIME_SERIES_CSV_HPP

#include <string>
#include <vector>

/**
 * @brief load_analog_series_from_csv
 *
 *
 *
 * @param filename - the name of the file to load
 * @return a vector of floats representing the analog time series
 */
std::vector<float> load_analog_series_from_csv(std::string const & filename);

void save_analog(
        std::vector<float> const & analog_series,
        std::vector<size_t> const & time_series,
        std::string const & block_output);


#endif// ANALOG_TIME_SERIES_CSV_HPP
