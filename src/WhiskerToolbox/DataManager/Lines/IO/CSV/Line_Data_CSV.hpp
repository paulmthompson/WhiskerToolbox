#ifndef LINE_DATA_LOADER_HPP
#define LINE_DATA_LOADER_HPP

#include "Lines/lines.hpp"

#include <map>
#include <string>
#include <vector>

class LineData;

void save_line_as_csv(Line2D const & line, std::string const & filename, int point_precision = 2);

void save_lines_csv(
        LineData const * line_data,
        std::string const & filename,
        std::string const & header = "Frame,X,Y");


std::vector<float> parse_string_to_float_vector(std::string const & str);

std::map<int, std::vector<Line2D>> load_line_csv(std::string const & filepath);

Line2D load_line_from_csv(std::string const & filename);


#endif// LINE_DATA_LOADER_HPP
