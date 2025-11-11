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
            _data[time].emplace_back(EntityId(0), line);// EntityId will be 0 initially
        }
    }
}

// Move constructor
LineData::LineData(LineData && other) noexcept
    : RaggedTimeSeries<Line2D>(std::move(other)) {};

// Move assignment operator
LineData & LineData::operator=(LineData && other) noexcept {
    RaggedTimeSeries<Line2D>::operator=(std::move(other));
    return *this;
}

// ========== Setters ==========

[[nodiscard]] std::optional<LineData::LineModifier> LineData::getMutableData(EntityId entity_id, NotifyObservers notify) {

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

    return LineModifier(line, [this, notify]() { if (notify == NotifyObservers::Yes) { this->notifyObservers(); } });
}

bool LineData::_clearAtTime(TimeFrameIndex const time, NotifyObservers notify) {
    auto it = _data.find(time);
    if (it != _data.end()) {
        _data.erase(it);
        if (notify == NotifyObservers::Yes) {
            notifyObservers();
        }
        return true;
    }
    return false;
}

bool LineData::clearAtTime(TimeIndexAndFrame const & time_index_and_frame, NotifyObservers notify) {

    TimeFrameIndex const converted_time = convert_time_index(time_index_and_frame.index,
                                                             time_index_and_frame.time_frame,
                                                             _time_frame.get());
    return _clearAtTime(converted_time, notify);
}

bool LineData::clearByEntityId(EntityId entity_id, NotifyObservers notify) {
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
    if (notify == NotifyObservers::Yes) {
        notifyObservers();
    }
    return true;
}

void LineData::addAtTime(TimeFrameIndex const time, Line2D const & line, NotifyObservers notify) {
    int const local_index = static_cast<int>(_data[time].size());
    auto entity_id = EntityId(0);
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::LineEntity, time, local_index);
    }

    _data[time].emplace_back(entity_id, line);

    if (notify == NotifyObservers::Yes) {
        notifyObservers();
    }
}

void LineData::addAtTime(TimeIndexAndFrame const & time_index_and_frame, Line2D const & line, NotifyObservers notify) {
    TimeFrameIndex const converted_time = convert_time_index(time_index_and_frame.index,
                                                             time_index_and_frame.time_frame,
                                                             _time_frame.get());
    addAtTime(converted_time, line, notify);
}

void LineData::addAtTime(TimeFrameIndex const time, Line2D && line, NotifyObservers notify) {
    int const local_index = static_cast<int>(_data[time].size());
    auto entity_id = EntityId(0);
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::LineEntity, time, local_index);
    }

    _data[time].emplace_back(entity_id, std::move(line));

    if (notify == NotifyObservers::Yes) {
        notifyObservers();
    }
}

void LineData::addAtTime(TimeIndexAndFrame const & time_index_and_frame, Line2D && line, NotifyObservers notify) {
    TimeFrameIndex const converted_time = convert_time_index(time_index_and_frame.index,
                                                             time_index_and_frame.time_frame,
                                                             _time_frame.get());
    addAtTime(converted_time, std::move(line), notify);
}

void LineData::addEntryAtTime(TimeFrameIndex const time, Line2D const & line, EntityId entity_id, NotifyObservers notify) {
    _data[time].emplace_back(entity_id, line);
    if (notify == NotifyObservers::Yes) {
        notifyObservers();
    }
}

void LineData::addAtTime(TimeFrameIndex const time, std::vector<Line2D> const & lines_to_add) {
    if (lines_to_add.empty()) {
        return;
    }

    // 1. Get (or create) the vector of entries for this time
    // This is our single map lookup.
    auto & entry_vec = _data[time];

    // 2. Reserve space once for high performance
    size_t const old_size = entry_vec.size();
    entry_vec.reserve(old_size + lines_to_add.size());

    // 3. Loop and emplace new entries
    for (size_t i = 0; i < lines_to_add.size(); ++i) {
        int const local_index = static_cast<int>(old_size + i);
        auto entity_id = EntityId(0);
        if (_identity_registry) {
            entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::LineEntity, time, local_index);
        }

        // Calls DataEntry(entity_id, lines_to_add[i])
        // This will invoke the Line2D copy constructor
        entry_vec.emplace_back(entity_id, lines_to_add[i]);
    }

    // Note: Following our "manual notify" pattern,
    // the caller is responsible for calling notifyObservers().
}

void LineData::addAtTime(TimeFrameIndex const time, std::vector<Line2D> && lines_to_add) {
    if (lines_to_add.empty()) {
        return;
    }

    // 1. Get (or create) the vector of entries for this time
    auto & entry_vec = _data[time];

    // 2. Reserve space once
    size_t const old_size = entry_vec.size();
    entry_vec.reserve(old_size + lines_to_add.size());

    // 3. Loop and emplace new entries
    for (size_t i = 0; i < lines_to_add.size(); ++i) {
        int const local_index = static_cast<int>(old_size + i);
        auto entity_id = EntityId(0);
        if (_identity_registry) {
            entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::LineEntity, time, local_index);
        }

        // Calls DataEntry(entity_id, std::move(lines_to_add[i]))
        // This will invoke the Line2D MOVE constructor
        entry_vec.emplace_back(entity_id, std::move(lines_to_add[i]));
    }
}
// ========== Getters ==========


// ========== Entity Lookup Methods ==========

std::optional<std::reference_wrapper<Line2D const>> LineData::getDataByEntityId(EntityId entity_id) const {
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

std::optional<TimeFrameIndex> LineData::getTimeByEntityId(EntityId entity_id) const {
    if (!_identity_registry) {
        return std::nullopt;
    }

    auto descriptor = _identity_registry->get(entity_id);
    return TimeFrameIndex{descriptor->time_value};
}


std::vector<std::pair<EntityId, std::reference_wrapper<Line2D const>>> LineData::getDataByEntityIds(std::vector<EntityId> const & entity_ids) const {
    std::vector<std::pair<EntityId, std::reference_wrapper<Line2D const>>> results;
    results.reserve(entity_ids.size());

    for (EntityId const entity_id: entity_ids) {
        auto line = getDataByEntityId(entity_id);
        if (line.has_value()) {
            results.emplace_back(entity_id, line.value());
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

// ========== Copy and Move ==========

std::size_t LineData::copyByEntityIds(LineData & target, std::unordered_set<EntityId> const & entity_ids, NotifyObservers notify) {
    bool const should_notify = (notify == NotifyObservers::Yes);
    return copy_by_entity_ids(_data, target, entity_ids, should_notify,
                              [](LineEntry const & entry) -> Line2D const & { return entry.data; });
}

std::size_t LineData::moveByEntityIds(LineData & target, std::unordered_set<EntityId> const & entity_ids, NotifyObservers notify) {
    bool const should_notify = (notify == NotifyObservers::Yes);
    auto result = move_by_entity_ids(_data, target, entity_ids, should_notify,
                                     [](LineEntry const & entry) -> Line2D const & { return entry.data; });

    if (notify == NotifyObservers::Yes && result > 0) {
        notifyObservers();
    }

    return result;
}