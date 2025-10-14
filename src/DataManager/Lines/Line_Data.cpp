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
            _data[time].emplace_back(line, 0);// EntityId will be 0 initially
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

bool LineData::clearAtTime(TimeFrameIndex const time, int const line_id, bool notify) {
    auto it = _data.find(time);
    if (it != _data.end() && static_cast<size_t>(line_id) < it->second.size()) {
        it->second.erase(it->second.begin() + static_cast<long int>(line_id));
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

void LineData::addAtTime(TimeFrameIndex const time, std::vector<float> const & x, std::vector<float> const & y, bool notify) {
    Line2D new_line;
    for (size_t i = 0; i < std::min(x.size(), y.size()); ++i) {
        new_line.push_back(Point2D<float>{x[i], y[i]});
    }

    int const local_index = static_cast<int>(_data[time].size());
    EntityId entity_id = 0;
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::LineEntity, time, local_index);
    }

    _data[time].emplace_back(std::move(new_line), entity_id);

    if (notify) {
        notifyObservers();
    }
}

void LineData::addAtTime(TimeFrameIndex const time, std::vector<Point2D<float>> const & line, bool notify) {
    addAtTime(time, Line2D(line), notify);
}

void LineData::addAtTime(TimeFrameIndex const time, Line2D const & line, bool notify) {
    int const local_index = static_cast<int>(_data[time].size());
    EntityId entity_id = 0;
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::LineEntity, time, local_index);
    }

    _data[time].emplace_back(line, entity_id);

    if (notify) {
        notifyObservers();
    }
}

void LineData::addPointToLine(TimeFrameIndex const time, int const line_id, Point2D<float> point, bool notify) {
    if (static_cast<size_t>(line_id) < _data[time].size()) {
        _data[time][static_cast<size_t>(line_id)].line.push_back(point);
    } else {
        std::cerr << "LineData::addPointToLine: line_id out of range" << std::endl;
        EntityId entity_id = 0;
        if (_identity_registry) {
            int const local_index = static_cast<int>(_data[time].size());
            entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::LineEntity, time, local_index);
        }
        _data[time].emplace_back(Line2D{point}, entity_id);
    }

    if (notify) {
        notifyObservers();
    }
}

void LineData::addPointToLineInterpolate(TimeFrameIndex const time, int const line_id, Point2D<float> point, bool notify) {
    if (static_cast<size_t>(line_id) >= _data[time].size()) {
        std::cerr << "LineData::addPointToLineInterpolate: line_id out of range" << std::endl;
        EntityId entity_id = 0;
        if (_identity_registry) {
            int const local_index = static_cast<int>(_data[time].size());
            entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::LineEntity, time, local_index);
        }
        _data[time].emplace_back(Line2D{}, entity_id);
    }

    Line2D & line = _data[time][static_cast<size_t>(line_id)].line;
    if (!line.empty()) {
        Point2D<float> const last_point = line.back();
        float const distance = std::sqrt(std::pow(point.x - last_point.x, 2.0f) + std::pow(point.y - last_point.y, 2.0f));
        int const n = static_cast<int>(distance / 2.0f);
        for (int i = 1; i <= n; ++i) {
            float const t = static_cast<float>(i) / static_cast<float>(n + 1);
            float const interp_x = last_point.x + t * (point.x - last_point.x);
            float const interp_y = last_point.y + t * (point.y - last_point.y);
            line.push_back(Point2D<float>{interp_x, interp_y});
        }
    }
    line.push_back(point);
    // Note: smooth_line function needs to be implemented or included
    // smooth_line(line);

    if (notify) {
        notifyObservers();
    }
}

void LineData::addEntryAtTime(TimeFrameIndex const time, Line2D const & line, EntityId entity_id, bool notify) {
    _data[time].emplace_back(line, entity_id);
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
        _temp_lines.push_back(entry.line);
    }

    return _temp_lines;
}

