#ifndef DIGITAL_EVENT_SERIES_CSV_HPP
#define DIGITAL_EVENT_SERIES_CSV_HPP

#include "datamanagerio_export.h"

#include "ParameterSchema/ParameterSchema.hpp"

#include <memory>
#include <string>
#include <vector>

class DigitalEventSeries;

/**
 * @struct CSVEventLoaderOptions
 *
 * @brief Options for loading DigitalEventSeries data from a CSV file.
 *        The CSV can have a single column with event timestamps, or multiple columns
 *        where one column contains timestamps and another contains identifiers.
 *
 * @var CSVEventLoaderOptions::filepath
 * The path to the CSV file to load.
 *
 * @var CSVEventLoaderOptions::delimiter
 * The delimiter used between columns. Defaults to ",".
 *
 * @var CSVEventLoaderOptions::has_header
 * Whether the file has a header row that should be skipped. Defaults to false.
 *
 * @var CSVEventLoaderOptions::event_column
 * The column index (0-based) for the event timestamp values. Defaults to 0.
 *
 * @var CSVEventLoaderOptions::identifier_column
 * The column index (0-based) for the identifier values. Set to -1 if no identifier column. Defaults to -1.
 *
 * @var CSVEventLoaderOptions::base_name
 * The base name for the data. If identifier_column is specified, series will be named base_name + "_" + identifier.
 * If no identifier column, the single series will be named base_name. Defaults to "events".
 */
struct CSVEventLoaderOptions {
    std::string filepath;
    std::string delimiter = ",";
    bool has_header = false;
    int event_column = 0;
    int identifier_column = -1;// -1 means no identifier column
    std::string base_name = "events";

    /// Scale factor to apply to timestamps. Applied BEFORE conversion to integer.
    /// For example, if timestamps are in seconds and you want sample indices at 30kHz,
    /// set scale=30000. The timestamp 0.01493 becomes 0.01493 * 30000 = 447.9 → 448.
    float scale = 1.0f;

    /// If true, divide by scale instead of multiplying.
    bool scale_divide = false;
};

/**
 * @struct CSVEventSaverOptions
 *
 * @brief Options for saving DigitalEventSeries data to a CSV file.
 *        The CSV will have one column with event timestamps.
 *
 * @var CSVEventSaverOptions::filename
 * The name of the file to save the data to (e.g., "events.csv").
 *
 * @var CSVEventSaverOptions::parent_dir
 * The directory where the file will be saved. Defaults to ".".
 *
 * @var CSVEventSaverOptions::delimiter
 * The delimiter to use between columns (if multiple columns). Defaults to ",".
 *
 * @var CSVEventSaverOptions::line_delim
 * The line delimiter to use. Defaults to "\n".
 *
 * @var CSVEventSaverOptions::save_header
 * Whether to save a header row. Defaults to true.
 *
 * @var CSVEventSaverOptions::header
 * The header string to use if save_header is true. Defaults to "Event".
 *
 * @var CSVEventSaverOptions::precision
 * The number of decimal places for floating point numbers. Defaults to 3.
 */
struct CSVEventSaverOptions {
    std::string filename = "events_output.csv";
    std::string parent_dir = ".";
    std::string delimiter = ",";
    std::string line_delim = "\n";
    bool save_header = true;
    std::string header = "Event";
    int precision = 3;
};

/**
 * @brief Load digital event series data from CSV using specified options
 *
 * This function can handle two scenarios:
 * 1. Single column CSV: Creates one DigitalEventSeries with the base_name
 * 2. Multi-column CSV: Creates one DigitalEventSeries per unique identifier found,
 *    each named as base_name + "_" + identifier
 *
 * @param options Configuration options for loading
 * @return Vector of shared pointers to DigitalEventSeries objects, one per unique identifier
 *         (or one total if no identifier column)
 */
DATAMANAGERIO_EXPORT std::vector<std::shared_ptr<DigitalEventSeries>> load(CSVEventLoaderOptions const & options);

/**
 * @brief Save DigitalEventSeries to a CSV file.
 *
 * Uses atomic writes: data is written to a temporary file and then
 * renamed over the target to prevent corruption on crash.
 *
 * @param event_data Non-null pointer to the DigitalEventSeries to save.
 * @param opts       Saver options (directory, filename, delimiters, header, precision).
 * @return true on success, false on I/O error.
 *
 * @pre event_data must not be null.
 */
DATAMANAGERIO_EXPORT bool save(DigitalEventSeries const * event_data,
          CSVEventSaverOptions const & opts);

namespace WhiskerToolbox::Transforms::V2 {

template<>
struct ParameterUIHints<CSVEventSaverOptions> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("filename")) {
            f->tooltip = "Output filename (combined with parent_dir)";
        }
        if (auto * f = schema.field("parent_dir")) {
            f->tooltip = "Directory in which to create the output file";
        }
        if (auto * f = schema.field("delimiter")) {
            f->tooltip = "Delimiter between columns (if multiple columns)";
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
            f->tooltip = "Number of decimal places for floating-point event times";
            f->min_value = 0.0;
            f->max_value = 15.0;
        }
    }
};

}// namespace WhiskerToolbox::Transforms::V2

#endif// DIGITAL_EVENT_SERIES_CSV_HPP
