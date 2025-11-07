#include "Line_Data.hpp"

#include "CoreGeometry/points.hpp"
#include "Entity/EntityRegistry.hpp"
#include "utils/map_timeseries.hpp"

#include <cmath>
#include <iostream>
#include <ranges>

// ========== Constructors ==========

LineData::LineData(std::map<TimeFrameIndex, std::vector<Line2D>> const & data) {
    // Convert old format to new format
    for (auto const & [time, lines]: data) {
        _data[time].reserve(lines.size());
        for (auto const & line: lines) {
            _data[time].emplace_back(0, line); // EntityId will be 0 initially
        }
    }
}

LineData::LineData(LineData && other) noexcept
    : ObserverData(std::move(other)),
      _data(std::move(other._data)),
      _image_size(other._image_size),
      _time_frame(std::move(other._time_frame)),
      _identity_data_key(std::move(other._identity_data_key)),
      _identity_registry(other._identity_registry) {
    other._identity_registry = nullptr;
}

LineData & LineData::operator=(LineData && other) noexcept {
    if (this != &other) {
        ObserverData::operator=(std::move(other));
        _data = std::move(other._data);
        _image_size = other._image_size;
        _time_frame = std::move(other._time_frame);
        _identity_data_key = std::move(other._identity_data_key);
        _identity_registry = other._identity_registry;
        other._identity_registry = nullptr;
    }
    return *this;
}

// ========== Setters ==========

[[nodiscard]] std::optional<LineData::LineModifier> LineData::getMutableData(EntityId entity_id, bool notify) {

    auto descriptor = _identity_registry->get(entity_id);
    if (!descriptor || descriptor->kind != EntityKind::LineEntity || descriptor->data_key != _identity_data_key) {
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

    Line2D & line = time_it->second[local_index].data;

    return LineModifier(line, [this, notify]() { if (notify) { this->notifyObservers(); } });
}

bool LineData::clearAtTime(TimeFrameIndex const time, bool notify) {
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

bool LineData::clearByEntityId(EntityId entity_id, bool notify) {
    if (!_identity_registry) {
        return false;
    }

    auto descriptor = _identity_registry->get(entity_id);
    if (!descriptor || descriptor->kind != EntityKind::LineEntity || descriptor->data_key != _identity_data_key) {
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

void LineData::addAtTime(TimeFrameIndex const time, Line2D const & line, bool notify) {
    int const local_index = static_cast<int>(_data[time].size());
    EntityId entity_id = 0;
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::LineEntity, time, local_index);
    }

    _data[time].emplace_back(entity_id, line);

    if (notify) {
        notifyObservers();
    }
}

void LineData::addAtTime(TimeFrameIndex const time, Line2D && line, bool notify) {
    int const local_index = static_cast<int>(_data[time].size());
    EntityId entity_id = 0;
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::LineEntity, time, local_index);
    }

    _data[time].emplace_back(entity_id, std::move(line));

    if (notify) {
        notifyObservers();
    }
}

void LineData::addEntryAtTime(TimeFrameIndex const time, Line2D const & line, EntityId entity_id, bool notify) {
    _data[time].emplace_back(entity_id, line);
    if (notify) {
        notifyObservers();
    }
}

// ========== Getters ==========

std::vector<Line2D> const & LineData::getAtTime(TimeFrameIndex const time) const {
    // This method needs to return a reference to a vector of Line2D objects
    // Since we now store LineEntry objects, we need to create a temporary vector
    // We'll use a member variable to store the converted data to maintain reference stability

    auto it = _data.find(time);
    if (it == _data.end()) {
        return _empty;
    }

    // Use a mutable member variable to store the converted lines
    // This is not thread-safe but maintains API compatibility
    _temp_lines.clear();
    _temp_lines.reserve(it->second.size());
    for (auto const & entry: it->second) {
        _temp_lines.push_back(entry.data);
    }

    return _temp_lines;
}

std::vector<Line2D> const & LineData::getAtTime(TimeFrameIndex const time,
                                                TimeFrame const & source_timeframe) const {
    TimeFrameIndex const converted_time = convert_time_index(time,
                                                             &source_timeframe,
                                                             _time_frame.get());

    return getAtTime(converted_time);
}

std::vector<EntityId> const & LineData::getEntityIdsAtTime(TimeFrameIndex const time) const {
    // Similar to getAtTime, we need to create a temporary vector
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

std::vector<EntityId> const & LineData::getEntityIdsAtTime(TimeFrameIndex const time,
                                                           TimeFrame const & source_timeframe) const {

    TimeFrameIndex const converted_time = convert_time_index(time,
                                                             &source_timeframe,
                                                             _time_frame.get());

    return getEntityIdsAtTime(converted_time);
}

std::vector<EntityId> LineData::getAllEntityIds() const {
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

std::optional<Line2D> LineData::getDataByEntityId(EntityId entity_id) const {
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

    return time_it->second[static_cast<size_t>(local_index)].data;
}

std::optional<TimeFrameIndex> LineData::getTimeByEntityId(EntityId entity_id) const {
    if (!_identity_registry) {
        return std::nullopt;
    }

    auto descriptor = _identity_registry->get(entity_id);
    return TimeFrameIndex{descriptor->time_value};
}


std::vector<std::pair<EntityId, Line2D>> LineData::getDataByEntityIds(std::vector<EntityId> const & entity_ids) const {
    std::vector<std::pair<EntityId, Line2D>> results;
    results.reserve(entity_ids.size());

    for (EntityId const entity_id: entity_ids) {
        auto line = getDataByEntityId(entity_id);
        if (line.has_value()) {
            results.emplace_back(entity_id, std::move(line.value()));
        }
    }

    return results;
}

// ========== Image Size ==========

void LineData::changeImageSize(ImageSize const & image_size) {
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

    for (auto & [time, entries]: _data) {
        for (auto & entry: entries) {
            for (auto & point: entry.data) {
                point.x *= scale_x;
                point.y *= scale_y;
            }
        }
    }
    _image_size = image_size;
}

void LineData::setIdentityContext(std::string const & data_key, EntityRegistry * registry) {
    _identity_data_key = data_key;
    _identity_registry = registry;
}

void LineData::rebuildAllEntityIds() {
    if (!_identity_registry) {
        for (auto & [t, entries]: _data) {
            for (auto & entry: entries) {
                entry.entity_id = 0;
            }
        }
        return;
    }

    for (auto & [t, entries]: _data) {
        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
            entries[static_cast<size_t>(i)].entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::LineEntity, t, i);
        }
    }
}

