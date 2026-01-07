#include "Digital_Interval_Series.hpp"
#include "Entity/EntityRegistry.hpp"

#include <algorithm>
#include <ranges>
#include <utility>
#include <vector>

// ========== Constructors ==========

DigitalIntervalSeries::DigitalIntervalSeries(std::vector<Interval> digital_vector) {
    _data = std::move(digital_vector);
    _sortData();
    _syncStorageFromData();
}

DigitalIntervalSeries::DigitalIntervalSeries(std::vector<std::pair<float, float>> const & digital_vector) {
    std::vector<Interval> intervals;
    intervals.reserve(digital_vector.size());
    for (auto & interval: digital_vector) {
        intervals.emplace_back(Interval{static_cast<int64_t>(interval.first), static_cast<int64_t>(interval.second)});
    }
    _data = std::move(intervals);
    _sortData();
    _syncStorageFromData();
}

// ========== Getters ==========

std::vector<Interval> const & DigitalIntervalSeries::getDigitalIntervalSeries() const {
    return _data;
}

bool DigitalIntervalSeries::isEventAtTime(TimeFrameIndex const time) const {

    auto Contained = [time](auto const & event) {
        return is_contained(event, time.getValue());
    };

    if (std::ranges::any_of(_data, Contained)) return true;

    return false;
}

void DigitalIntervalSeries::addEvent(Interval new_interval) {
    size_t const old_size = _data.size();
    _addEvent(new_interval);
    size_t const new_size = _data.size();
    
    // If an interval was actually added (not merged into existing), assign EntityId
    if (new_size > old_size && _identity_registry) {
        // Find the index of the newly added interval
        auto it = std::ranges::find(_data, new_interval);
        if (it != _data.end()) {
            size_t const idx = static_cast<size_t>(std::distance(_data.begin(), it));
            
            // Ensure _entity_ids vector is properly sized
            if (_entity_ids.size() < new_size) {
                _entity_ids.resize(new_size, EntityId{0});
            }
            
            // Generate EntityId for the new interval
            EntityId entity_id = _identity_registry->ensureId(
                _identity_data_key,
                EntityKind::IntervalEntity,
                TimeFrameIndex{new_interval.start},
                static_cast<int>(idx)
            );
            _entity_ids[idx] = entity_id;
        }
    } else if (new_size > old_size) {
        // No registry, just ensure vector is sized with zero IDs
        _entity_ids.resize(new_size, EntityId{0});
    }
    // Note: If intervals were merged (new_size <= old_size), entity IDs may need rebuilding
    // via rebuildAllEntityIds() if precise tracking is needed
    
    _syncStorageFromData();
    notifyObservers();
}

void DigitalIntervalSeries::_addEvent(Interval new_interval) {
    auto it = _data.begin();
    while (it != _data.end()) {
        if (is_overlapping(*it, new_interval) || is_contiguous(*it, new_interval)) {
            new_interval.start = std::min(new_interval.start, it->start);
            new_interval.end = std::max(new_interval.end, it->end);
            it = _data.erase(it);
        } else if (is_contained(new_interval, *it)) {
            // The new interval is completely contained within an existing interval, so we do nothing.
            return;
        } else {
            ++it;
        }
    }
    _data.push_back(new_interval);
    _sortData();
}

void DigitalIntervalSeries::setEventAtTime(TimeFrameIndex time, bool const event) {
    _setEventAtTime(time, event);
    _syncStorageFromData();
    notifyObservers();
}

bool DigitalIntervalSeries::removeInterval(Interval const & interval) {
    auto it = std::ranges::find(_data, interval);
    if (it != _data.end()) {
        _data.erase(it);
        _syncStorageFromData();
        notifyObservers();
        return true;
    }
    return false;
}

size_t DigitalIntervalSeries::removeIntervals(std::vector<Interval> const & intervals) {
    size_t removed_count = 0;

    for (auto const & interval: intervals) {
        auto it = std::ranges::find(_data, interval);
        if (it != _data.end()) {
            _data.erase(it);
            removed_count++;
        }
    }

    if (removed_count > 0) {
        _sortData();// Re-sort after removals
        _syncStorageFromData();
        notifyObservers();
    }

    return removed_count;
}

