
#include "Point_Data.hpp"
#include "utils/container.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

PointData::PointData()
{

}

void PointData::clearPointsAtTime(int const time)
{
    _data[time].clear();
}

void PointData::addPointAtTime(int const time,float const x, float const y)
{
    _data[time].push_back(Point2D<float>{x,y});
}

std::vector<int> PointData::getTimesWithPoints() const
{
    std::vector<int> keys;
    keys.reserve(_data.size());
    for (auto kv : _data) {
        keys.push_back(kv.first);
    }
    return keys;
}

std::vector<Point2D<float>> const& PointData::getPointsAtTime(int const time) const
{
    // [] operator is not const because it inserts if mask is not present
    if (_data.find(time) != _data.end())
    {
        return _data.at(time);
    } else {
        return _empty;
    }
}

std::map<int,Point2D<float>> load_points_from_csv(std::string const& filename, int const frame_column, int const x_column, int const y_column)
{
    std::string csv_line;

    char const column_delim = ',';

    auto line_output = std::map<int,Point2D<float>>{};

    std::fstream myfile;
    myfile.open (filename, std::fstream::in);

    std::string x_str;
    std::string y_str;
    std::string frame_str;
    std::string col_value;

    while (getline(myfile, csv_line)) {

        std::stringstream ss(csv_line);

        int cols_read = 0;
        while (getline(ss, col_value)) {
            if (cols_read == frame_column) {
                frame_str = col_value;
            } else if (cols_read == x_column) {
                x_str = col_value;
            } else if (cols_read == y_column) {
                y_str = col_value;
            }
            cols_read++;
        }

        line_output[std::stoi(frame_str)]=Point2D<float>{std::stof(x_str),std::stof(y_str)};
    }

    return line_output;
}
