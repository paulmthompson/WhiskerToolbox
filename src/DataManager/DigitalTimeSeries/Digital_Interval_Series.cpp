#include "Digital_Interval_Series.hpp"
#include "Entity/EntityRegistry.hpp"

#include <algorithm>
#include <ranges>
#include <utility>
#include <vector>

// ========== Constructors ==========

DigitalIntervalSeries::DigitalIntervalSeries(std::vector<Interval> digital_vector) {
    // Sort the input
    std::sort(digital_vector.begin(), digital_vector.end());
    
    // Build owning storage directly
    OwningDigitalIntervalStorage new_storage;
    new_storage.reserve(digital_vector.size());
    for (auto const & interval : digital_vector) {
        new_storage.addInterval(interval, EntityId{0});
    }
    _storage = DigitalIntervalStorageWrapper{std::move(new_storage)};
    _cacheOptimizationPointers();
}

DigitalIntervalSeries::DigitalIntervalSeries(std::vector<std::pair<float, float>> const & digital_vector) {
    std::vector<Interval> intervals;
    intervals.reserve(digital_vector.size());
    for (auto const & interval : digital_vector) {
        intervals.emplace_back(Interval{static_cast<int64_t>(interval.first), static_cast<int64_t>(interval.second)});
    }
    std::sort(intervals.begin(), intervals.end());
    
    // Build owning storage directly
    OwningDigitalIntervalStorage new_storage;
    new_storage.reserve(intervals.size());
    for (auto const & interval : intervals) {
        new_storage.addInterval(interval, EntityId{0});
    }
    _storage = DigitalIntervalStorageWrapper{std::move(new_storage)};
    _cacheOptimizationPointers();
}

// ========== Getters ==========

void DigitalIntervalSeries::addEvent(Interval new_interval) {
    auto* owning = _storage.tryGetMutableOwning();
    if (!owning) {
        // Non-owning storage - need to materialize first
        auto materialized = materialize();
        _storage = std::move(materialized->_storage);
        owning = _storage.tryGetMutableOwning();
        if (!owning) {
            throw std::runtime_error("Failed to get mutable storage for addEvent");
        }
    }
    
    size_t const old_size = owning->size();
    _addEventInternal(new_interval);
    size_t const new_size = owning->size();
    
    // If an interval was actually added (not merged into existing), assign EntityId
    if (new_size > old_size && _identity_registry) {
        // Find the index of the newly added interval (search in storage)
        for (size_t idx = 0; idx < owning->size(); ++idx) {
            if (owning->getInterval(idx) == new_interval) {
                EntityId entity_id = _identity_registry->ensureId(
                    _identity_data_key,
                    EntityKind::IntervalEntity,
                    TimeFrameIndex{new_interval.start},
                    static_cast<int>(idx)
                );
                owning->setEntityId(idx, entity_id);
                break;
            }
        }
    }
    
    _cacheOptimizationPointers();
    notifyObservers();
}

void DigitalIntervalSeries::_addEventInternal(Interval new_interval) {
    auto* owning = _storage.tryGetMutableOwning();
    if (!owning) {
        return;  // Caller should have ensured mutable storage
    }
    
    // Collect indices of overlapping/contiguous intervals to merge
    std::vector<size_t> indices_to_remove;
    for (size_t i = 0; i < owning->size(); ++i) {
        Interval const existing = owning->getInterval(i);
        if (is_overlapping(existing, new_interval) || is_contiguous(existing, new_interval)) {
            new_interval.start = std::min(new_interval.start, existing.start);
            new_interval.end = std::max(new_interval.end, existing.end);
            indices_to_remove.push_back(i);
        } else if (is_contained(new_interval, existing)) {
            // The new interval is completely contained within an existing interval, so we do nothing.
            return;
        }
    }
    
    // Remove merged intervals in reverse order to maintain indices
    std::ranges::sort(indices_to_remove, std::greater<>{});
    for (size_t idx : indices_to_remove) {
        owning->removeAt(idx);
    }
    
    // Add the merged interval
    owning->addInterval(new_interval, EntityId{0});
    owning->sort();
}

void DigitalIntervalSeries::setEventAtTime(TimeFrameIndex time, bool const event) {
    _setEventAtTimeInternal(time, event);
    _cacheOptimizationPointers();
    notifyObservers();
}

bool DigitalIntervalSeries::removeInterval(Interval const & interval) {
    auto* owning = _storage.tryGetMutableOwning();
    if (!owning) {
        // Non-owning storage - need to materialize first
        auto materialized = materialize();
        _storage = std::move(materialized->_storage);
        owning = _storage.tryGetMutableOwning();
        if (!owning) {
            return false;
        }
    }
    
    for (size_t i = 0; i < owning->size(); ++i) {
        if (owning->getInterval(i) == interval) {
            owning->removeAt(i);
            _cacheOptimizationPointers();
            notifyObservers();
            return true;
        }
    }
    return false;
}

