#ifndef LINE_DATA_LOADER_HPP
#define LINE_DATA_LOADER_HPP

#include "Lines/lines.hpp"
#include "TimeFrame.hpp"

#include <map>
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
 * @var CSVMultiFileLineLoaderOptions::parent_dir
 * The directory containing the CSV files to load.
 *
 * @var CSVMultiFileLineLoaderOptions::delimiter
 * The delimiter used between columns in the CSV files.
 *
 * @var CSVMultiFileLineLoaderOptions::x_column
 * The column index (0-based) containing X coordinates.
 *
 * @var CSVMultiFileLineLoaderOptions::y_column
 * The column index (0-based) containing Y coordinates.
 *
 * @var CSVMultiFileLineLoaderOptions::has_header
 * Whether the CSV files contain a header row that should be skipped.
 *
 * @var CSVMultiFileLineLoaderOptions::file_pattern
 * File pattern to match (e.g., "*.csv" for all CSV files).
 */
struct CSVMultiFileLineLoaderOptions {
    std::string parent_dir = ".";
    std::string delimiter = ",";
    int x_column = 0;
    int y_column = 1;
    bool has_header = true;
    std::string file_pattern = "*.csv";
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
 * @var CSVSingleFileLineLoaderOptions::filepath
 * The path to the CSV file to load.
 *
 * @var CSVSingleFileLineLoaderOptions::delimiter
 * The delimiter used between main columns (frame, X values, Y values).
 *
 * @var CSVSingleFileLineLoaderOptions::coordinate_delimiter
 * The delimiter used within X and Y coordinate lists.
 *
 * @var CSVSingleFileLineLoaderOptions::has_header
 * Whether the CSV file contains a header row that should be skipped.
 *
 * @var CSVSingleFileLineLoaderOptions::header_identifier
 * The text in the first column that identifies the header row.
 */
struct CSVSingleFileLineLoaderOptions {
    std::string filepath;
    std::string delimiter = ",";
    std::string coordinate_delimiter = ",";
    bool has_header = true;
    std::string header_identifier = "Frame";
};

void save_line_as_csv(Line2D const & line, std::string const & filename, int point_precision = 2);

void save(LineData const * line_data,
          CSVSingleFileLineSaverOptions & opts);

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
void save(LineData const * line_data,
          CSVMultiFileLineSaverOptions & opts);

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
std::map<TimeFrameIndex, std::vector<Line2D>> load(CSVMultiFileLineLoaderOptions const & opts);

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
std::map<TimeFrameIndex, std::vector<Line2D>> load(CSVSingleFileLineLoaderOptions const & opts);

std::vector<float> parse_string_to_float_vector(std::string const & str, std::string const & delimiter = ",");

std::map<TimeFrameIndex, std::vector<Line2D>> load_line_csv(std::string const & filepath);

Line2D load_line_from_csv(std::string const & filename);


#endif// LINE_DATA_LOADER_HPP