void DigitalIntervalSeries::_setEventAtTime(TimeFrameIndex time, bool const event) {
    if (!event) {
        _removeEventAtTime(time);
    } else {
        _addEvent(Interval{time.getValue(), time.getValue()});
    }
}

void DigitalIntervalSeries::_removeEventAtTime(TimeFrameIndex const time) {
    for (auto it = _data.begin(); it != _data.end(); ++it) {
        if (is_contained(*it, time.getValue())) {
            if (time.getValue() == it->start && time.getValue() == it->end) {
                _data.erase(it);
            } else if (time.getValue() == it->start) {
                it->start = time.getValue() + 1;
            } else if (time.getValue() == it->end) {
                it->end = time.getValue() - 1;
            } else {
                auto preceding_event = Interval{it->start, time.getValue() - 1};
                auto following_event = Interval{time.getValue() + 1, it->end};
                _data.erase(it);
                _data.push_back(preceding_event);
                _data.push_back(following_event);

                _sortData();
            }
            return;
        }
    }
}

void DigitalIntervalSeries::_sortData() {
    std::sort(_data.begin(), _data.end());
}

void DigitalIntervalSeries::rebuildAllEntityIds() {
    if (!_identity_registry) {
        _entity_ids.assign(_data.size(), EntityId(0));
        _syncStorageFromData();
        return;
    }
    _entity_ids.clear();
    _entity_ids.reserve(_data.size());
    for (size_t i = 0; i < _data.size(); ++i) {
        // Use start as the discrete time index representative, and i as stable local index
        _entity_ids.push_back(
                _identity_registry->ensureId(_identity_data_key, EntityKind::IntervalEntity, TimeFrameIndex{_data[i].start}, static_cast<int>(i)));
    }
    _syncStorageFromData();
}

// ========== Entity Lookup Methods ==========

std::optional<Interval> DigitalIntervalSeries::getIntervalByEntityId(EntityId entity_id) const {
    if (!_identity_registry) {
        return std::nullopt;
    }

    auto descriptor = _identity_registry->get(entity_id);
    if (!descriptor || descriptor->kind != EntityKind::IntervalEntity || descriptor->data_key != _identity_data_key) {
        return std::nullopt;
    }

    int const local_index = descriptor->local_index;

    if (local_index < 0 || static_cast<size_t>(local_index) >= _data.size()) {
        return std::nullopt;
    }

    return _data[static_cast<size_t>(local_index)];
}

std::optional<int> DigitalIntervalSeries::getIndexByEntityId(EntityId entity_id) const {
    if (!_identity_registry) {
        return std::nullopt;
    }

    auto descriptor = _identity_registry->get(entity_id);
    if (!descriptor || descriptor->kind != EntityKind::IntervalEntity || descriptor->data_key != _identity_data_key) {
        return std::nullopt;
    }

    int const local_index = descriptor->local_index;

    if (local_index < 0 || static_cast<size_t>(local_index) >= _data.size()) {
        return std::nullopt;
    }

    return local_index;
}

std::vector<std::pair<EntityId, Interval>> DigitalIntervalSeries::getIntervalsByEntityIds(std::vector<EntityId> const & entity_ids) const {
    std::vector<std::pair<EntityId, Interval>> result;
    result.reserve(entity_ids.size());

    for (EntityId const entity_id: entity_ids) {
        auto interval = getIntervalByEntityId(entity_id);
        if (interval) {
            result.emplace_back(entity_id, *interval);
        }
    }

    return result;
}

std::vector<std::pair<EntityId, int>> DigitalIntervalSeries::getIndexInfoByEntityIds(std::vector<EntityId> const & entity_ids) const {
    std::vector<std::pair<EntityId, int>> result;
    result.reserve(entity_ids.size());

    for (EntityId const entity_id: entity_ids) {
        auto index = getIndexByEntityId(entity_id);
        if (index) {
            result.emplace_back(entity_id, *index);
        }
    }

    return result;
}

