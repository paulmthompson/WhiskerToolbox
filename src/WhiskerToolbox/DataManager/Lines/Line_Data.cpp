
#include "Line_Data.hpp"
#include "Points/points.hpp"

#include <cmath>
#include <iostream>


void LineData::clearLinesAtTime(int const time, bool notify) {

    _data[time].clear();

    if (notify) {
        notifyObservers();
    }
}

void LineData::clearLineAtTime(int const time, int const line_id, bool notify) {

    if (line_id < _data[time].size()) {
        _data[time].erase(_data[time].begin() + line_id);
    }

    if (notify) {
        notifyObservers();
    }
}

void LineData::addLineAtTime(int const time, std::vector<float> const & x, std::vector<float> const & y, bool notify) {

    auto new_line = create_line(x, y);
    _data[time].push_back(new_line);

    if (notify) {
        notifyObservers();
    }
}

void LineData::addLineAtTime(int const time, std::vector<Point2D<float>> const & line, bool notify) {
    _data[time].push_back(line);

    if (notify) {
        notifyObservers();
    }
}

void LineData::addPointToLine(int const time, int const line_id, Point2D<float> point, bool notify) {

    if (line_id < _data[time].size()) {
        _data[time][line_id].push_back(point);
    } else {
        std::cerr << "LineData::addPointToLine: line_id out of range" << std::endl;
        _data[time].emplace_back();
        _data[time].back().push_back(point);
    }

    if (notify) {
        notifyObservers();
    }
}

void LineData::addPointToLineInterpolate(int const time, int const line_id, Point2D<float> point, bool notify) {

    if (line_id >= _data[time].size()) {
        std::cerr << "LineData::addPointToLineInterpolate: line_id out of range" << std::endl;
        _data[time].emplace_back();
    }

    Line2D & line = _data[time][line_id];
    if (!line.empty()) {
        Point2D<float> const last_point = line.back();
        double const distance = std::sqrt(std::pow(point.x - last_point.x, 2) + std::pow(point.y - last_point.y, 2));
        int const n = static_cast<int>(distance / 2.0f);
        for (int i = 1; i <= n; ++i) {
            float const t = static_cast<float>(i) / static_cast<float>(n + 1);
            float const interp_x = last_point.x + t * (point.x - last_point.x);
            float const interp_y = last_point.y + t * (point.y - last_point.y);
            line.push_back(Point2D<float>{interp_x, interp_y});
        }
    }
    line.push_back(point);
    smooth_line(line);

    if (notify) {
        notifyObservers();
    }
}

std::vector<Line2D> const & LineData::getLinesAtTime(int const time) const {
    // [] operator is not const because it inserts if mask is not present
    if (_data.find(time) != _data.end()) {
        return _data.at(time);
    } else {
        return _empty;
    }
}

std::vector<int> LineData::getTimesWithData() const {
    std::vector<int> keys;
    keys.reserve(_data.size());
    for (auto const & kv: _data) {
        keys.push_back(kv.first);
    }
    return keys;
}

void LineData::changeImageSize(ImageSize const & image_size)
{
    if (_image_size.width == -1 || _image_size.height == -1) {
        std::cout << "No size set for current image. "
                  << " Please set a valid image size before trying to scale" << std::endl;
    }

    if (_image_size.width == image_size.width && _image_size.height == image_size.height) {
        std::cout << "Image size is the same. No need to scale" << std::endl;
        return;
    }

    float const scale_x = static_cast<float>(image_size.width) / static_cast<float>(_image_size.width);
    float const scale_y = static_cast<float>(image_size.height) / static_cast<float>(_image_size.height);

    for (auto & [time, lines] : _data) {
        for (auto & line : lines) {
            for (auto & point : line) {
                point.x *= scale_x;
                point.y *= scale_y;
            }
        }
    }
    _image_size = image_size;

}