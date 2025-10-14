#include "Point_Data.hpp"

#include "Entity/EntityRegistry.hpp"
#include "utils/map_timeseries.hpp"

#include <algorithm>
#include <iostream>
#include <ranges>

// ========== Constructors ==========

PointData::PointData(std::map<TimeFrameIndex, Point2D<float>> const & data) {
    for (auto const & [time, point]: data) {
        _data[time].emplace_back(point, 0);
    }
}

PointData::PointData(std::map<TimeFrameIndex, std::vector<Point2D<float>>> const & data) {
    for (auto const & [time, points]: data) {
        auto & entries = _data[time];
        entries.reserve(points.size());
        for (auto const & p: points) {
            entries.emplace_back(p, 0);
        }
    }
}

// ========== Setters ==========

bool PointData::clearAtTime(TimeFrameIndex const time, bool notify) {
    auto it = _data.find(time);
    if (it != _data.end()) {
        _data.erase(it);
        if (notify) {
            notifyObservers();
        }
        return true;
    }
    return false;
}

bool PointData::clearAtTime(TimeFrameIndex const time, size_t const index, bool notify) {
    auto it = _data.find(time);
    if (it != _data.end()) {
        if (index >= it->second.size()) {
            return false;
        }
        it->second.erase(it->second.begin() + static_cast<std::ptrdiff_t>(index));
        if (it->second.empty()) {
            _data.erase(it);
        }
        if (notify) {
            notifyObservers();
        }
        return true;
    }
    return false;
}

void PointData::overwritePointAtTime(TimeFrameIndex const time, Point2D<float> const point, bool notify) {
    EntityId entity_id = 0;
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::PointEntity, time, 0);
    }
    _data[time].clear();
    _data[time].emplace_back(point, entity_id);
    if (notify) {
        notifyObservers();
    }
}

void PointData::overwritePointsAtTime(TimeFrameIndex const time, std::vector<Point2D<float>> const & points, bool notify) {
    auto & entries = _data[time];
    entries.clear();
    entries.reserve(points.size());
    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
        EntityId id = 0;
        if (_identity_registry) {
            id = _identity_registry->ensureId(_identity_data_key, EntityKind::PointEntity, time, i);
        }
        entries.emplace_back(points[static_cast<size_t>(i)], id);
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
        auto const & pts = points[i];
        auto const time = times[i];
        auto & entries = _data[time];
        entries.clear();
        entries.reserve(pts.size());
        for (int j = 0; j < static_cast<int>(pts.size()); ++j) {
            EntityId id = 0;
            if (_identity_registry) {
                id = _identity_registry->ensureId(_identity_data_key, EntityKind::PointEntity, time, j);
            }
            entries.emplace_back(pts[static_cast<size_t>(j)], id);
        }
    }
    if (notify) {
        notifyObservers();
    }
}

void PointData::addAtTime(TimeFrameIndex const time, Point2D<float> const point, bool notify) {
    int const local_index = static_cast<int>(_data[time].size());
    EntityId entity_id = 0;
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::PointEntity, time, local_index);
    }
    _data[time].emplace_back(point, entity_id);

    if (notify) {
        notifyObservers();
    }
}

void PointData::addPointsAtTime(TimeFrameIndex const time, std::vector<Point2D<float>> const & points, bool notify) {
    auto & entries = _data[time];
    int const start_index = static_cast<int>(entries.size());
    entries.reserve(entries.size() + points.size());
    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
        EntityId id = 0;
        if (_identity_registry) {
            id = _identity_registry->ensureId(_identity_data_key, EntityKind::PointEntity, time, start_index + i);
        }
        entries.emplace_back(points[static_cast<size_t>(i)], id);
    }

    if (notify) {
        notifyObservers();
    }
}

void PointData::addEntryAtTime(TimeFrameIndex const time,
                               Point2D<float> const & point,
                               EntityId const entity_id,
                               bool const notify) {
    _data[time].emplace_back(point, entity_id);
    if (notify) {
        notifyObservers();
    }
}

// ========== Getters ==========

std::vector<Point2D<float>> const & PointData::getAtTime(TimeFrameIndex const time) const {
    auto it = _data.find(time);
    if (it == _data.end()) {
        return _empty;
    }
    _temp_points.clear();
    _temp_points.reserve(it->second.size());
    for (auto const & entry: it->second) {
        _temp_points.push_back(entry.point);
    }
    return _temp_points;
}

