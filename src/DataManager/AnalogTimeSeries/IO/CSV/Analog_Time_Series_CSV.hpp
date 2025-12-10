#ifndef ANALOG_TIME_SERIES_CSV_HPP
#define ANALOG_TIME_SERIES_CSV_HPP

#include "utils/LoaderOptionsConcepts.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <string>
#include <vector>

class AnalogTimeSeries;

/**
 * @brief CSV analog data loader options with validation
 * 
 * Uses reflect-cpp for automatic JSON serialization/deserialization
 * and includes validators to ensure data integrity.
 * Optional fields can be omitted from JSON and will use default values.
 * 
 * @note This struct conforms to ValidLoaderOptions concept.
 */
struct CSVAnalogLoaderOptions {
    std::string filepath;
    
    // Common delimiters: comma, tab, semicolon, pipe, space
    std::optional<std::string> delimiter;
    
    std::optional<bool> has_header;
    std::optional<bool> single_column_format;
    
    // Column indices should be non-negative
    std::optional<rfl::Validator<int, rfl::Minimum<0>>> time_column;
    std::optional<rfl::Validator<int, rfl::Minimum<0>>> data_column;
    
    // Helper methods to get values with defaults
    std::string getDelimiter() const { return delimiter.value_or(","); }
    bool getHasHeader() const { return has_header.value_or(false); }
    bool getSingleColumnFormat() const { return single_column_format.value_or(true); }
    int getTimeColumn() const { return time_column.has_value() ? time_column.value().value() : 0; }
    int getDataColumn() const { return data_column.has_value() ? data_column.value().value() : 1; }
};

// Compile-time validation that CSVAnalogLoaderOptions conforms to loader requirements
static_assert(WhiskerToolbox::ValidLoaderOptions<CSVAnalogLoaderOptions>,
    "CSVAnalogLoaderOptions must have 'filepath' field and must not have 'data_type' or 'name' fields");

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
 * @brief Load analog time series data from CSV using specified options
 *
 * @param options Configuration options for loading
 * @return Shared pointer to AnalogTimeSeries object
 */
std::shared_ptr<AnalogTimeSeries> load(CSVAnalogLoaderOptions const & options);


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
 * @brief Saves an AnalogTimeSeries object to a CSV file using specified options.
 * 
 * @param analog_data Pointer to the AnalogTimeSeries object to save.
 * @param opts Configuration options for saving.
 */
void save(AnalogTimeSeries * analog_data,
          CSVAnalogSaverOptions & opts);


#endif// ANALOG_TIME_SERIES_CSV_HPP
