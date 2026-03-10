#ifndef ANALOG_TIME_SERIES_CSV_HPP
#define ANALOG_TIME_SERIES_CSV_HPP

#include "ParameterSchema/ParameterSchema.hpp"
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
 * @brief Save AnalogTimeSeries to a CSV file.
 *
 * Uses atomic writes: data is written to a temporary file and then
 * renamed over the target to prevent corruption on crash.
 *
 * @param analog_data Non-null pointer to the AnalogTimeSeries to save.
 * @param opts        Saver options (directory, filename, delimiters, header, precision).
 * @return true on success, false on I/O error.
 *
 * @pre analog_data must not be null.
 */
bool save(AnalogTimeSeries const * analog_data,
          CSVAnalogSaverOptions const & opts);

namespace WhiskerToolbox::Transforms::V2 {

template<>
struct ParameterUIHints<CSVAnalogSaverOptions> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("filename")) {
            f->tooltip = "Output filename (combined with parent_dir)";
        }
        if (auto * f = schema.field("parent_dir")) {
            f->tooltip = "Directory in which to create the output file";
        }
        if (auto * f = schema.field("delimiter")) {
            f->tooltip = "Delimiter between time and data columns";
        }
        if (auto * f = schema.field("line_delim")) {
            f->tooltip = "Line delimiter (newline character)";
        }
        if (auto * f = schema.field("save_header")) {
            f->tooltip = "Whether to write a header row as the first line";
        }
        if (auto * f = schema.field("header")) {
            f->tooltip = "Header text to write when save_header is true";
        }
        if (auto * f = schema.field("precision")) {
            f->tooltip = "Number of decimal places for floating-point data values";
            f->min_value = 0.0;
            f->max_value = 15.0;
        }
    }
};

}// namespace WhiskerToolbox::Transforms::V2

#endif// ANALOG_TIME_SERIES_CSV_HPP
