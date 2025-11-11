#include "Point_Data.hpp"

#include "Entity/EntityRegistry.hpp"
#include "utils/map_timeseries.hpp"

#include <algorithm>
#include <iostream>
#include <ranges>

// ========== Constructors ==========

PointData::PointData(std::map<TimeFrameIndex, Point2D<float>> const & data) {
    for (auto const & [time, point]: data) {
        _data[time].emplace_back(EntityId(0), point);
    }
}

PointData::PointData(std::map<TimeFrameIndex, std::vector<Point2D<float>>> const & data) {
    for (auto const & [time, points]: data) {
        auto & entries = _data[time];
        entries.reserve(points.size());
        for (auto const & p: points) {
            entries.emplace_back(EntityId(0), p);
        }
    }
}

// ========== Setters ==========


[[nodiscard]] std::optional<PointData::PointModifier> PointData::getMutableData(EntityId entity_id, bool notify) {

    auto descriptor = _identity_registry->get(entity_id);
    if (!descriptor || descriptor->kind != EntityKind::PointEntity || descriptor->data_key != _identity_data_key) {
        return std::nullopt;
    }

    auto time_it = _data.find(TimeFrameIndex{descriptor->time_value});
    if (time_it == _data.end()) {
        return std::nullopt;
    }

    size_t local_index = static_cast<size_t>(descriptor->local_index);
    if (local_index >= time_it->second.size()) {
        return std::nullopt;
    }

    Point2D<float> & point = time_it->second[local_index].data;

    return PointModifier(point, [this, notify]() { if (notify) { this->notifyObservers(); } });
}


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


void PointData::overwritePointAtTime(TimeFrameIndex const time, Point2D<float> const point, bool notify) {
    auto entity_id = EntityId(0);
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::PointEntity, time, 0);
    }
    _data[time].clear();
    _data[time].emplace_back(entity_id, point);
    if (notify) {
        notifyObservers();
    }
}

void PointData::addAtTime(TimeFrameIndex const time, std::vector<Point2D<float>> const & points_to_add) {
    if (points_to_add.empty()) {
        return;
    }

    // 1. Get (or create) the vector of entries for this time
    // This is our single map lookup.
    auto & entry_vec = _data[time];

    // 2. Reserve space once for high performance
    size_t const old_size = entry_vec.size();
    entry_vec.reserve(old_size + points_to_add.size());

    // 3. Loop and emplace new entries
    for (size_t i = 0; i < points_to_add.size(); ++i) {
        int const local_index = static_cast<int>(old_size + i);
        auto entity_id = EntityId(0);
        if (_identity_registry) {
            entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::PointEntity, time, local_index);
        }

        // Calls DataEntry(entity_id, points_to_add[i])
        // This will invoke the Point2D<float> copy constructor
        entry_vec.emplace_back(entity_id, points_to_add[i]);
    }

    // Note: Following our "manual notify" pattern,
    // the caller is responsible for calling notifyObservers().
}

void PointData::addAtTime(TimeFrameIndex const time, std::vector<Point2D<float>> && points_to_add) {
    if (points_to_add.empty()) {
        return;
    }

    // 1. Get (or create) the vector of entries for this time
    auto & entry_vec = _data[time];

    // 2. Reserve space once
    size_t const old_size = entry_vec.size();
    entry_vec.reserve(old_size + points_to_add.size());

    // 3. Loop and emplace new entries
    for (size_t i = 0; i < points_to_add.size(); ++i) {
        int const local_index = static_cast<int>(old_size + i);
        auto entity_id = EntityId(0);
        if (_identity_registry) {
            entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::PointEntity, time, local_index);
        }

        // Calls DataEntry(entity_id, std::move(lines_to_add[i]))
        // This will invoke the Line2D MOVE constructor
        entry_vec.emplace_back(entity_id, std::move(points_to_add[i]));
    }
}

