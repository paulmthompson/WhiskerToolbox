#ifndef POINT_DATA_CSV_HPP
#define POINT_DATA_CSV_HPP

#include "CoreGeometry/points.hpp"
#include "TimeFrame/TimeFrame.hpp"

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

/**
 * @struct DLCPointLoaderOptions
 * 
 * @brief Options for loading DLC (DeepLabCut) format CSV files.
 *          
 * @var DLCPointLoaderOptions::filename
 * The full filepath to the DLC CSV file.
 * 
 * @var DLCPointLoaderOptions::frame_column
 * The column index for frame numbers (usually 0).
 * 
 * @var DLCPointLoaderOptions::likelihood_threshold
 * Minimum likelihood score for points to be included (default 0.0).
 */
struct DLCPointLoaderOptions {
    std::string filename;
    int frame_column = 0;
    float likelihood_threshold = 0.0f;
};

std::map<std::string, std::map<TimeFrameIndex, Point2D<float>>> load_dlc_csv(DLCPointLoaderOptions const & opts);


#endif// POINT_DATA_CSV_HPP
