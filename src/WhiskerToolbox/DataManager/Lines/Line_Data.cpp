
#include "Line_Data.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

LineData::LineData()
{

}

void LineData::clearLinesAtTime(int const time)
{
    _data[time].clear();
}

void LineData::addLineAtTime(int const time, std::vector<float> const& x, std::vector<float> const& y)
{
    auto new_line = _createLine(x,y);

    _data[time].push_back(new_line);
}

void LineData::addLineAtTime(int const time, std::vector<Point2D> const line)
{
    _data[time].push_back(line);
}

std::vector<Line2D> const& LineData::getLinesAtTime(int const time) const
{
    // [] operator is not const because it inserts if mask is not present
    if (_data.find(time) != _data.end())
    {
        return _data.at(time);
    } else {
        std::cout << "Error. Mask does not exist at time " << std::to_string(time) << std::endl;
    }
}

Line2D LineData::_createLine(std::vector<float> const& x, std::vector<float> const& y)
{
    auto new_line = Line2D{x.size()};

    for (int i = 0; i < x.size(); i++)
    {
        new_line[i] = Point2D{x[i],y[i]};
    }

    return new_line;
}

void save_line_as_csv(Line2D const& line, std::string const& filename, int const point_precision)
{
    std::fstream myfile;
    myfile.open (filename, std::fstream::out);

    myfile << std::fixed << std::setprecision(point_precision);
    for (auto& point: line)
    {
        myfile << point.x << "," << point.y << "\n";
    }

    myfile.close();
}

Line2D load_line_from_csv(std::string const& filename)
{
    std::string csv_line;

    auto line_output = Line2D();

    std::fstream myfile;
    myfile.open (filename, std::fstream::in);

    std::string x_str;
    std::string y_str;

    while (getline(myfile, csv_line)) {

        std::stringstream ss(csv_line);

        getline(ss, x_str, ',');
        getline(ss, y_str);

        //std::cout << x_str << " , " << y_str << std::endl;

        line_output.push_back(Point2D{std::stof(x_str),std::stof(y_str)});
    }

    return line_output;
}
