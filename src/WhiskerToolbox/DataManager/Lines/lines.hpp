#ifndef DATAMANAGER_LINES_HPP
#define DATAMANAGER_LINES_HPP

#include "Points/points.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

using Line2D = std::vector<Point2D<float>>;


inline Line2D create_line(std::vector<float> const& x, std::vector<float> const& y)
{
    auto new_line = Line2D{x.size()};

    for (std::size_t i = 0; i < x.size(); i++)
    {
        new_line[i] = Point2D<float>{x[i],y[i]};
    }

    return new_line;
}


inline void smooth_line(Line2D& line)
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


inline std::vector<uint8_t> line_to_image(Line2D& line, int height, int width)
{
    auto image = std::vector<uint8_t>(height * width);

    for (auto point : line) {
        auto x = std::lround(point.x);
        auto y = std::lround(point.y);

        auto index = width * y + x;
        image[index] = 255;
    }

    return image;
}


inline void save_line_as_csv(Line2D const& line, std::string const& filename, int const point_precision = 2)
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

inline Line2D load_line_from_csv(std::string const& filename)
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



#endif // DATAMANAGER_LINES_HPP
