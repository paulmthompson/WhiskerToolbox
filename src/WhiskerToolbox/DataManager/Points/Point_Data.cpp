#include "Point_Data.hpp"

#include <algorithm>
#include <iostream>

PointData::PointData(std::map<int, Point2D<float>> const & data) {
    for (auto const & [time, point]: data) {
        _data[time].push_back(point);
    }
}

PointData::PointData(std::map<int, std::vector<Point2D<float>>> const & data) 
    : _data(data) {
}

void PointData::clearAtTime(int const time, bool notify) {
    auto it = _data.find(time);
    if (it != _data.end()) {
        _data.erase(it);
    }

    if (notify) {
        notifyObservers();
    }
}

void PointData::overwritePointAtTime(int const time, Point2D<float> const point, bool notify) {
    _data[time] = {point};
    if (notify) {
        notifyObservers();
    }
}

void PointData::overwritePointsAtTime(int const time, std::vector<Point2D<float>> const & points, bool notify) {
    _data[time] = points;
    if (notify) {
        notifyObservers();
    }
}

void PointData::overwritePointsAtTimes(
        std::vector<int> const & times,
        std::vector<std::vector<Point2D<float>>> const & points,
        bool notify) {
    if (times.size() != points.size()) {
        std::cout << "overwritePointsAtTimes: times and points must be the same size" << std::endl;
        return;
    }

    for (std::size_t i = 0; i < times.size(); i++) {
        _data[times[i]] = points[i];
    }
    if (notify) {
        notifyObservers();
    }
}

void PointData::addPointAtTime(int const time, Point2D<float> const point, bool notify) {
    _data[time].push_back(point);

    if (notify) {
        notifyObservers();
    }
}

void PointData::addPointsAtTime(int const time, std::vector<Point2D<float>> const & points, bool notify) {
    _data[time].insert(_data[time].end(), points.begin(), points.end());
    
    if (notify) {
        notifyObservers();
    }
}

std::vector<int> PointData::getTimesWithData() const {
    std::vector<int> times;
    times.reserve(_data.size());
    for (auto const & [time, points] : _data) {
        times.push_back(time);
    }
    return times;
}

std::vector<Point2D<float>> const & PointData::getPointsAtTime(int const time) const {
    auto it = _data.find(time);
    if (it != _data.end()) {
        return it->second;
    } else {
        return _empty;
    }
}

std::size_t PointData::getMaxPoints() const {
    std::size_t max_points = 0;
    for (auto const & [time, points] : _data) {
        max_points = std::max(max_points, points.size());
    }
    return max_points;
}

void PointData::changeImageSize(ImageSize const & image_size) {
    if (_image_size.width == -1 || _image_size.height == -1) {
        std::cout << "No size set for current image."
                  << " Please set a valid image size before trying to scale" << std::endl;
        _image_size = image_size; // Set the image size if it wasn't set before
        return;
    }

    if (_image_size.width == image_size.width && _image_size.height == image_size.height) {
        std::cout << "Image size is the same. No need to scale" << std::endl;
        return;
    }

    float const scale_x = static_cast<float>(image_size.width) / static_cast<float>(_image_size.width);
    float const scale_y = static_cast<float>(image_size.height) / static_cast<float>(_image_size.height);

    for (auto & [time, points] : _data) {
        for (auto & point : points) {
            point.x *= scale_x;
            point.y *= scale_y;
        }
    }
    _image_size = image_size;
}
