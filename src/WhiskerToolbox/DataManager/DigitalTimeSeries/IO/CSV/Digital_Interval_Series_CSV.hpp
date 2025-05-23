#ifndef DIGITAL_INTERVAL_SERIES_CSV_HPP
#define DIGITAL_INTERVAL_SERIES_CSV_HPP

#include "DigitalTimeSeries/interval_data.hpp"

#include <string>
#include <vector>

class DigitalIntervalSeries;

std::vector<Interval> load_digital_series_from_csv(std::string const & filename, char delimiter = ' ');


/**
 * @struct CSVIntervalSaverOptions
 *
 * @brief Options for saving DigitalIntervalSeries data to a CSV file.
 *          The CSV will typically have two columns: Start and End for each interval.
 *
 * @var CSVIntervalSaverOptions::filename
 * The name of the file to save the data to (e.g., "intervals.csv").
 *
 * @var CSVIntervalSaverOptions::parent_dir
 * The directory where the file will be saved. Defaults to ".".
 *
 * @var CSVIntervalSaverOptions::delimiter
 * The delimiter to use between columns. Defaults to ",".
 *
 * @var CSVIntervalSaverOptions::line_delim
 * The line delimiter to use. Defaults to "\n".
 *
 * @var CSVIntervalSaverOptions::save_header
 * Whether to save a header row. Defaults to true.
 *
 * @var CSVIntervalSaverOptions::header
 * The header string to use if save_header is true. Defaults to "Start,End".
 */
struct CSVIntervalSaverOptions {
    std::string filename = "intervals_output.csv";
    std::string parent_dir = ".";
    std::string delimiter = ",";
    std::string line_delim = "\n";
    bool save_header = true;
    std::string header = "Start,End";
};


/**
 * @brief Saves a DigitalIntervalSeries object to a CSV file using specified options.
 * 
 * @param interval_data Pointer to the DigitalIntervalSeries object to save.
 * @param opts Configuration options for saving.
 */
void save(DigitalIntervalSeries const * interval_data,
          CSVIntervalSaverOptions const & opts);

#endif// DIGITAL_INTERVAL_SERIES_CSV_HPP
