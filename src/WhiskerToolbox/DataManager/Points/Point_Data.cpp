
#include "Point_Data.hpp"


#include <algorithm>
#include <iostream>

PointData::PointData(std::map<int, Point2D<float>> const & data) {
    for (auto [key, value]: data) {
        _data[key].push_back(value);
    }
}

PointData::PointData(std::map<int, std::vector<Point2D<float>>> data) {
    _data = std::move(data);
}

void PointData::clearPointsAtTime(int const time) {
    _clearPointsAtTime(time);
    notifyObservers();
}

void PointData::_clearPointsAtTime(int const time) {
    if (_data.find(time) == _data.end()) {
        return;
    }

    _data[time].clear();
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
    if (_data.find(time) == _data.end()) {
        _data[time] = std::vector<Point2D<float>>{Point2D<float>{x, y}};
    }
    _data[time].push_back(Point2D<float>{x, y});
}

void PointData::addPointsAtTime(int const time, std::vector<Point2D<float>> const & points) {
    _addPointsAtTime(time, points);
    notifyObservers();
}

void PointData::_addPointsAtTime(int const time, std::vector<Point2D<float>> const & points) {
    if (_data.find(time) == _data.end()) {
        _data[time] = points;
    } else {
        _data[time].insert(_data[time].end(), points.begin(), points.end());
    }
}

std::vector<int> PointData::getTimesWithPoints() const {
    std::vector<int> keys;
    keys.reserve(_data.size());
    for (auto const & kv: _data) {
        keys.push_back(kv.first);
    }
    return keys;
}

std::vector<Point2D<float>> const & PointData::getPointsAtTime(int const time) const {
    // [] operator is not const because it inserts if mask is not present
    if (_data.find(time) != _data.end()) {
        return _data.at(time);
    } else {
        return _empty;
    }
}

std::size_t PointData::getMaxPoints() const {
    std::size_t max_points = 1;
    for (auto const & [time, points]: _data) {
        if (points.size() > max_points) {
            max_points = points.size();
        }
    }
    return max_points;
}
