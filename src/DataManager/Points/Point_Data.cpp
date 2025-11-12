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