// ========== Intervals with EntityIDs ==========

std::vector<IntervalWithId> DigitalIntervalSeries::getIntervalsWithIdsInRange(TimeFrameIndex start_time, TimeFrameIndex stop_time) const {
    
    std::vector<IntervalWithId> result;
   // result.reserve(_data.size());// Reserve space for potential worst case

    for (size_t i = 0; i < _data.size(); ++i) {
        Interval const & interval = _data[i];
        // Check if interval overlaps with the range (using overlapping logic)
        if (interval.start <= stop_time.getValue() && interval.end >= start_time.getValue()) {
            EntityId const entity_id = (i < _entity_ids.size()) ? _entity_ids[i] : EntityId(0);
            result.emplace_back(interval, entity_id);
        }
    }
    return result;
}

std::vector<IntervalWithId> DigitalIntervalSeries::getIntervalsWithIdsInRange(TimeFrameIndex start_index,
                                                                              TimeFrameIndex stop_index,
                                                                              TimeFrame const & source_time_frame) const {
    if (&source_time_frame == _time_frame.get()) {
        return getIntervalsWithIdsInRange(start_index, stop_index);
    }

    // If either timeframe is null, fall back to original behavior
    if (!_time_frame.get()) {
        return getIntervalsWithIdsInRange(start_index, stop_index);
    }

    auto [target_start_index, target_stop_index] = convertTimeFrameRange(start_index, stop_index, source_time_frame, *_time_frame);
    return getIntervalsWithIdsInRange(target_start_index, target_stop_index);
}

std::pair<int64_t, int64_t> DigitalIntervalSeries::_getTimeRangeFromIndices(
        TimeFrameIndex start_index,
        TimeFrameIndex stop_index) const {

    if (_time_frame) {
        auto start_time_value = _time_frame->getTimeAtIndex(start_index);
        auto stop_time_value = _time_frame->getTimeAtIndex(stop_index);
        return {static_cast<int64_t>(start_time_value), static_cast<int64_t>(stop_time_value)};
    } else {
        // Fallback to using indices as time values if no timeframe
        return {start_index.getValue(), stop_index.getValue()};
    }
}

int find_closest_preceding_event(DigitalIntervalSeries * digital_series, TimeFrameIndex time) {
    auto const & events = digital_series->getDigitalIntervalSeries();

    // Check if sorted
    for (size_t i = 1; i < events.size(); ++i) {
        if (events[i].start < events[i - 1].start) {
            throw std::runtime_error("DigitalIntervalSeries is not sorted");
        }
    }
    int closest_index = -1;
    for (size_t i = 0; i < events.size(); ++i) {
        if (events[i].start <= time.getValue()) {
            closest_index = static_cast<int>(i);
            if (time.getValue() <= events[i].end) {
                return static_cast<int>(i);
            }
        } else {
            break;
        }
    }
    return closest_index;
}

// ========== Storage Integration ==========

void DigitalIntervalSeries::_syncStorageFromData() {
    // Rebuild owning storage from legacy _data
    auto* owning = _storage.tryGetMutableOwning();
    if (owning) {
        owning->clear();
        for (size_t i = 0; i < _data.size(); ++i) {
            EntityId id = (i < _entity_ids.size()) ? _entity_ids[i] : EntityId{0};
            owning->addInterval(_data[i], id);
        }
    }
}

// ========== Factory Methods ==========

