#include "Point_Data.hpp"

#include "utils/map_timeseries.hpp"

#include <algorithm>
#include <iostream>
#include <ranges>

// ========== Constructors ==========

PointData::PointData(std::map<TimeFrameIndex, Point2D<float>> const & data) {
    for (auto const & [time, point]: data) {
        _data[time].push_back(point);
    }
}

PointData::PointData(std::map<TimeFrameIndex, std::vector<Point2D<float>>> const & data) 
    : _data(data) {
}

// ========== Setters ==========

bool PointData::clearAtTime(TimeFrameIndex const time, bool notify) {
    if (clear_at_time(time, _data)) {
        if (notify) {
            notifyObservers();
        }
        return true;
    }
    return false;
}

bool PointData::clearAtTime(TimeFrameIndex const time, size_t const index, bool notify) {
    if (clear_at_time(time, index, _data)) {
        if (notify) {
            notifyObservers();
        }
        return true;
    }
    return false;
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

// ========== Getters ==========

std::vector<Point2D<float>> const & PointData::getAtTime(TimeFrameIndex const time) const {
    auto it = _data.find(time);
    if (it != _data.end()) {
        return it->second;
    } else {
        return _empty;
    }
}

std::vector<Point2D<float>> const & PointData::getAtTime(TimeFrameIndex const time, 
                                                        TimeFrame const * source_timeframe,
                                                        TimeFrame const * target_timeframe) const {
    // If the timeframes are the same object, no conversion is needed
    if (source_timeframe == target_timeframe) {
        return getAtTime(time);
    }
    
    // If either timeframe is null, fall back to original behavior
    if (!source_timeframe || !target_timeframe) {
        return getAtTime(time);
    }
    
    // Convert the time index from source timeframe to target timeframe
    // 1. Get the time value from the source timeframe
    auto time_value = source_timeframe->getTimeAtIndex(time);
    
    // 2. Convert that time value to an index in the target timeframe  
    auto target_index = target_timeframe->getIndexAtTime(static_cast<float>(time_value));
    
    return getAtTime(target_index);
}

std::size_t PointData::getMaxPoints() const {
    std::size_t max_points = 0;
    for (auto const & [time, points] : _data) {
        max_points = std::max(max_points, points.size());
    }
    return max_points;
}

// ========== Image Size ==========

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

// ========== Copy and Move ==========

std::size_t PointData::copyTo(PointData& target, TimeFrameInterval const & interval, bool notify) const {
    if (interval.start > interval.end) {
        std::cerr << "PointData::copyTo: interval start (" << interval.start.getValue() 
                  << ") must be <= interval end (" << interval.end.getValue() << ")" << std::endl;
        return 0;
    }

    std::size_t total_points_copied = 0;

    // Iterate through all times in the source data within the interval
    for (auto const & [time, points] : _data) {
        if (time >= interval.start && time <= interval.end && !points.empty()) {
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

std::size_t PointData::moveTo(PointData& target, TimeFrameInterval const & interval, bool notify) {
    if (interval.start > interval.end) {
        std::cerr << "PointData::moveTo: interval start (" << interval.start.getValue() 
                  << ") must be <= interval end (" << interval.end.getValue() << ")" << std::endl;
        return 0;
    }

    std::size_t total_points_moved = 0;
    std::vector<TimeFrameIndex> times_to_clear;

    // First, copy all points in the interval to target
    for (auto const & [time, points] : _data) {
        if (time >= interval.start && time <= interval.end && !points.empty()) {
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