void PointData::addAtTime(TimeFrameIndex const time, Point2D<float> const point, bool notify) {
    int const local_index = static_cast<int>(_data[time].size());
    auto entity_id = EntityId(0);
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::PointEntity, time, local_index);
    }
    _data[time].emplace_back(entity_id, point);

    if (notify) {
        notifyObservers();
    }
}

void PointData::addEntryAtTime(TimeFrameIndex const time,
                               Point2D<float> const & point,
                               EntityId const entity_id,
                               bool const notify) {
    _data[time].emplace_back(entity_id, point);
    if (notify) {
        notifyObservers();
    }
}


bool PointData::clearByEntityId(EntityId entity_id, bool notify) {
    if (!_identity_registry) {
        return false;
    }

    auto descriptor = _identity_registry->get(entity_id);
    if (!descriptor || descriptor->kind != EntityKind::PointEntity || descriptor->data_key != _identity_data_key) {
        return false;
    }

    TimeFrameIndex const time{descriptor->time_value};
    int const local_index = descriptor->local_index;

    auto time_it = _data.find(time);
    if (time_it == _data.end()) {
        return false;
    }

    if (local_index < 0 || static_cast<size_t>(local_index) >= time_it->second.size()) {
        return false;
    }

    time_it->second.erase(time_it->second.begin() + static_cast<std::ptrdiff_t>(local_index));
    if (time_it->second.empty()) {
        _data.erase(time_it);
    }
    if (notify) {
        notifyObservers();
    }
    return true;
}

// ========== Getters ==========


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
            entry.data.x *= scale_x;
            entry.data.y *= scale_y;
        }
    }
    _image_size = image_size;
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
                entry.entity_id = EntityId(0);
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

// ========== Entity Lookup Methods ==========

std::optional<std::reference_wrapper<Point2D<float> const>> PointData::getDataByEntityId(EntityId entity_id) const {
    if (!_identity_registry) {
        return std::nullopt;
    }

    auto descriptor = _identity_registry->get(entity_id);
    if (!descriptor || descriptor->kind != EntityKind::LineEntity || descriptor->data_key != _identity_data_key) {
        return std::nullopt;
    }

    TimeFrameIndex const time{descriptor->time_value};
    int const local_index = descriptor->local_index;

    auto time_it = _data.find(time);
    if (time_it == _data.end()) {
        return std::nullopt;
    }

    if (local_index < 0 || static_cast<size_t>(local_index) >= time_it->second.size()) {
        return std::nullopt;
    }

    return std::cref(time_it->second[static_cast<size_t>(local_index)].data);
}

std::optional<TimeFrameIndex> PointData::getTimeByEntityId(EntityId entity_id) const {
    if (!_identity_registry) {
        return std::nullopt;
    }

    auto descriptor = _identity_registry->get(entity_id);
    return TimeFrameIndex{descriptor->time_value};
}

std::vector<std::pair<EntityId, std::reference_wrapper<Point2D<float> const>>> PointData::getDataByEntityIds(std::vector<EntityId> const & entity_ids) const {
    std::vector<std::pair<EntityId, std::reference_wrapper<Point2D<float> const>>> results;
    results.reserve(entity_ids.size());

    for (EntityId const entity_id: entity_ids) {
        auto point = getDataByEntityId(entity_id);
        if (point.has_value()) {
            results.emplace_back(entity_id, point.value());
        }
    }

    return results;
}

// ======== Copy/Move by EntityIds =========

std::size_t PointData::copyByEntityIds(PointData & target, std::unordered_set<EntityId> const & entity_ids, bool const notify) {
    return copy_by_entity_ids(_data, target, entity_ids, notify,
                              [](PointEntry const & entry) -> Point2D<float> const & { return entry.data; });
}

std::size_t PointData::moveByEntityIds(PointData & target, std::unordered_set<EntityId> const & entity_ids, bool notify) {
    auto result = move_by_entity_ids(_data, target, entity_ids, notify,
                                     [](PointEntry const & entry) -> Point2D<float> const & { return entry.data; });

    if (notify && result > 0) {
        notifyObservers();
    }

    return result;
}
