#include "Mask_Data.hpp"

#include "Entity/EntityRegistry.hpp"
#include "utils/map_timeseries.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <ranges>

// ========== Constructors ==========

// ========== Setters ==========

bool MaskData::clearAtTime(TimeFrameIndex const time, bool notify) {
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

bool MaskData::clearAtTime(TimeIndexAndFrame const & time_index_and_frame, bool notify) {

    TimeFrameIndex const converted_time = convert_time_index(time_index_and_frame.index,
        time_index_and_frame.time_frame.get(),
        _time_frame.get());
    return clearAtTime(converted_time, notify);
}

bool MaskData::clearByEntityId(EntityId entity_id, bool notify) {
    if (!_identity_registry) {
        return false;
    }

    auto descriptor = _identity_registry->get(entity_id);
    if (!descriptor || descriptor->kind != EntityKind::MaskEntity || descriptor->data_key != _identity_data_key) {
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


void MaskData::addAtTime(TimeFrameIndex const time, Mask2D const & mask, bool notify) {
    int const local_index = static_cast<int>(_data[time].size());
    EntityId entity_id = 0;
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::MaskEntity, time, local_index);
    }

    _data[time].emplace_back(entity_id, mask);

    if (notify) {
        notifyObservers();
    }
}

void MaskData::addAtTime(TimeFrameIndex const time, Mask2D && mask, bool notify) {
    int const local_index = static_cast<int>(_data[time].size());
    EntityId entity_id = 0;
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::MaskEntity, time, local_index);
    }

    _data[time].emplace_back(entity_id, std::move(mask));

    if (notify) {
        notifyObservers();
    }
}

void MaskData::addAtTime(TimeFrameIndex const time,
                         std::vector<uint32_t> const & x,
                         std::vector<uint32_t> const & y,
                         bool notify) {
    auto new_mask = create_mask(x, y);
    int const local_index = static_cast<int>(_data[time].size());
    EntityId entity_id = 0;
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::MaskEntity, time, local_index);
    }
    _data[time].emplace_back(entity_id, std::move(new_mask));

    if (notify) {
        notifyObservers();
    }
}


void MaskData::addAtTime(TimeIndexAndFrame const & time_index_and_frame,
                         std::vector<Point2D<uint32_t>> mask,
                         bool notify) {
    if (time_index_and_frame.time_frame.get() == _time_frame.get()) {
        addAtTime(time_index_and_frame.index, std::move(mask), notify);
        return;
    }

    if (!time_index_and_frame.time_frame || !_time_frame.get()) {
        return;
    }

    auto time = time_index_and_frame.time_frame->getTimeAtIndex(time_index_and_frame.index);
    auto time_index = _time_frame->getIndexAtTime(static_cast<float>(time));

    int const local_index = static_cast<int>(_data[time_index].size());
    EntityId entity_id = 0;
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::MaskEntity, time_index, local_index);
    }
    _data[time_index].emplace_back(entity_id, std::move(mask));
}

void MaskData::addAtTime(TimeFrameIndex const time,
                         std::vector<uint32_t> && x,
                         std::vector<uint32_t> && y,
                         bool notify) {
    // Create mask efficiently using move semantics
    auto new_mask = Mask2D{};
    auto xx = std::move(x);
    auto yy = std::move(y);
    new_mask.reserve(xx.size());

    for (std::size_t i = 0; i < xx.size(); i++) {
        new_mask.emplace_back(xx[i], yy[i]);
    }

    int const local_index = static_cast<int>(_data[time].size());
    EntityId entity_id = 0;
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::MaskEntity, time, local_index);
    }
    _data[time].emplace_back(entity_id, std::move(new_mask));

    if (notify) {
        notifyObservers();
    }
}


void MaskData::addEntryAtTime(TimeFrameIndex const time,
                              Mask2D const & mask,
                              EntityId const entity_id,
                              bool const notify) {
    _data[time].emplace_back(entity_id, mask);
    if (notify) {
        notifyObservers();
    }
}

// ========== Getters ==========