std::shared_ptr<DigitalIntervalSeries> DigitalIntervalSeries::createView(
    std::shared_ptr<DigitalIntervalSeries const> source,
    int64_t start,
    int64_t end)
{
    auto result = std::make_shared<DigitalIntervalSeries>();
    result->_time_frame = source->_time_frame;
    result->_identity_data_key = source->_identity_data_key;
    result->_identity_registry = source->_identity_registry;
    
    // Get owning storage from source
    auto const* source_owning = source->_storage.tryGetOwning();
    if (!source_owning) {
        // Source is not owning - materialize first
        auto materialized = source->materialize();
        source_owning = materialized->_storage.tryGetOwning();
        if (!source_owning) {
            throw std::runtime_error("Failed to materialize source for view creation");
        }
        // Create view from materialized storage
        auto view_storage = std::make_shared<OwningDigitalIntervalStorage const>(*source_owning);
        ViewDigitalIntervalStorage view{view_storage};
        view.filterByOverlappingRange(start, end);
        result->_storage = DigitalIntervalStorageWrapper{std::move(view)};
    } else {
        // Create view from source owning storage
        auto shared_source = std::make_shared<OwningDigitalIntervalStorage const>(*source_owning);
        ViewDigitalIntervalStorage view{shared_source};
        view.filterByOverlappingRange(start, end);
        result->_storage = DigitalIntervalStorageWrapper{std::move(view)};
    }
    
    // Sync legacy _data for compatibility
    result->_data.clear();
    for (size_t i = 0; i < result->_storage.size(); ++i) {
        result->_data.push_back(result->_storage.getInterval(i));
        result->_entity_ids.push_back(result->_storage.getEntityId(i));
    }
    
    return result;
}

std::shared_ptr<DigitalIntervalSeries> DigitalIntervalSeries::createView(
    std::shared_ptr<DigitalIntervalSeries const> source,
    std::unordered_set<EntityId> const& entity_ids)
{
    auto result = std::make_shared<DigitalIntervalSeries>();
    result->_time_frame = source->_time_frame;
    result->_identity_data_key = source->_identity_data_key;
    result->_identity_registry = source->_identity_registry;
    
    // Get owning storage from source
    auto const* source_owning = source->_storage.tryGetOwning();
    if (!source_owning) {
        // Source is not owning - materialize first
        auto materialized = source->materialize();
        source_owning = materialized->_storage.tryGetOwning();
        if (!source_owning) {
            throw std::runtime_error("Failed to materialize source for view creation");
        }
        auto view_storage = std::make_shared<OwningDigitalIntervalStorage const>(*source_owning);
        ViewDigitalIntervalStorage view{view_storage};
        view.filterByEntityIds(entity_ids);
        result->_storage = DigitalIntervalStorageWrapper{std::move(view)};
    } else {
        auto shared_source = std::make_shared<OwningDigitalIntervalStorage const>(*source_owning);
        ViewDigitalIntervalStorage view{shared_source};
        view.filterByEntityIds(entity_ids);
        result->_storage = DigitalIntervalStorageWrapper{std::move(view)};
    }
    
    // Sync legacy _data for compatibility
    result->_data.clear();
    for (size_t i = 0; i < result->_storage.size(); ++i) {
        result->_data.push_back(result->_storage.getInterval(i));
        result->_entity_ids.push_back(result->_storage.getEntityId(i));
    }
    
    return result;
}

std::shared_ptr<DigitalIntervalSeries> DigitalIntervalSeries::materialize() const {
    auto result = std::make_shared<DigitalIntervalSeries>();
    result->_time_frame = _time_frame;
    result->_identity_data_key = _identity_data_key;
    result->_identity_registry = _identity_registry;
    
    // Copy all data to new owning storage
    OwningDigitalIntervalStorage new_storage;
    new_storage.reserve(_storage.size());
    
    for (size_t i = 0; i < _storage.size(); ++i) {
        new_storage.addInterval(_storage.getInterval(i), _storage.getEntityId(i));
    }
    
    result->_storage = DigitalIntervalStorageWrapper{std::move(new_storage)};
    
    // Sync legacy _data for compatibility
    result->_data.clear();
    result->_data.reserve(result->_storage.size());
    result->_entity_ids.clear();
    result->_entity_ids.reserve(result->_storage.size());
    
    for (size_t i = 0; i < result->_storage.size(); ++i) {
        result->_data.push_back(result->_storage.getInterval(i));
        result->_entity_ids.push_back(result->_storage.getEntityId(i));
    }
    
    return result;
}
