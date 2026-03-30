#ifndef LINE_DATA_LOADER_HPP
#define LINE_DATA_LOADER_HPP

#include "datamanagerio_export.h"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"
#include "ParameterSchema/ParameterSchema.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "IO/core/LoaderOptionsConcepts.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

class LineData;

/**
 * @struct CSVSingleFileLineSaverOptions
 *
 * @brief Options for saving 2D LineData into single CSV file.
 *
 * The first column represents a frame, while the 2nd column is X values, and 3rd is y values
 *
 * @var CSVSingleFileLineSaverOptions::filename
 * The full filepath to save the points to.
 *
 * @var CSVSingleFileLineSaverOptions::delimiter
 * The delimiter to use between columns.
 *
 * @var CSVSingleFileLineSaverOptions::line_delim
 * The line delimiter to use.
 *
 * @var CSVSingleFileLineSaverOptions::precision
 * The number of decimals to include with floating point numbers
 */
struct CSVSingleFileLineSaverOptions {
    std::string filename;
    std::string parent_dir = ".";
    std::string delimiter = ",";
    std::string line_delim = "\n";
    bool save_header = true;
    std::string header = "Frame,X,Y";
    int precision = 1;
};

/**
 * @struct CSVMultiFileLineSaverOptions
 *
 * @brief Options for saving 2D LineData into multiple CSV files, one per timestamp.
 *
 * Each file contains X values in one column and Y values in another column.
 * Only the first line (index 0) at each timestamp is saved.
 * Filenames are zero-padded timestamps with .csv extension.
 *
 * @var CSVMultiFileLineSaverOptions::parent_dir
 * The directory where CSV files will be saved.
 *
 * @var CSVMultiFileLineSaverOptions::delimiter
 * The delimiter to use between columns.
 *
 * @var CSVMultiFileLineSaverOptions::line_delim
 * The line delimiter to use.
 *
 * @var CSVMultiFileLineSaverOptions::precision
 * The number of decimals to include with floating point numbers.
 *
 * @var CSVMultiFileLineSaverOptions::save_header
 * Whether to include a header row in each CSV file.
 *
 * @var CSVMultiFileLineSaverOptions::header
 * The header text to use if save_header is true.
 *
 * @var CSVMultiFileLineSaverOptions::frame_id_padding
 * Number of digits for zero-padding frame numbers in filenames (e.g., 7 for 0000123.csv).
 *
 * @var CSVMultiFileLineSaverOptions::overwrite_existing
 * If true, overwrite existing files. If false, skip existing files and continue with next file.
 */
struct CSVMultiFileLineSaverOptions {
    std::string parent_dir = ".";
    std::string delimiter = ",";
    std::string line_delim = "\n";
    bool save_header = true;
    std::string header = "X,Y";
    int precision = 1;
    int frame_id_padding = 7;
    bool overwrite_existing = false;
};

/**
 * @struct CSVMultiFileLineLoaderOptions
 *
 * @brief Options for loading 2D LineData from multiple CSV files, one per timestamp.
 *
 * Each CSV file should contain X values in one column and Y values in another column.
 * The filename (without extension) is parsed as the timestamp/frame number.
 * Files are expected to be in the format: NNNNNN.csv (zero-padded frame numbers).
 *
 * Uses reflect-cpp for automatic JSON serialization/deserialization.
 * Optional fields can be omitted from JSON and will use default values.
 *
 * @note This struct uses parent_dir instead of filepath since it loads from a directory.
 */
struct CSVMultiFileLineLoaderOptions {
    std::string parent_dir;// Directory containing CSV files (required)

    std::optional<std::string> delimiter;
    std::optional<rfl::Validator<int, rfl::Minimum<0>>> x_column;
    std::optional<rfl::Validator<int, rfl::Minimum<0>>> y_column;
    std::optional<bool> has_header;
    std::optional<std::string> file_pattern;

    // Helper methods to get values with defaults
    std::string getDelimiter() const { return delimiter.value_or(","); }
    int getXColumn() const { return x_column.has_value() ? x_column.value().value() : 0; }
    int getYColumn() const { return y_column.has_value() ? y_column.value().value() : 1; }
    bool getHasHeader() const { return has_header.value_or(true); }
    std::string getFilePattern() const { return file_pattern.value_or("*.csv"); }
};

/**
 * @struct CSVSingleFileLineLoaderOptions
 *
 * @brief Options for loading 2D LineData from a single CSV file containing all timestamps.
 *
 * The CSV file should have a format where the first column is the frame number,
 * followed by comma-separated X coordinates and comma-separated Y coordinates.
 * X and Y coordinate lists are typically enclosed in quotes.
 *
 * Uses reflect-cpp for automatic JSON serialization/deserialization.
 * Optional fields can be omitted from JSON and will use default values.
 *
 * @note This struct conforms to ValidLoaderOptions concept.
 */
struct CSVSingleFileLineLoaderOptions {
    std::string filepath;// Path to the CSV file (required, consistent with DataManager JSON)

    std::optional<std::string> delimiter;
    std::optional<std::string> coordinate_delimiter;
    std::optional<bool> has_header;
    std::optional<std::string> header_identifier;

    // Helper methods to get values with defaults
    std::string getDelimiter() const { return delimiter.value_or(","); }
    std::string getCoordinateDelimiter() const { return coordinate_delimiter.value_or(","); }
    bool getHasHeader() const { return has_header.value_or(true); }
    std::string getHeaderIdentifier() const { return header_identifier.value_or("Frame"); }
};

