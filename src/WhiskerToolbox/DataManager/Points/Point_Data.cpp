
#include "Point_Data.hpp"


#include <algorithm>
#include <iostream>

PointData::PointData(std::map<int, Point2D<float>> const & data) {
    for (auto const & [key, value]: data) {
        auto it = std::find(_time.begin(), _time.end(), key);
        if (it != _time.end()) {
            size_t index = std::distance(_time.begin(), it);
            _data[index].push_back(value);
        } else {
            _time.push_back(key);
            _data.push_back({value});
        }
    }
}

PointData::PointData(std::map<int, std::vector<Point2D<float>>> data) {
    for (auto const & [key, value]: data) {
        _time.push_back(key);
        _data.push_back(value);
    }
}

void PointData::clearPointsAtTime(int const time) {
    _clearPointsAtTime(time);
    notifyObservers();
}

void PointData::_clearPointsAtTime(int const time) {
    auto it = std::find(_time.begin(), _time.end(), time);
    if (it != _time.end()) {
        size_t index = std::distance(_time.begin(), it);
        _data[index].clear();
    } else {
        // If time doesn't exist, add it with empty vector
        _time.push_back(time);
        _data.emplace_back();
    }
}

void PointData::overwritePointAtTime(int const time, float const x, float const y) {
    _overwritePointAtTime(time, x, y);
    notifyObservers();
}

void PointData::_overwritePointAtTime(int const time, float const x, float const y) {
    _clearPointsAtTime(time);
    _addPointAtTime(time, x, y);
    notifyObservers();
}

void PointData::overwritePointsAtTime(int const time, std::vector<Point2D<float>> const & points) {
    _overwritePointsAtTime(time, points);
    notifyObservers();
}

void PointData::_overwritePointsAtTime(int const time, std::vector<Point2D<float>> const & points) {
    _clearPointsAtTime(time);
    _addPointsAtTime(time, points);
    notifyObservers();
}

void PointData::overwritePointsAtTimes(
        std::vector<size_t> const & times,
        std::vector<std::vector<Point2D<float>>> const & points) {
    if (times.size() != points.size()) {
        std::cout << "overwritePointsAtTimes: times and points must be the same size" << std::endl;
        return;
    }

    for (std::size_t i = 0; i < times.size(); i++) {
        _overwritePointsAtTime(times[i], points[i]);
    }
    notifyObservers();
}

void PointData::addPointAtTime(int const time, float const x, float const y) {
    _addPointAtTime(time, x, y);
    notifyObservers();
}

void PointData::_addPointAtTime(int const time, float const x, float const y) {
    auto it = std::find(_time.begin(), _time.end(), time);
    if (it != _time.end()) {
        size_t index = std::distance(_time.begin(), it);
        _data[index].push_back(Point2D<float>{x, y});
    } else {
        _time.push_back(time);
        _data.push_back({Point2D<float>{x, y}});
    }
}

void PointData::addPointsAtTime(int const time, std::vector<Point2D<float>> const & points) {
    _addPointsAtTime(time, points);
    notifyObservers();
}

void PointData::_addPointsAtTime(int const time, std::vector<Point2D<float>> const & points) {
    auto it = std::find(_time.begin(), _time.end(), time);
    if (it != _time.end()) {
        size_t index = std::distance(_time.begin(), it);
        _data[index].insert(_data[index].end(), points.begin(), points.end());
    } else {
        _time.push_back(time);
        _data.push_back(points);
    }
}

std::vector<int> PointData::getTimesWithPoints() const {
    std::vector<int> times;
    times.reserve(_time.size());
    for (auto t: _time) {
        times.push_back(static_cast<int>(t));
    }
    return times;
}

std::vector<Point2D<float>> const & PointData::getPointsAtTime(int const time) const {
    auto it = std::find(_time.begin(), _time.end(), time);
    if (it != _time.end()) {
        size_t index = std::distance(_time.begin(), it);
        return _data[index];
    } else {
        return _empty;
    }
}

std::size_t PointData::getMaxPoints() const {
    std::size_t max_points = 1;
    for (auto const & points: _data) {
        if (points.size() > max_points) {
            max_points = points.size();
        }
    }
    return max_points;
}