std::vector<Point2D<float>> const & PointData::getAtTime(TimeFrameIndex const time,
                                                         TimeFrame const * source_timeframe,
                                                         TimeFrame const * target_timeframe) const {
    TimeFrameIndex const converted = convert_time_index(time, source_timeframe, target_timeframe);
    return getAtTime(converted);
}

std::size_t PointData::getMaxPoints() const {
    std::size_t max_points = 0;
    for (auto const & [time, entries]: _data) {
        (void) time;
        max_points = std::max(max_points, entries.size());
    }
    return max_points;
}

// ========== Image Size ==========

void PointData::changeImageSize(ImageSize const & image_size) {
    if (_image_size.width == -1 || _image_size.height == -1) {
        std::cout << "No size set for current image. "
                  << " Please set a valid image size before trying to scale" << std::endl;
        _image_size = image_size;// Set the image size if it wasn't set before
        return;
    }

    if (_image_size.width == image_size.width && _image_size.height == image_size.height) {
        std::cout << "Image size is the same. No need to scale" << std::endl;
        return;
    }

    float const scale_x = static_cast<float>(image_size.width) / static_cast<float>(_image_size.width);
    float const scale_y = static_cast<float>(image_size.height) / static_cast<float>(_image_size.height);

    for (auto & [time, entries]: _data) {
        (void) time;
        for (auto & entry: entries) {
            entry.point.x *= scale_x;
            entry.point.y *= scale_y;
        }
    }
    _image_size = image_size;
}

// ========== Copy and Move ==========

std::size_t PointData::copyTo(PointData & target, TimeFrameInterval const & interval, bool notify) const {
    if (interval.start > interval.end) {
        std::cerr << "PointData::copyTo: interval start (" << interval.start.getValue()
                  << ") must be <= interval end (" << interval.end.getValue() << ")" << std::endl;
        return 0;
    }

    std::size_t total_points_copied = 0;

    // Iterate through all times in the source data within the interval
    for (auto const & [time, entries]: _data) {
        if (time >= interval.start && time <= interval.end && !entries.empty()) {
            for (auto const & entry: entries) {
                target.addAtTime(time, entry.point, false);
                total_points_copied += 1;
            }
        }
    }

    // Notify observer only once at the end if requested
    if (notify && total_points_copied > 0) {
        target.notifyObservers();
    }

    return total_points_copied;
}

std::size_t PointData::copyTo(PointData & target, std::vector<TimeFrameIndex> const & times, bool notify) const {
    std::size_t total_points_copied = 0;

    // Copy points for each specified time
    for (TimeFrameIndex time: times) {
        auto it = _data.find(time);
        if (it != _data.end() && !it->second.empty()) {
            for (auto const & entry: it->second) {
                target.addAtTime(time, entry.point, false);
                total_points_copied += 1;
            }
        }
    }

    // Notify observer only once at the end if requested
    if (notify && total_points_copied > 0) {
        target.notifyObservers();
    }

    return total_points_copied;
}

std::size_t PointData::moveTo(PointData & target, TimeFrameInterval const & interval, bool notify) {
    if (interval.start > interval.end) {
        std::cerr << "PointData::moveTo: interval start (" << interval.start.getValue()
                  << ") must be <= interval end (" << interval.end.getValue() << ")" << std::endl;
        return 0;
    }

    std::size_t total_points_moved = 0;
    std::vector<TimeFrameIndex> times_to_clear;

    // First, copy all points in the interval to target
    for (auto const & [time, entries]: _data) {
        if (time >= interval.start && time <= interval.end && !entries.empty()) {
            for (auto const & entry: entries) {
                target.addAtTime(time, entry.point, false);
                total_points_moved += 1;
            }
            times_to_clear.push_back(time);
        }
    }

    // Then, clear all the times from source
    for (TimeFrameIndex time: times_to_clear) {
        (void) clearAtTime(time, false);// Don't notify for each operation
    }

    // Notify observers only once at the end if requested
    if (notify && total_points_moved > 0) {
        target.notifyObservers();
        notifyObservers();
    }

    return total_points_moved;
}

std::size_t PointData::moveTo(PointData & target, std::vector<TimeFrameIndex> const & times, bool notify) {
    std::size_t total_points_moved = 0;
    std::vector<TimeFrameIndex> times_to_clear;

    // First, copy points for each specified time to target
    for (TimeFrameIndex time: times) {
        auto it = _data.find(time);
        if (it != _data.end() && !it->second.empty()) {
            for (auto const & entry: it->second) {
                target.addAtTime(time, entry.point, false);
                total_points_moved += 1;
            }
            times_to_clear.push_back(time);
        }
    }

    // Then, clear all the times from source
    for (TimeFrameIndex time: times_to_clear) {
        (void) clearAtTime(time, false);// Don't notify for each operation
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
        for (auto & [t, entries]: _data) {
            (void) t;
            for (auto & entry: entries) {
                entry.entity_id = 0;
            }
        }
        return;
    }
    for (auto & [t, entries]: _data) {
        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
            entries[static_cast<size_t>(i)].entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::PointEntity, t, i);
        }
    }
}

