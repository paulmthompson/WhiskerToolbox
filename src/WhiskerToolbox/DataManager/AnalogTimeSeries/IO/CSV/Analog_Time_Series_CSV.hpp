#ifndef ANALOG_TIME_SERIES_CSV_HPP
#define ANALOG_TIME_SERIES_CSV_HPP

#include <string>
#include <vector>

class AnalogTimeSeries;

/**
 * @struct CSVAnalogSaverOptions
 * 
 * @brief Options for saving AnalogTimeSeries data to a CSV file.
 *          The CSV will typically have two columns: Time and Data.
 * 
 * @var CSVAnalogSaverOptions::filename
 * The name of the file to save the data to (e.g., "analog_data.csv").
 * 
 * @var CSVAnalogSaverOptions::parent_dir
 * The directory where the file will be saved. Defaults to ".".
 * 
 * @var CSVAnalogSaverOptions::delimiter
 * The delimiter to use between columns. Defaults to ",".
 * 
 * @var CSVAnalogSaverOptions::line_delim
 * The line delimiter to use. Defaults to "\n".
 * 
 * @var CSVAnalogSaverOptions::save_header
 * Whether to save a header row. Defaults to true.
 * 
 * @var CSVAnalogSaverOptions::header
 * The header string to use if save_header is true. Defaults to "Time,Data".
 * 
 * @var CSVAnalogSaverOptions::precision
 * The number of decimal places for floating point data values. Defaults to 6.
 */
struct CSVAnalogSaverOptions {
    std::string filename = "analog_output.csv"; 
    std::string parent_dir = ".";
    std::string delimiter = ",";
    std::string line_delim = "\n";
    bool save_header = true;
    std::string header = "Time,Data";
    int precision = 2;
};

/**
 * @brief load_analog_series_from_csv
 *
 * Loads an analog time series from a CSV file assuming a single column of data.
 * The time is assumed to be the line number (0-indexed).
 *
 * @param filename - the name of the file to load
 * @return a vector of floats representing the analog time series
 */
std::vector<float> load_analog_series_from_csv(std::string const & filename);

/**
 * @brief Saves an AnalogTimeSeries object to a CSV file using specified options.
 * 
 * @param analog_data Pointer to the AnalogTimeSeries object to save.
 * @param opts Configuration options for saving.
 */
void save_analog_series_to_csv(
        AnalogTimeSeries * analog_data,
        CSVAnalogSaverOptions & opts);


#endif// ANALOG_TIME_SERIES_CSV_HPP
