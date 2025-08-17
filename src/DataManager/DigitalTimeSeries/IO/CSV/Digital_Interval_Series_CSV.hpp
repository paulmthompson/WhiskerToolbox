#ifndef DIGITAL_INTERVAL_SERIES_CSV_HPP
#define DIGITAL_INTERVAL_SERIES_CSV_HPP

#include "TimeFrame/interval_data.hpp"

#include <string>
#include <vector>

class DigitalIntervalSeries;

std::vector<Interval> load_digital_series_from_csv(std::string const & filename, char delimiter = ' ');

/**
 * @struct CSVIntervalLoaderOptions
 *
 * @brief Options for loading DigitalIntervalSeries data from a CSV file.
 *          The CSV should have two columns: Start and End for each interval.
 *
 * @var CSVIntervalLoaderOptions::filepath
 * The path to the CSV file to load.
 *
 * @var CSVIntervalLoaderOptions::delimiter
 * The delimiter used between columns. Defaults to ",".
 *
 * @var CSVIntervalLoaderOptions::has_header
 * Whether the file has a header row that should be skipped. Defaults to false.
 *
 * @var CSVIntervalLoaderOptions::start_column
 * The column index (0-based) for the start time values. Defaults to 0.
 *
 * @var CSVIntervalLoaderOptions::end_column
 * The column index (0-based) for the end time values. Defaults to 1.
 */
struct CSVIntervalLoaderOptions {
    std::string filepath;
    std::string delimiter = ",";
    bool has_header = false;
    int start_column = 0;
    int end_column = 1;
};

/**
 * @brief Load digital interval series data from CSV using specified options
 *
 * @param options Configuration options for loading
 * @return Vector of Interval objects loaded from the CSV file
 */
std::vector<Interval> load(CSVIntervalLoaderOptions const & options);


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
