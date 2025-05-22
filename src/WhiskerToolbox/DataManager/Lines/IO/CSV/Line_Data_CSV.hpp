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

void save_line_as_csv(Line2D const & line, std::string const & filename, int point_precision = 2);

void save_lines_csv(
        LineData const * line_data,
        CSVSingleFileLineSaverOptions & opts);


std::vector<float> parse_string_to_float_vector(std::string const & str);

std::map<int, std::vector<Line2D>> load_line_csv(std::string const & filepath);

Line2D load_line_from_csv(std::string const & filename);


#endif// LINE_DATA_LOADER_HPP