// Compile-time validation that CSVSingleFileLineLoaderOptions conforms to loader requirements
static_assert(WhiskerToolbox::ValidLoaderOptions<CSVSingleFileLineLoaderOptions>,
              "CSVSingleFileLineLoaderOptions must have 'filepath' field and must not have 'data_type' or 'name' fields");

void save_line_as_csv(Line2D const & line, std::string const & filename, int point_precision = 2);

/**
 * @brief Save LineData to a single CSV file.
 *
 * Uses atomic writes: data is written to a temporary file and then
 * renamed over the target to prevent corruption on crash.
 *
 * @param line_data Non-null pointer to the LineData to save.
 * @param opts      Saver options (directory, filename, delimiters, header, precision).
 * @return true on success, false on I/O error.
 *
 * @pre line_data must not be null.
 */
DATAMANAGERIO_EXPORT bool save(LineData const * line_data,
          CSVSingleFileLineSaverOptions const & opts);

/**
 * @brief Save LineData to multiple CSV files, one per timestamp
 *
 * Creates one CSV file per timestamp containing X and Y coordinates in separate columns.
 * Only saves the first line (index 0) at each timestamp.
 * Filenames are zero-padded frame numbers with .csv extension.
 *
 * @param line_data The LineData object to save
 * @param opts Options controlling the save behavior
 */
/**
 * @brief Save LineData to multiple CSV files, one per timestamp.
 *
 * Creates one CSV file per timestamp containing X and Y coordinates in separate columns.
 * Only saves the first line (index 0) at each timestamp.
 * Filenames are zero-padded frame numbers with .csv extension.
 * Each per-frame file is written atomically.
 *
 * @param line_data Non-null pointer to the LineData to save.
 * @param opts      Options controlling the save behavior.
 * @return true on success, false if any file write failed.
 *
 * @pre line_data must not be null.
 */
DATAMANAGERIO_EXPORT bool save(LineData const * line_data,
          CSVMultiFileLineSaverOptions const & opts);

/**
 * @brief Load LineData from multiple CSV files, one per timestamp
 *
 * Loads CSV files from a directory where each file represents one timestamp.
 * The filename (without extension) is parsed as the frame number.
 * Each file should contain X and Y coordinates in separate columns.
 *
 * @param opts Options controlling the load behavior
 * @return A map of timestamps to vectors of Line2D objects
 */
DATAMANAGERIO_EXPORT std::map<TimeFrameIndex, std::vector<Line2D>> load(CSVMultiFileLineLoaderOptions const & opts);

/**
 * @brief Load LineData from a single CSV file containing all timestamps
 *
 * Loads line data from a single CSV file where each row contains a frame number
 * followed by comma-separated X coordinates and comma-separated Y coordinates.
 * This function wraps the optimized load_line_csv function with configurable options.
 *
 * @param opts Options controlling the load behavior
 * @return A map of timestamps to vectors of Line2D objects
 */
DATAMANAGERIO_EXPORT std::map<TimeFrameIndex, std::vector<Line2D>> load(CSVSingleFileLineLoaderOptions const & opts);

std::vector<float> parse_string_to_float_vector(std::string const & str, std::string const & delimiter = ",");

std::map<TimeFrameIndex, std::vector<Line2D>> load_line_csv(std::string const & filepath);

Line2D load_line_from_csv(std::string const & filename);


template<>
struct ParameterUIHints<CSVSingleFileLineSaverOptions> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("filename")) {
            f->tooltip = "Output filename (combined with parent_dir)";
        }
        if (auto * f = schema.field("parent_dir")) {
            f->tooltip = "Directory in which to create the output file";
        }
        if (auto * f = schema.field("delimiter")) {
            f->tooltip = "Delimiter between coordinate values within X/Y lists";
        }
        if (auto * f = schema.field("save_header")) {
            f->tooltip = "Whether to write a header row as the first line";
        }
        if (auto * f = schema.field("header")) {
            f->tooltip = "Header text to write when save_header is true";
        }
        if (auto * f = schema.field("precision")) {
            f->tooltip = "Number of decimal places for floating-point coordinates";
            f->min_value = 0.0;
            f->max_value = 15.0;
        }
    }
};

template<>
struct ParameterUIHints<CSVMultiFileLineSaverOptions> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("parent_dir")) {
            f->tooltip = "Directory where per-frame CSV files will be saved";
        }
        if (auto * f = schema.field("delimiter")) {
            f->tooltip = "Delimiter between X and Y columns";
        }
        if (auto * f = schema.field("save_header")) {
            f->tooltip = "Whether to include a header row in each CSV file";
        }
        if (auto * f = schema.field("header")) {
            f->tooltip = "Header text for each file when save_header is true";
        }
        if (auto * f = schema.field("precision")) {
            f->tooltip = "Number of decimal places for floating-point coordinates";
            f->min_value = 0.0;
            f->max_value = 15.0;
        }
        if (auto * f = schema.field("frame_id_padding")) {
            f->tooltip = "Number of digits for zero-padding frame numbers in filenames";
            f->min_value = 1.0;
            f->max_value = 10.0;
        }
        if (auto * f = schema.field("overwrite_existing")) {
            f->tooltip = "If true, overwrite existing files; otherwise skip them";
        }
    }
};

#endif// LINE_DATA_LOADER_HPP
