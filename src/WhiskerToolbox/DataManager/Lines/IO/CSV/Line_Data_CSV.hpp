#ifndef LINE_DATA_LOADER_HPP
#define LINE_DATA_LOADER_HPP

#include "Lines/lines.hpp"

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
 */
struct CSVMultiFileLineSaverOptions {
    std::string parent_dir = ".";
    std::string delimiter = ",";
    std::string line_delim = "\n";
    bool save_header = true;
    std::string header = "X,Y";
    int precision = 1;
    int frame_id_padding = 7;
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

std::vector<float> parse_string_to_float_vector(std::string const & str);

std::map<int, std::vector<Line2D>> load_line_csv(std::string const & filepath);

Line2D load_line_from_csv(std::string const & filename);


#endif// LINE_DATA_LOADER_HPP
