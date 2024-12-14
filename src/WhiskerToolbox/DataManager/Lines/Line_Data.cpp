
#include "Line_Data.hpp"
#include "utils/container.hpp"

#include <cmath>
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
    auto new_line = create_line(x,y);

    _data[time].push_back(new_line);
}

void LineData::addLineAtTime(int const time, std::vector<Point2D<float>> const & line)
{
    _data[time].push_back(std::move(line));
}

void LineData::addPointToLine(int const time, int const line_id, const float x, const float y)
{
    // make sure line_id is valid
    if (line_id < _data[time].size())
    {
        _data[time][line_id].push_back(Point2D<float>{x,y});
    } else {
        std::cerr << "LineData::addPointToLine: line_id out of range" << std::endl;
        //Create a new line
        _data[time].push_back(Line2D{});
        _data[time].back().push_back(Point2D<float>{x,y});
    }
}

void LineData::addPointToLineInterpolate(int const time, int const line_id, const float x, const float y)
{
    // Check if line_id is valid
    if (line_id >= _data[time].size())
    {
        std::cerr << "LineData::addPointToLineInterpolate: line_id out of range" << std::endl;
        _data[time].push_back(Line2D{});
    }

    Line2D& line = _data[time][line_id];

    // Add the new point
    Point2D<float> new_point{x, y};
    if (!line.empty())
    {
        Point2D<float> last_point = line.back();

        // Calculate the distance between the last point and the new point
        float distance = std::sqrt(std::pow(new_point.x - last_point.x, 2) + std::pow(new_point.y - last_point.y, 2));
        int n = static_cast<int>(distance / 2.0f); // Number of interpolated points

        // Add interpolated points
        for (int i = 1; i <= n; ++i)
        {
            float t = static_cast<float>(i) / (n + 1);
            float interp_x = last_point.x + t * (new_point.x - last_point.x);
            float interp_y = last_point.y + t * (new_point.y - last_point.y);
            line.push_back(Point2D<float>{interp_x, interp_y});
        }
    }

    // Add the new point to the line
    line.push_back(new_point);

    // Smooth the points in the line
    smooth_line(line);
}

std::vector<Line2D> const& LineData::getLinesAtTime(int const time) const
{
    // [] operator is not const because it inserts if mask is not present
    if (_data.find(time) != _data.end())
    {
        return _data.at(time);
    } else {
        return _empty;
    }
}

std::vector<int> LineData::getTimesWithLines() const
{
    std::vector<int> keys;
    keys.reserve(_data.size());
    for (auto kv : _data) {
        keys.push_back(kv.first);
    }
    return keys;
}

Line2D create_line(std::vector<float> const& x, std::vector<float> const& y)
{
    auto new_line = Line2D{x.size()};

    for (std::size_t i = 0; i < x.size(); i++)
    {
        new_line[i] = Point2D<float>{x[i],y[i]};
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

        line_output.push_back(Point2D<float>{std::stof(x_str),std::stof(y_str)});
    }

    return line_output;
}

void smooth_line(Line2D& line)
{
    if (line.size() < 3) return; // No need to smooth if less than 3 points

    Line2D smoothed_line;
    smoothed_line.push_back(line.front()); // First point remains the same

    for (std::size_t i = 1; i < line.size() - 1; ++i)
    {
        float smoothed_x = (line[i - 1].x + line[i].x + line[i + 1].x) / 3.0f;
        float smoothed_y = (line[i - 1].y + line[i].y + line[i + 1].y) / 3.0f;
        smoothed_line.push_back(Point2D<float>{smoothed_x, smoothed_y});
    }

    smoothed_line.push_back(line.back()); // Last point remains the same
    line = std::move(smoothed_line);
}
