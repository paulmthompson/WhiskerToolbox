
#include "Point_Data.hpp"
#include "utils/container.hpp"
#include "utils/string_manip.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>


PointData::PointData()
{

}

PointData::PointData(std::map<int, Point2D<float>> data)
{
    for (auto [key, value] : data)
    {
        _data[key].push_back(value);
    }
}

PointData::PointData(std::map<int,std::vector<Point2D<float>>> data)
{
    _data = data;
}

void PointData::clearPointsAtTime(int const time)
{
    if (_data.find(time) == _data.end())
    {
        return;
    }

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

//https://stackoverflow.com/questions/4654636/how-to-determine-if-a-string-is-a-number-with-c
bool is_number(const std::string& s)
{
    return !s.empty() && std::find_if(s.begin(),
                                      s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
}

std::map<int,Point2D<float>> load_points_from_csv(
    std::string const& filename,
    int const frame_column,
    int const x_column,
    int const y_column,
    char const column_delim)
{
    std::string csv_line;

    auto line_output = std::map<int,Point2D<float>>{};

    std::fstream myfile;
    myfile.open (filename, std::fstream::in);

    std::string x_str;
    std::string y_str;
    std::string frame_str;
    std::string col_value;

    std::vector<std::pair<int,Point2D<float>>> csv_vector = {};

    while (getline(myfile, csv_line)) {

        std::stringstream ss(csv_line);

        int cols_read = 0;
        while (getline(ss, col_value,column_delim)) {
            if (cols_read == frame_column) {
                frame_str = col_value;
            } else if (cols_read == x_column) {
                x_str = col_value;
            } else if (cols_read == y_column) {
                y_str = col_value;
            }
            cols_read++;
        }

        if (is_number(frame_str)) {
            //line_output[std::stoi(frame_str)]=Point2D<float>{std::stof(x_str),std::stof(y_str)};
            csv_vector.emplace_back(std::make_pair(std::stoi(frame_str),Point2D<float>{std::stof(x_str),std::stof(y_str)}));
        }
    }
    std::cout.flush();

    std::cout << "Read " << csv_vector.size() << " lines from " << filename << std::endl;

    line_output.insert(csv_vector.begin(), csv_vector.end());

    return line_output;
}

std::map<std::string, std::map<int, Point2D<float>>> load_multiple_points_from_csv(std::string const& filename, int const frame_column){
    std::fstream file;
    file.open(filename, std::fstream::in);

    std::string ln, ele;

    getline(file, ln); // skip the "scorer" row
    
    getline(file, ln); // bodyparts row
    std::vector<std::string> bodyparts;
    {
        std::stringstream ss(ln);
        while (getline(ss, ele, ',')){
            bodyparts.push_back(ele);
        }
    }

    getline(file, ln); // coords row
    std::vector<std::string> dims;
    {
        std::stringstream ss(ln);
        while (getline(ss, ele, ',')){
            dims.push_back(ele);
        }
    }

    std::map<std::string, std::map<int, Point2D<float>>> data;
    while (getline(file, ln)){
        std::stringstream ss(ln);
        int col_no = 0;
        int frame_no = -1;
        while (getline(ss, ele, ',')){
            if (col_no == frame_column){
                frame_no = std::stoi(extract_numbers_from_string(ele));
            } else if (dims[col_no] == "x"){
                data[bodyparts[col_no]][frame_no].x = std::stof(ele);
            } else if (dims[col_no] == "y"){
                data[bodyparts[col_no]][frame_no].y = std::stof(ele);
            }
            ++col_no;
        }
    }

    return data;
}
