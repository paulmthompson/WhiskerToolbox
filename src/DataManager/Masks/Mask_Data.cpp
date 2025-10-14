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

    if (time_index_and_frame.time_frame.get() == _time_frame.get()) {
        return clearAtTime(time_index_and_frame.index, notify);
    }

    if (!time_index_and_frame.time_frame || !_time_frame.get()) {
        return false;
    }

    auto time = time_index_and_frame.time_frame->getTimeAtIndex(time_index_and_frame.index);
    auto time_index = _time_frame->getIndexAtTime(static_cast<float>(time));


    if (clear_at_time(time_index, _data)) {
        if (notify) {
            notifyObservers();
        }
        return true;
    }
    return false;
}

bool MaskData::clearAtTime(TimeFrameIndex const time, size_t const index, bool notify) {
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

void MaskData::addAtTime(TimeFrameIndex const time,
                         std::vector<uint32_t> const & x,
                         std::vector<uint32_t> const & y,
                         bool notify) {
    auto new_mask = create_mask(x, y);
    int const local_index = static_cast<int>(_data[time].size());
    EntityId entity_id = 0;
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::IntervalEntity, time, local_index);
    }
    _data[time].emplace_back(std::move(new_mask), entity_id);

    if (notify) {
        notifyObservers();
    }
}

void MaskData::addAtTime(TimeFrameIndex const time,
                         std::vector<Point2D<uint32_t>> mask,
                         bool notify) {
    int const local_index = static_cast<int>(_data[time].size());
    EntityId entity_id = 0;
    if (_identity_registry) {
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::IntervalEntity, time, local_index);
    }
    _data[time].emplace_back(std::move(mask), entity_id);

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
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::IntervalEntity, time_index, local_index);
    }
    _data[time_index].emplace_back(std::move(mask), entity_id);
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
        entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::IntervalEntity, time, local_index);
    }
    _data[time].emplace_back(std::move(new_mask), entity_id);

    if (notify) {
        notifyObservers();
    }
}

void MaskData::addEntryAtTime(TimeFrameIndex const time,
                              Mask2D const & mask,
                              EntityId const entity_id,
                              bool const notify) {
    _data[time].emplace_back(mask, entity_id);
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
        _temp_masks.push_back(entry.mask);
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
                                                TimeFrame const * source_timeframe,
                                                TimeFrame const * mask_timeframe) const {

    TimeFrameIndex const converted = convert_time_index(time, source_timeframe, mask_timeframe);
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
            for (auto & point: entry.mask) {
                point.x = static_cast<uint32_t>(std::round(static_cast<float>(point.x) * scale_x));
                point.y = static_cast<uint32_t>(std::round(static_cast<float>(point.y) * scale_y));
            }
        }
    }
    _image_size = image_size;
}

// ========== Copy and Move ==========

std::size_t MaskData::copyTo(MaskData & target, TimeFrameInterval const & interval, bool notify) const {
    if (interval.start > interval.end) {
        std::cerr << "MaskData::copyTo: interval start (" << interval.start.getValue()
                  << ") must be <= interval end (" << interval.end.getValue() << ")" << std::endl;
        return 0;
    }

    // Ensure target is not the same as source
    if (this == &target) {
        std::cerr << "MaskData::copyTo: Cannot copy to self" << std::endl;
        return 0;
    }

    std::size_t total_masks_copied = 0;

    // Iterate through all times in the source data within the interval
    for (auto const & [time, entries]: _data) {
        if (time >= interval.start && time <= interval.end && !entries.empty()) {
            for (auto const & entry: entries) {
                target.addAtTime(time, entry.mask, false);// Don't notify for each operation
                total_masks_copied++;
            }
        }
    }

    // Notify observer only once at the end if requested
    if (notify && total_masks_copied > 0) {
        target.notifyObservers();
    }

    return total_masks_copied;
}

std::size_t MaskData::copyTo(MaskData & target, std::vector<TimeFrameIndex> const & times, bool notify) const {
    std::size_t total_masks_copied = 0;

    // Copy masks for each specified time
    for (TimeFrameIndex const time: times) {
        auto it = _data.find(time);
        if (it != _data.end() && !it->second.empty()) {
            for (auto const & entry: it->second) {
                target.addAtTime(time, entry.mask, false);// Don't notify for each operation
                total_masks_copied++;
            }
        }
    }

    // Notify observer only once at the end if requested
    if (notify && total_masks_copied > 0) {
        target.notifyObservers();
    }

    return total_masks_copied;
}

std::size_t MaskData::moveTo(MaskData & target, TimeFrameInterval const & interval, bool notify) {
    if (interval.start > interval.end) {
        std::cerr << "MaskData::moveTo: interval start (" << interval.start.getValue()
                  << ") must be <= interval end (" << interval.end.getValue() << ")" << std::endl;
        return 0;
    }

    std::size_t total_masks_moved = 0;
    std::vector<TimeFrameIndex> times_to_clear;

    // First, copy all masks in the interval to target
    for (auto const & [time, entries]: _data) {
        if (time >= interval.start && time <= interval.end && !entries.empty()) {
            for (auto const & entry: entries) {
                target.addAtTime(time, entry.mask, false);// Don't notify for each operation
                total_masks_moved++;
            }
            times_to_clear.push_back(time);
        }
    }

    // Then, clear all the times from source
    for (TimeFrameIndex const time: times_to_clear) {
        (void) clearAtTime(time, false);// Don't notify for each operation
    }

    // Notify observers only once at the end if requested
    if (notify && total_masks_moved > 0) {
        target.notifyObservers();
        notifyObservers();
    }

    return total_masks_moved;
}

std::size_t MaskData::moveTo(MaskData & target, std::vector<TimeFrameIndex> const & times, bool notify) {
    std::size_t total_masks_moved = 0;
    std::vector<TimeFrameIndex> times_to_clear;

    // First, copy masks for each specified time to target
    for (TimeFrameIndex const time: times) {
        auto it = _data.find(time);
        if (it != _data.end() && !it->second.empty()) {
            for (auto const & entry: it->second) {
                target.addAtTime(time, entry.mask, false);// Don't notify for each operation
                total_masks_moved++;
            }
            times_to_clear.push_back(time);
        }
    }

    // Then, clear all the times from source
    for (TimeFrameIndex const time: times_to_clear) {
        (void) clearAtTime(time, false);// Don't notify for each operation
    }

    // Notify observers only once at the end if requested
    if (notify && total_masks_moved > 0) {
        target.notifyObservers();
        notifyObservers();
    }

    return total_masks_moved;
}

std::size_t MaskData::copyMasksByEntityIds(MaskData & target, std::vector<EntityId> const & entity_ids, bool const notify) {
    std::size_t total_copied = 0;
    for (auto const & [time, entries]: _data) {
        for (auto const & entry: entries) {
            if (std::ranges::find(entity_ids, entry.entity_id) != entity_ids.end()) {
                target.addAtTime(time, entry.mask, false);
                total_copied++;
            }
        }
    }
    if (notify && total_copied > 0) {
        target.notifyObservers();
    }
    return total_copied;
}

std::size_t MaskData::moveByEntityIds(MaskData & target, std::unordered_set<EntityId> const & entity_ids, bool notify) {
    auto result = move_by_entity_ids(_data, target, entity_ids, notify,
                                     [](MaskEntry const & entry) -> Mask2D const & { return entry.mask; });
    
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
