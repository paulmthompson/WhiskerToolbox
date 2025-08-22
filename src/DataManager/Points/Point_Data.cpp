#include "Point_Data.hpp"

#include "utils/map_timeseries.hpp"
#include "Entity/EntityRegistry.hpp"

#include <algorithm>
#include <iostream>
#include <ranges>

// ========== Constructors ==========

PointData::PointData(std::map<TimeFrameIndex, Point2D<float>> const & data) {
    for (auto const & [time, point]: data) {
        _data[time].push_back(point);
        _entity_ids_by_time[time].push_back(0); // placeholder until identity context is set
    }
}

PointData::PointData(std::map<TimeFrameIndex, std::vector<Point2D<float>>> const & data) 
    : _data(data) {
    for (auto const & [time, points] : _data) {
        _entity_ids_by_time[time].resize(points.size(), 0);
    }
}

// ========== Setters ==========

bool PointData::clearAtTime(TimeFrameIndex const time, bool notify) {
    if (clear_at_time(time, _data)) {
        _entity_ids_by_time.erase(time);
        if (notify) {
            notifyObservers();
        }
        return true;
    }
    return false;
}

bool PointData::clearAtTime(TimeFrameIndex const time, size_t const index, bool notify) {
    if (clear_at_time(time, index, _data)) {
        auto it = _entity_ids_by_time.find(time);
        if (it != _entity_ids_by_time.end()) {
            if (index < it->second.size()) {
                it->second.erase(it->second.begin() + static_cast<std::ptrdiff_t>(index));
            }
            if (it->second.empty()) {
                _entity_ids_by_time.erase(it);
            }
        }
        if (notify) {
            notifyObservers();
        }
        return true;
    }
    return false;
}

void PointData::overwritePointAtTime(TimeFrameIndex const time, Point2D<float> const point, bool notify) {
    _data[time] = {point};
    if (_identity_registry) {
        _entity_ids_by_time[time] = {
            _identity_registry->ensureId(_identity_data_key, EntityKind::Point, time, 0)
        };
    } else {
        _entity_ids_by_time[time] = {0};
    }
    if (notify) {
        notifyObservers();
    }
}

void PointData::overwritePointsAtTime(TimeFrameIndex const time, std::vector<Point2D<float>> const & points, bool notify) {
    _data[time] = points;
    _entity_ids_by_time[time].clear();
    _entity_ids_by_time[time].reserve(points.size());
    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
        if (_identity_registry) {
            _entity_ids_by_time[time].push_back(
                _identity_registry->ensureId(_identity_data_key, EntityKind::Point, time, i)
            );
        } else {
            _entity_ids_by_time[time].push_back(0);
        }
    }
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
        _entity_ids_by_time[times[i]].clear();
        _entity_ids_by_time[times[i]].reserve(points[i].size());
        for (int j = 0; j < static_cast<int>(points[i].size()); ++j) {
            if (_identity_registry) {
                _entity_ids_by_time[times[i]].push_back(
                    _identity_registry->ensureId(_identity_data_key, EntityKind::Point, times[i], j)
                );
            } else {
                _entity_ids_by_time[times[i]].push_back(0);
            }
        }
    }
    if (notify) {
        notifyObservers();
    }
}

void PointData::addAtTime(TimeFrameIndex const time, Point2D<float> const point, bool notify) {
    add_at_time(time, point, _data);
    int local_index = static_cast<int>(_data[time].size()) - 1;
    if (_identity_registry) {
        EntityId id = _identity_registry->ensureId(_identity_data_key, EntityKind::Point, time, local_index);
        _entity_ids_by_time[time].push_back(id);
    } else {
        _entity_ids_by_time[time].push_back(0);
    }

    if (notify) {
        notifyObservers();
    }
}

void PointData::addPointsAtTime(TimeFrameIndex const time, std::vector<Point2D<float>> const & points, bool notify) {
    _data[time].insert(_data[time].end(), points.begin(), points.end());
    int start_index = static_cast<int>(_entity_ids_by_time[time].size());
    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
        if (_identity_registry) {
            _entity_ids_by_time[time].push_back(
                _identity_registry->ensureId(_identity_data_key, EntityKind::Point, time, start_index + i)
            );
        } else {
            _entity_ids_by_time[time].push_back(0);
        }
    }
    
    if (notify) {
        notifyObservers();
    }
}

// ========== Getters ==========

std::vector<Point2D<float>> const & PointData::getAtTime(TimeFrameIndex const time) const {
    return get_at_time(time, _data, _empty);
}

std::vector<Point2D<float>> const & PointData::getAtTime(TimeFrameIndex const time, 
                                                        TimeFrame const * source_timeframe,
                                                        TimeFrame const * target_timeframe) const {
    return get_at_time(time, _data, _empty, source_timeframe, target_timeframe);
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
        (void) clearAtTime(time, false); // Don't notify for each operation
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
        (void) clearAtTime(time, false); // Don't notify for each operation
    }

    // Notify observers only once at the end if requested
    if (notify && total_points_moved > 0) {
        target.notifyObservers();
        notifyObservers();
    }

    return total_points_moved;
}

void PointData::setIdentityContext(std::string const & data_key, EntityRegistry * registry) {
    _identity_data_key = data_key;
    _identity_registry = registry;
}

void PointData::rebuildAllEntityIds() {
    if (!_identity_registry) {
        // Clear to placeholder zeros to maintain alignment
        for (auto & [t, pts] : _data) {
            _entity_ids_by_time[t].assign(pts.size(), 0);
        }
        return;
    }
    for (auto & [t, pts] : _data) {
        auto & ids = _entity_ids_by_time[t];
        ids.clear();
        ids.reserve(pts.size());
        for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
            ids.push_back(_identity_registry->ensureId(_identity_data_key, EntityKind::Point, t, i));
        }
    }
}

std::vector<EntityId> const & PointData::getEntityIdsAtTime(TimeFrameIndex time) const {
    auto it = _entity_ids_by_time.find(time);
    if (it == _entity_ids_by_time.end()) {
        static const std::vector<EntityId> kEmpty;
        return kEmpty;
    }
    return it->second;
}

std::vector<EntityId> PointData::getAllEntityIds() const {
    std::vector<EntityId> out;
    for (auto const & [t, ids] : _entity_ids_by_time) {
        out.insert(out.end(), ids.begin(), ids.end());
    }
    return out;
}