std::vector<Mask2D> const & MaskData::getAtTime(TimeFrameIndex const time) const {
    auto it = _data.find(time);
    if (it == _data.end()) {
        return _empty;
    }
    _temp_masks.clear();
    _temp_masks.reserve(it->second.size());
    for (auto const & entry: it->second) {
        _temp_masks.push_back(entry.data);
    }
    return _temp_masks;
}

std::vector<Mask2D> const & MaskData::getAtTime(TimeIndexAndFrame const & time_index_and_frame) const {
    auto converted = convert_time_index(time_index_and_frame.index,
                                        time_index_and_frame.time_frame.get(),
                                        _time_frame.get());
    return getAtTime(converted);
}

std::vector<Mask2D> const & MaskData::getAtTime(TimeFrameIndex const time,
                                                TimeFrame const & source_timeframe) const {

    TimeFrameIndex const converted = convert_time_index(time, &source_timeframe, _time_frame.get());
    return getAtTime(converted);
}

// ========== Image Size ==========

void MaskData::changeImageSize(ImageSize const & image_size) {
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
        (void) time;
        for (auto & entry: entries) {
            for (auto & point: entry.data) {
                point.x = static_cast<uint32_t>(std::round(static_cast<float>(point.x) * scale_x));
                point.y = static_cast<uint32_t>(std::round(static_cast<float>(point.y) * scale_y));
            }
        }
    }
    _image_size = image_size;
}

// ========== Copy and Move ==========


std::size_t MaskData::copyByEntityIds(MaskData & target, std::unordered_set<EntityId> const & entity_ids, bool const notify) {
    return copy_by_entity_ids(_data, target, entity_ids, notify,
                              [](MaskEntry const & entry) -> Mask2D const & { return entry.data; });
}

std::size_t MaskData::moveByEntityIds(MaskData & target, std::unordered_set<EntityId> const & entity_ids, bool notify) {
    auto result = move_by_entity_ids(_data, target, entity_ids, notify,
                                     [](MaskEntry const & entry) -> Mask2D const & { return entry.data; });
    
    if (notify && result > 0) {
        notifyObservers();
    }
    
    return result;
}

void MaskData::setIdentityContext(std::string const & data_key, EntityRegistry * registry) {
    _identity_data_key = data_key;
    _identity_registry = registry;
}

void MaskData::rebuildAllEntityIds() {
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
            entries[static_cast<size_t>(i)].entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::IntervalEntity, t, i);
        }
    }
}

std::vector<EntityId> const & MaskData::getEntityIdsAtTime(TimeFrameIndex time) const {
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

std::vector<EntityId> MaskData::getAllEntityIds() const {
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

std::optional<Mask2D> MaskData::getMaskByEntityId(EntityId entity_id) const {
    if (!_identity_registry) {
        return std::nullopt;
    }

    auto descriptor = _identity_registry->get(entity_id);
    if (!descriptor || descriptor->kind != EntityKind::IntervalEntity || descriptor->data_key != _identity_data_key) {
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

std::optional<std::pair<TimeFrameIndex, int>> MaskData::getTimeAndIndexByEntityId(EntityId entity_id) const {
    if (!_identity_registry) {
        return std::nullopt;
    }

    auto descriptor = _identity_registry->get(entity_id);
    if (!descriptor || descriptor->kind != EntityKind::IntervalEntity || descriptor->data_key != _identity_data_key) {
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

std::vector<std::pair<EntityId, Mask2D>> MaskData::getMasksByEntityIds(std::vector<EntityId> const & entity_ids) const {
    std::vector<std::pair<EntityId, Mask2D>> results;
    results.reserve(entity_ids.size());

    for (EntityId const entity_id: entity_ids) {
        auto mask = getMaskByEntityId(entity_id);
        if (mask.has_value()) {
            results.emplace_back(entity_id, std::move(mask.value()));
        }
    }

    return results;
}

std::vector<std::tuple<EntityId, TimeFrameIndex, int>> MaskData::getTimeInfoByEntityIds(std::vector<EntityId> const & entity_ids) const {
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