std::vector<EntityId> const & PointData::getEntityIdsAtTime(TimeFrameIndex time) const {
    auto it = _data.find(time);
    if (it == _data.end()) {
        return _empty_entity_ids;
    }
    _temp_entity_ids.clear();
    _temp_entity_ids.reserve(it->second.size());
    for (auto const & entry: it->second) {
        _temp_entity_ids.push_back(entry.entity_id);
    }
    return _temp_entity_ids;
}

std::vector<EntityId> PointData::getAllEntityIds() const {
    std::vector<EntityId> out;
    for (auto const & [t, entries]: _data) {
        (void) t;
        for (auto const & entry: entries) {
            out.push_back(entry.entity_id);
        }
    }
    return out;
}

// ========== Entity Lookup Methods ==========

std::optional<Point2D<float>> PointData::getPointByEntityId(EntityId entity_id) const {
    if (!_identity_registry) {
        return std::nullopt;
    }

    auto descriptor = _identity_registry->get(entity_id);
    if (!descriptor || descriptor->kind != EntityKind::PointEntity || descriptor->data_key != _identity_data_key) {
        return std::nullopt;
    }

    TimeFrameIndex time{descriptor->time_value};
    int local_index = descriptor->local_index;

    auto time_it = _data.find(time);
    if (time_it == _data.end()) {
        return std::nullopt;
    }

    if (local_index < 0 || static_cast<size_t>(local_index) >= time_it->second.size()) {
        return std::nullopt;
    }

    return time_it->second[static_cast<size_t>(local_index)].point;
}

std::optional<std::pair<TimeFrameIndex, int>> PointData::getTimeAndIndexByEntityId(EntityId entity_id) const {
    if (!_identity_registry) {
        return std::nullopt;
    }

    auto descriptor = _identity_registry->get(entity_id);
    if (!descriptor || descriptor->kind != EntityKind::PointEntity || descriptor->data_key != _identity_data_key) {
        return std::nullopt;
    }

    TimeFrameIndex time{descriptor->time_value};
    int local_index = descriptor->local_index;

    // Verify the time and index are valid
    auto time_it = _data.find(time);
    if (time_it == _data.end() || local_index < 0 || static_cast<size_t>(local_index) >= time_it->second.size()) {
        return std::nullopt;
    }

    return std::make_pair(time, local_index);
}

std::vector<std::pair<EntityId, Point2D<float>>> PointData::getPointsByEntityIds(std::vector<EntityId> const & entity_ids) const {
    std::vector<std::pair<EntityId, Point2D<float>>> result;
    result.reserve(entity_ids.size());

    for (EntityId entity_id: entity_ids) {
        auto point = getPointByEntityId(entity_id);
        if (point) {
            result.emplace_back(entity_id, *point);
        }
    }

    return result;
}

std::vector<std::tuple<EntityId, TimeFrameIndex, int>> PointData::getTimeInfoByEntityIds(std::vector<EntityId> const & entity_ids) const {
    std::vector<std::tuple<EntityId, TimeFrameIndex, int>> result;
    result.reserve(entity_ids.size());

    for (EntityId entity_id: entity_ids) {
        auto time_info = getTimeAndIndexByEntityId(entity_id);
        if (time_info) {
            result.emplace_back(entity_id, time_info->first, time_info->second);
        }
    }

    return result;
}

// ======== Copy/Move by EntityIds =========

std::size_t PointData::copyByEntityIds(PointData & target, std::unordered_set<EntityId> const & entity_ids, bool const notify) {
    return copy_by_entity_ids(_data, target, entity_ids, notify,
                              [](PointEntry const & entry) -> Point2D<float> const & { return entry.point; });
}

std::size_t PointData::moveByEntityIds(PointData & target, std::unordered_set<EntityId> const & entity_ids, bool notify) {
    auto result = move_by_entity_ids(_data, target, entity_ids, notify, 
                                     [](PointEntry const & entry) -> Point2D<float> const & { return entry.point; });
    
    if (notify && result > 0) {
        notifyObservers();
    }
    
    return result;
}
