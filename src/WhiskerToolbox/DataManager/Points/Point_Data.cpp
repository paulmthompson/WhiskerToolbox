#include "Point_Data.hpp"

#include <algorithm>
#include <iostream>

PointData::PointData(std::map<TimeFrameIndex, Point2D<float>> const & data) {
    for (auto const & [time, point]: data) {
        _data[time].push_back(point);
    }
}

PointData::PointData(std::map<TimeFrameIndex, std::vector<Point2D<float>>> const & data) 
    : _data(data) {
}

void PointData::clearAtTime(TimeFrameIndex const time, bool notify) {
    auto it = _data.find(time);
    if (it != _data.end()) {
        _data.erase(it);
    }

    if (notify) {
        notifyObservers();
    }
}

void PointData::overwritePointAtTime(TimeFrameIndex const time, Point2D<float> const point, bool notify) {
    _data[time] = {point};
    if (notify) {
        notifyObservers();
    }
}

void PointData::overwritePointsAtTime(TimeFrameIndex const time, std::vector<Point2D<float>> const & points, bool notify) {
    _data[time] = points;
    if (notify) {
        notifyObservers();
    }
}

void PointData::overwritePointsAtTimes(
        std::vector<TimeFrameIndex> const & times,
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

void PointData::addPointAtTime(TimeFrameIndex const time, Point2D<float> const point, bool notify) {
    _data[time].push_back(point);

    if (notify) {
        notifyObservers();
    }
}

void PointData::addPointsAtTime(TimeFrameIndex const time, std::vector<Point2D<float>> const & points, bool notify) {
    _data[time].insert(_data[time].end(), points.begin(), points.end());
    
    if (notify) {
        notifyObservers();
    }
}

std::vector<TimeFrameIndex> PointData::getTimesWithData() const {
    std::vector<TimeFrameIndex> times;
    times.reserve(_data.size());
    for (auto const & [time, points] : _data) {
        times.push_back(time);
    }
    return times;
}

std::vector<Point2D<float>> const & PointData::getPointsAtTime(TimeFrameIndex const time) const {
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
        std::cout << "No size set for current image. "
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

std::size_t PointData::copyTo(PointData& target, TimeFrameIndex start_time, TimeFrameIndex end_time, bool notify) const {
    if (start_time > end_time) {
        std::cerr << "PointData::copyTo: start_time (" << start_time.getValue() 
                  << ") must be <= end_time (" << end_time.getValue() << ")" << std::endl;
        return 0;
    }

    std::size_t total_points_copied = 0;

    // Iterate through all times in the source data within the range
    for (auto const & [time, points] : _data) {
        if (time >= start_time && time <= end_time && !points.empty()) {
            target.addPointsAtTime(time, points, false); // Don't notify for each operation
            total_points_copied += points.size();
        }
    }

    // Notify observer only once at the end if requested
    if (notify && total_points_copied > 0) {
        target.notifyObservers();
    }

    return total_points_copied;
}

std::size_t PointData::copyTo(PointData& target, std::vector<TimeFrameIndex> const& times, bool notify) const {
    std::size_t total_points_copied = 0;

    // Copy points for each specified time
    for (TimeFrameIndex time : times) {
        auto it = _data.find(time);
        if (it != _data.end() && !it->second.empty()) {
            target.addPointsAtTime(time, it->second, false); // Don't notify for each operation
            total_points_copied += it->second.size();
        }
    }

    // Notify observer only once at the end if requested
    if (notify && total_points_copied > 0) {
        target.notifyObservers();
    }

    return total_points_copied;
}

std::size_t PointData::moveTo(PointData& target, TimeFrameIndex start_time, TimeFrameIndex end_time, bool notify) {
    if (start_time > end_time) {
        std::cerr << "PointData::moveTo: start_time (" << start_time.getValue() 
                  << ") must be <= end_time (" << end_time.getValue() << ")" << std::endl;
        return 0;
    }

    std::size_t total_points_moved = 0;
    std::vector<TimeFrameIndex> times_to_clear;

    // First, copy all points in the range to target
    for (auto const & [time, points] : _data) {
        if (time >= start_time && time <= end_time && !points.empty()) {
            target.addPointsAtTime(time, points, false); // Don't notify for each operation
            total_points_moved += points.size();
            times_to_clear.push_back(time);
        }
    }

    // Then, clear all the times from source
    for (TimeFrameIndex time : times_to_clear) {
        clearAtTime(time, false); // Don't notify for each operation
    }

    // Notify observers only once at the end if requested
    if (notify && total_points_moved > 0) {
        target.notifyObservers();
        notifyObservers();
    }

    return total_points_moved;
}

std::size_t PointData::moveTo(PointData& target, std::vector<TimeFrameIndex> const& times, bool notify) {
    std::size_t total_points_moved = 0;
    std::vector<TimeFrameIndex> times_to_clear;

    // First, copy points for each specified time to target
    for (TimeFrameIndex time : times) {
        auto it = _data.find(time);
        if (it != _data.end() && !it->second.empty()) {
            target.addPointsAtTime(time, it->second, false); // Don't notify for each operation
            total_points_moved += it->second.size();
            times_to_clear.push_back(time);
        }
    }

    // Then, clear all the times from source
    for (TimeFrameIndex time : times_to_clear) {
        clearAtTime(time, false); // Don't notify for each operation
    }

    // Notify observers only once at the end if requested
    if (notify && total_points_moved > 0) {
        target.notifyObservers();
        notifyObservers();
    }

    return total_points_moved;
}
