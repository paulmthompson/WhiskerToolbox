
#include "Line_Data.hpp"

#include <fstream>
#include <iomanip>

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

std::vector<Line2D> LineData::getLinesAtTime(int const time)
{
    return _data[time];
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