std::vector<Line2D> const & LineData::getAtTime(TimeFrameIndex const time,
                                                TimeFrame const * source_timeframe,
                                                TimeFrame const * line_timeframe) const {
    // Convert time if needed
    TimeFrameIndex converted_time = time;
    if (source_timeframe && line_timeframe && source_timeframe != line_timeframe) {
        auto time_value = source_timeframe->getTimeAtIndex(time);
        converted_time = line_timeframe->getIndexAtTime(static_cast<float>(time_value));
    }

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
                                                           TimeFrame const * source_timeframe,
                                                           TimeFrame const * line_timeframe) const {
    // Convert time if needed
    TimeFrameIndex converted_time = time;
    if (source_timeframe && line_timeframe && source_timeframe != line_timeframe) {
        auto time_value = source_timeframe->getTimeAtIndex(time);
        converted_time = line_timeframe->getIndexAtTime(static_cast<float>(time_value));
    }

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

std::optional<Line2D> LineData::getLineByEntityId(EntityId entity_id) const {
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

    return time_it->second[static_cast<size_t>(local_index)].line;
}

std::optional<std::reference_wrapper<Line2D>> LineData::getMutableLineByEntityId(EntityId entity_id) {
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

    return std::ref(time_it->second[static_cast<size_t>(local_index)].line);
}

std::optional<std::pair<TimeFrameIndex, int>> LineData::getTimeAndIndexByEntityId(EntityId entity_id) const {
    if (!_identity_registry) {
        return std::nullopt;
    }

    auto descriptor = _identity_registry->get(entity_id);
    if (!descriptor || descriptor->kind != EntityKind::LineEntity || descriptor->data_key != _identity_data_key) {
        return std::nullopt;
    }

    TimeFrameIndex const time{descriptor->time_value};
    int const local_index = descriptor->local_index;

    // Verify the time and index are valid
    auto time_it = _data.find(time);
    if (time_it == _data.end() || local_index < 0 || static_cast<size_t>(local_index) >= time_it->second.size()) {
        return std::nullopt;
    }

    return std::make_pair(time, local_index);
}

std::vector<std::pair<EntityId, Line2D>> LineData::getLinesByEntityIds(std::vector<EntityId> const & entity_ids) const {
    std::vector<std::pair<EntityId, Line2D>> results;
    results.reserve(entity_ids.size());

    for (EntityId const entity_id: entity_ids) {
        auto line = getLineByEntityId(entity_id);
        if (line.has_value()) {
            results.emplace_back(entity_id, std::move(line.value()));
        }
    }

    return results;
}

std::vector<std::tuple<EntityId, TimeFrameIndex, int>> LineData::getTimeInfoByEntityIds(std::vector<EntityId> const & entity_ids) const {
    std::vector<std::tuple<EntityId, TimeFrameIndex, int>> results;
    results.reserve(entity_ids.size());

    for (EntityId const entity_id: entity_ids) {
        auto time_info = getTimeAndIndexByEntityId(entity_id);
        if (time_info.has_value()) {
            results.emplace_back(entity_id, time_info->first, time_info->second);
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
            for (auto & point: entry.line) {
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

    // Iterate through all times in the source data within the interval
    for (auto const & [time, entries]: _data) {
        if (time >= interval.start && time <= interval.end && !entries.empty()) {
            for (auto const & entry: entries) {
                target.addAtTime(time, entry.line, false);// Don't notify for each operation
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

std::size_t LineData::copyTo(LineData & target, std::vector<TimeFrameIndex> const & times, bool notify) const {
    std::size_t total_lines_copied = 0;

    // Copy lines for each specified time
    for (TimeFrameIndex const time: times) {
        auto it = _data.find(time);
        if (it != _data.end() && !it->second.empty()) {
            for (auto const & entry: it->second) {
                target.addAtTime(time, entry.line, false);// Don't notify for each operation
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
                              [](LineEntry const & entry) -> Line2D const & { return entry.line; });
}

std::size_t LineData::moveByEntityIds(LineData & target, std::unordered_set<EntityId> const & entity_ids, bool notify) {
    auto result = move_by_entity_ids(_data, target, entity_ids, notify,
                                     [](LineEntry const & entry) -> Line2D const & { return entry.line; });
    
    if (notify && result > 0) {
        notifyObservers();
    }
    
    return result;
}