size_t DigitalIntervalSeries::removeIntervals(std::vector<Interval> const & intervals) {
    auto* owning = _storage.tryGetMutableOwning();
    if (!owning) {
        // Non-owning storage - need to materialize first
        auto materialized = materialize();
        _storage = std::move(materialized->_storage);
        owning = _storage.tryGetMutableOwning();
        if (!owning) {
            return 0;
        }
    }
    
    size_t removed_count = 0;
    
    // Collect indices to remove (search for each interval)
    std::vector<size_t> indices_to_remove;
    for (auto const & interval : intervals) {
        for (size_t i = 0; i < owning->size(); ++i) {
            if (owning->getInterval(i) == interval) {
                indices_to_remove.push_back(i);
                break;
            }
        }
    }
    
    // Remove in reverse order
    std::ranges::sort(indices_to_remove, std::greater<>{});
    for (size_t idx : indices_to_remove) {
        owning->removeAt(idx);
        removed_count++;
    }

    if (removed_count > 0) {
        owning->sort();
        _cacheOptimizationPointers();
        notifyObservers();
    }

    return removed_count;
}

void DigitalIntervalSeries::_setEventAtTimeInternal(TimeFrameIndex time, bool const event) {
    if (!event) {
        _removeEventAtTimeInternal(time);
    } else {
        _addEventInternal(Interval{time.getValue(), time.getValue()});
    }
}

void DigitalIntervalSeries::_removeEventAtTimeInternal(TimeFrameIndex const time) {
    auto* owning = _storage.tryGetMutableOwning();
    if (!owning) {
        return;  // Caller should ensure mutable storage
    }
    
    for (size_t i = 0; i < owning->size(); ++i) {
        Interval existing = owning->getInterval(i);
        if (is_contained(existing, time.getValue())) {
            if (time.getValue() == existing.start && time.getValue() == existing.end) {
                owning->removeAt(i);
            } else if (time.getValue() == existing.start) {
                owning->setInterval(i, Interval{time.getValue() + 1, existing.end});
            } else if (time.getValue() == existing.end) {
                owning->setInterval(i, Interval{existing.start, time.getValue() - 1});
            } else {
                // Split the interval
                auto preceding_event = Interval{existing.start, time.getValue() - 1};
                auto following_event = Interval{time.getValue() + 1, existing.end};
                owning->removeAt(i);
                owning->addInterval(preceding_event, EntityId{0});
                owning->addInterval(following_event, EntityId{0});
                owning->sort();
            }
            return;
        }
    }
}

void DigitalIntervalSeries::rebuildAllEntityIds() {
    auto* owning = _storage.tryGetMutableOwning();
    if (!owning) {
        return;  // Can't rebuild IDs on non-owning storage
    }
    
    for (size_t i = 0; i < owning->size(); ++i) {
        if (_identity_registry) {
            Interval const interval = owning->getInterval(i);
            EntityId id = _identity_registry->ensureId(
                _identity_data_key, 
                EntityKind::IntervalEntity, 
                TimeFrameIndex{interval.start}, 
                static_cast<int>(i));
            owning->setEntityId(i, id);
        } else {
            owning->setEntityId(i, EntityId{0});
        }
    }
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

    if (local_index < 0 || static_cast<size_t>(local_index) >= _storage.size()) {
        return std::nullopt;
    }

    return _storage.getInterval(static_cast<size_t>(local_index));
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
    auto const & intervals = digital_series->view();

    int closest_index = -1;
    int i = 0;
    for (auto const & interval : intervals) {
        if (interval.value().start <= time.getValue()) {
            closest_index = static_cast<int>(i);
            if (time.getValue() <= interval.value().end) {
                return static_cast<int>(i);
            }
        } else {
            break;
        }
        ++i;
    }
    return closest_index;
}

// ========== Storage Integration ==========

void DigitalIntervalSeries::_cacheOptimizationPointers() {
    _cached_storage = _storage.tryGetCache();
}

// ========== Factory Methods ==========

std::shared_ptr<DigitalIntervalSeries> DigitalIntervalSeries::createView(
    std::shared_ptr<DigitalIntervalSeries const> source,
    int64_t start,
    int64_t end)
{
    // Get shared owning storage from source (zero-copy via aliasing constructor)
    auto shared_owning = source->_storage.getSharedOwningStorage();
    if (!shared_owning) {
        // Source is lazy storage - materialize first, then recursively create view
        auto materialized = source->materialize();
        return createView(materialized, start, end);
    }
    
    // Create view from shared owning storage (no data copy)
    ViewDigitalIntervalStorage view{shared_owning};
    view.filterByOverlappingRange(start, end);
    
    auto result = std::make_shared<DigitalIntervalSeries>();
    result->_storage = DigitalIntervalStorageWrapper{std::move(view)};
    result->_time_frame = source->_time_frame;
    result->_identity_data_key = source->_identity_data_key;
    result->_identity_registry = source->_identity_registry;
    result->_cacheOptimizationPointers();
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
    
    // Get shared owning storage from source (zero-copy via aliasing constructor)
    auto shared_owning = source->_storage.getSharedOwningStorage();
    if (!shared_owning) {
        // Source is lazy storage - materialize first, then get shared storage
        auto materialized = source->materialize();
        shared_owning = materialized->_storage.getSharedOwningStorage();
        if (!shared_owning) {
            throw std::runtime_error("Failed to get shared storage for view creation");
        }
    }
    
    // Create view from shared owning storage (no data copy)
    ViewDigitalIntervalStorage view{shared_owning};
    view.filterByEntityIds(entity_ids);
    result->_storage = DigitalIntervalStorageWrapper{std::move(view)};
    
    result->_cacheOptimizationPointers();
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
    result->_cacheOptimizationPointers();
    return result;
}