// ========== Copy and Move ==========

std::size_t LineData::copyTo(LineData & target, TimeFrameInterval const & interval, bool notify) const {
    if (interval.start > interval.end) {
        std::cerr << "LineData::copyTo: interval start (" << interval.start.getValue()
                  << ") must be <= interval end (" << interval.end.getValue() << ")" << std::endl;
        return 0;
    }

    std::size_t total_lines_copied = 0;

    // Use lower_bound and upper_bound just like moveTo
    auto start_it = _data.lower_bound(interval.start);
    auto end_it = _data.upper_bound(interval.end); // upper_bound is correct for [start, end]

    for (auto it = start_it; it != end_it; ++it) {
        for (auto const & entry: it->second) {
            target.addAtTime(it->first, entry.data, false); // Don't notify yet
            total_lines_copied++;
        }
    }

    // Notify observer only once at the end if requested
    if (notify && total_lines_copied > 0) {
        target.notifyObservers();
    }

    return total_lines_copied;
}

std::size_t LineData::copyTo(LineData & target, std::vector<TimeFrameIndex> const & times, bool notify) const {
    std::size_t total_lines_copied = 0;

    // Copy lines for each specified time
    for (TimeFrameIndex const time: times) {
        auto it = _data.find(time);
        if (it != _data.end() && !it->second.empty()) {
            for (auto const & entry: it->second) {
                target.addAtTime(time, entry.data, false);// Don't notify for each operation
                total_lines_copied++;
            }
        }
    }

    // Notify observer only once at the end if requested
    if (notify && total_lines_copied > 0) {
        target.notifyObservers();
    }

    return total_lines_copied;
}

std::size_t LineData::moveTo(LineData & target, TimeFrameInterval const & interval, bool notify) {
    if (interval.start > interval.end) {
        std::cerr << "LineData::moveTo: interval start (" << interval.start.getValue()
                  << ") must be <= interval end (" << interval.end.getValue() << ")" << std::endl;
        return 0;
    }

    std::size_t total_lines_moved = 0;

    auto start_it = _data.lower_bound(interval.start);
    auto end_it = _data.upper_bound(interval.end);

    for (auto it = start_it; it != end_it;) {
        auto node = _data.extract(it++);
        total_lines_moved += node.mapped().size();
        auto [target_it, inserted, node_handle] = target._data.insert(std::move(node));
        if (!inserted) {
            // If the key already exists in the target, merge the vectors
            target_it->second.insert(target_it->second.end(),
                                     std::make_move_iterator(node_handle.mapped().begin()),
                                     std::make_move_iterator(node_handle.mapped().end()));
        }
    }

    if (notify && total_lines_moved > 0) {
        target.notifyObservers();
        notifyObservers();
    }

    return total_lines_moved;
}

std::size_t LineData::moveTo(LineData & target, std::vector<TimeFrameIndex> const & times, bool notify) {
    std::size_t total_lines_moved = 0;

    for (TimeFrameIndex const time: times) {
        if (auto it = _data.find(time); it != _data.end()) {
            auto node = _data.extract(it);
            total_lines_moved += node.mapped().size();
            auto [target_it, inserted, node_handle] = target._data.insert(std::move(node));
            if (!inserted) {
                // If the key already exists in the target, merge the vectors
                target_it->second.insert(target_it->second.end(),
                                         std::make_move_iterator(node_handle.mapped().begin()),
                                         std::make_move_iterator(node_handle.mapped().end()));
            }
        }
    }

    if (notify && total_lines_moved > 0) {
        target.notifyObservers();
        notifyObservers();
    }

    return total_lines_moved;
}

std::size_t LineData::copyByEntityIds(LineData & target, std::unordered_set<EntityId> const & entity_ids, bool notify) {
    return copy_by_entity_ids(_data, target, entity_ids, notify,
                              [](LineEntry const & entry) -> Line2D const & { return entry.data; });
}

std::size_t LineData::moveByEntityIds(LineData & target, std::unordered_set<EntityId> const & entity_ids, bool notify) {
    auto result = move_by_entity_ids(_data, target, entity_ids, notify,
                                     [](LineEntry const & entry) -> Line2D const & { return entry.data; });

    if (notify && result > 0) {
        notifyObservers();
    }

    return result;
}