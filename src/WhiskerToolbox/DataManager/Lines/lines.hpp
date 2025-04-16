#ifndef DATAMANAGER_LINES_HPP
#define DATAMANAGER_LINES_HPP

#include "Points/points.hpp"

#include <cmath>
#include <vector>

using Line2D = std::vector<Point2D<float>>;


inline Line2D create_line(std::vector<float> const & x, std::vector<float> const & y) {
    auto new_line = Line2D{x.size()};

    for (std::size_t i = 0; i < x.size(); i++) {
        new_line[i] = Point2D<float>{x[i], y[i]};
    }

    return new_line;
}


inline void smooth_line(Line2D & line) {
    if (line.size() < 3) return;// No need to smooth if less than 3 points

    Line2D smoothed_line;
    smoothed_line.push_back(line.front());// First point remains the same

    for (std::size_t i = 1; i < line.size() - 1; ++i) {
        float const smoothed_x = (line[i - 1].x + line[i].x + line[i + 1].x) / 3.0f;
        float const smoothed_y = (line[i - 1].y + line[i].y + line[i + 1].y) / 3.0f;
        smoothed_line.push_back(Point2D<float>{smoothed_x, smoothed_y});
    }

    smoothed_line.push_back(line.back());// Last point remains the same
    line = std::move(smoothed_line);
}


inline std::vector<uint8_t> line_to_image(Line2D & line, int height, int width) {
    auto image = std::vector<uint8_t>(static_cast<size_t>(height * width));

    for (auto point: line) {
        auto x = std::lround(point.x);
        auto y = std::lround(point.y);

        auto index = width * y + x;
        image[index] = 255;
    }

    return image;
}


#endif// DATAMANAGER_LINES_HPP
