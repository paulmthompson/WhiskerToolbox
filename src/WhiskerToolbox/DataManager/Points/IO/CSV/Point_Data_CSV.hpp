#ifndef POINT_DATA_CSV_HPP
#define POINT_DATA_CSV_HPP

#include "Points/points.hpp"

#include "nlohmann/json.hpp"

#include <memory>
#include <string>

class PointData;

std::shared_ptr<PointData> load_into_PointData(std::string const & file_path, nlohmann::basic_json<> const & item);

struct CSVPointLoaderOptions {
    std::string filename;
    int frame_column = 0;
    int x_column = 1;
    int y_column = 2;
    char column_delim = ' ';
};

std::map<int, Point2D<float>> load_points_from_csv(CSVPointLoaderOptions const & opts);

std::map<std::string, std::map<int, Point2D<float>>> load_multiple_points_from_csv(std::string const & filename, int frame_column);

#endif// POINT_DATA_CSV_HPP
