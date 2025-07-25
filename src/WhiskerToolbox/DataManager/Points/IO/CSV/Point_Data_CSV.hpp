#ifndef POINT_DATA_CSV_HPP
#define POINT_DATA_CSV_HPP

#include "CoreGeometry/points.hpp"
#include "TimeFrame.hpp"

#include <map>
#include <memory>
#include <string>

class PointData;

struct CSVPointLoaderOptions {
    std::string filename;
    int frame_column = 0;
    int x_column = 1;
    int y_column = 2;
    char column_delim = ' ';
};

std::map<TimeFrameIndex, Point2D<float>> load(CSVPointLoaderOptions const & opts);

/**
 * @struct CSVPointSaverOptions
 * 
 * @brief Options for saving points to a CSV file.
 *          
 * @var CSVPointSaverOptions::filename
 * The full filepath to save the points to.
 * 
 * @var CSVPointSaverOptions::delimiter
 * The delimiter to use between columns.
 * 
 * @var CSVPointSaverOptions::line_delim
 * The line delimiter to use.
 */
struct CSVPointSaverOptions {
    std::string filename; 
    std::string parent_dir = ".";
    std::string delimiter = ",";
    std::string line_delim = "\n";
    bool save_header = true;
    std::string header = "frame,x,y";
};

void save(PointData const * point_data, CSVPointSaverOptions const & opts);

std::map<std::string, std::map<TimeFrameIndex, Point2D<float>>> load_multiple_points_from_csv(std::string const & filename, int frame_column);


#endif// POINT_DATA_CSV_HPP
