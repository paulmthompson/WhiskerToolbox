#include "Digital_Interval_Series.hpp"

#include "storage/OwningDigitalIntervalStorage.hpp"
#include "storage/ViewDigitalIntervalStorage.hpp"

#include "Entity/EntityRegistry.hpp"

#include <algorithm>
#include <cassert>
#include <climits>
#include <iostream>
#include <limits>
#include <ranges>
#include <utility>
#include <vector>

namespace {

/**
 * @brief Get or create an EntityId keyed on interval (start, end) bounds.
 * @pre interval.end >= 0 && interval.end <= INT_MAX
 */
EntityId ensureIntervalEntityId(EntityRegistry & registry,
                                std::string const & data_key,
                                TimeFrameInterval const & interval) {
    assert(interval.end.getValue() >= 0 && interval.end.getValue() <= INT_MAX);
    return registry.ensureId(data_key,
                             EntityKind::IntervalEntity,
                             interval.start,
                             interval.end.getValue());
}

}// namespace

// ========== Constructors ==========

DigitalIntervalSeries::DigitalIntervalSeries(std::vector<TimeFrameInterval> digital_vector) {
    // Sort the input
    std::sort(digital_vector.begin(), digital_vector.end());

    // Build owning storage directly
    OwningDigitalIntervalStorage new_storage;
    new_storage.reserve(digital_vector.size());
    for (auto const & interval: digital_vector) {
        new_storage.addInterval(interval, EntityId{0});
    }
    _storage = DigitalIntervalStorageWrapper{std::move(new_storage)};
    _cacheOptimizationPointers();
    _syncStorageDisjointHint();
}

DigitalIntervalSeries::DigitalIntervalSeries(std::vector<std::pair<float, float>> const & digital_vector) {
    std::vector<TimeFrameInterval> intervals;
    intervals.reserve(digital_vector.size());
    for (auto const & interval: digital_vector) {
        intervals.emplace_back(TimeFrameInterval{TimeFrameIndex{static_cast<int64_t>(interval.first)},
                                                 TimeFrameIndex{static_cast<int64_t>(interval.second)}});
    }
    std::sort(intervals.begin(), intervals.end());

    // Build owning storage directly
    OwningDigitalIntervalStorage new_storage;
    new_storage.reserve(intervals.size());
    for (auto const & interval: intervals) {
        new_storage.addInterval(interval, EntityId{0});
    }
    _storage = DigitalIntervalStorageWrapper{std::move(new_storage)};
    _cacheOptimizationPointers();
    _syncStorageDisjointHint();
}

std::shared_ptr<DigitalIntervalSeries> DigitalIntervalSeries::createOverlapping(
        std::vector<TimeFrameInterval> intervals,
        std::shared_ptr<TimeFrame> time_frame) {
    auto result = std::make_shared<DigitalIntervalSeries>();
    result->_layout = IntervalLayout::Overlapping;
    result->_time_frame = std::move(time_frame);

    std::sort(intervals.begin(), intervals.end());
    OwningDigitalIntervalStorage new_storage;
    new_storage.reserve(intervals.size());
    for (auto const & interval: intervals) {
        new_storage.addInterval(interval, EntityId{0});
    }
    new_storage.setAssumeDisjointIntervals(false);
    result->_storage = DigitalIntervalStorageWrapper{std::move(new_storage)};
    result->_cacheOptimizationPointers();
    return result;
}

// ========== Getters ==========

void DigitalIntervalSeries::addEvent(TimeFrameInterval new_interval) {
    auto * owning = _storage.tryGetMutableOwning();
    if (!owning) {
        // Non-owning storage - need to materialize first
        auto materialized = materialize();
        _storage = std::move(materialized->_storage);
        owning = _storage.tryGetMutableOwning();
        if (!owning) {
            throw std::runtime_error("Failed to get mutable storage for addEvent");
        }
        _syncStorageDisjointHint();
    }

    auto const result_interval = _addEventInternal(new_interval);

    if (result_interval && _identity_registry) {
        if (auto const idx = owning->findByInterval(*result_interval)) {
            if (owning->getEntityId(*idx) == EntityId{0}) {
                EntityId const entity_id = ensureIntervalEntityId(
                        *_identity_registry, _identity_data_key, *result_interval);
                owning->setEntityId(*idx, entity_id);
            }
        }
    }

    _cacheOptimizationPointers();
    notifyObservers();
}

void DigitalIntervalSeries::addEvent(TimeFrameIndex start, TimeFrameIndex end) {

    if (start > end) {
        std::cout << "Start time is greater than end time" << std::endl;
        return;
    }

    addEvent(TimeFrameInterval{start, end});
}

std::optional<TimeFrameInterval> DigitalIntervalSeries::_addEventInternal(TimeFrameInterval new_interval) {
    auto * owning = _storage.tryGetMutableOwning();
    if (!owning) {
        return std::nullopt;// Caller should have ensured mutable storage
    }

    if (_layout == IntervalLayout::Overlapping) {
        if (owning->addInterval(new_interval, EntityId{0})) {
            return new_interval;
        }
        return std::nullopt;
    }

    // Collect indices of overlapping/contiguous intervals to merge
    std::vector<size_t> indices_to_remove;
    EntityId inherited_id{0};
    int64_t min_start = std::numeric_limits<int64_t>::max();

    for (size_t i = 0; i < owning->size(); ++i) {
        TimeFrameInterval const existing = owning->getInterval(i);
        if (is_overlapping(existing, new_interval) || is_contiguous(existing, new_interval)) {
            new_interval.start = std::min(new_interval.start, existing.start);
            new_interval.end = std::max(new_interval.end, existing.end);
            indices_to_remove.push_back(i);

            if (existing.start.getValue() < min_start) {
                min_start = existing.start.getValue();
                inherited_id = owning->getEntityId(i);
            } else if (existing.start.getValue() == min_start && inherited_id == EntityId{0}) {
                inherited_id = owning->getEntityId(i);
            }
        } else if (is_contained(new_interval, existing)) {
            // The new interval is completely contained within an existing interval, so we do nothing.
            return std::nullopt;
        }
    }

    // Remove merged intervals in reverse order to maintain indices
    std::ranges::sort(indices_to_remove, std::greater<>{});
    for (size_t const idx: indices_to_remove) {
        owning->removeAt(idx);
    }

    // Add the merged interval
    owning->addInterval(new_interval, inherited_id);
    owning->sort();

    if (inherited_id != EntityId{0} && _identity_registry) {
        _identity_registry->rebindKey(inherited_id,
                                      TimeFrameIndex{new_interval.start},
                                      new_interval.end.getValue());
    }

    return new_interval;
}

void DigitalIntervalSeries::setEventAtTime(TimeFrameIndex time, bool const event) {
    assert(_layout == IntervalLayout::Disjoint && "setEventAtTime requires Disjoint layout");
    _setEventAtTimeInternal(time, event);
    _cacheOptimizationPointers();
    notifyObservers();
}

bool DigitalIntervalSeries::removeInterval(TimeFrameInterval const & interval) {
    auto * owning = _storage.tryGetMutableOwning();
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

size_t DigitalIntervalSeries::removeIntervals(std::vector<TimeFrameInterval> const & intervals) {
    auto * owning = _storage.tryGetMutableOwning();
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
    for (auto const & interval: intervals) {
        for (size_t i = 0; i < owning->size(); ++i) {
            if (owning->getInterval(i) == interval) {
                indices_to_remove.push_back(i);
                break;
            }
        }
    }

    // Remove in reverse order
    std::ranges::sort(indices_to_remove, std::greater<>{});
    for (size_t const idx: indices_to_remove) {
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
    assert(_layout == IntervalLayout::Disjoint && "setEventAtTime requires Disjoint layout");
    if (!event) {
        _removeEventAtTimeInternal(time);
    } else {
        _addEventInternal(TimeFrameInterval{time, time});
    }
}

void DigitalIntervalSeries::_removeEventAtTimeInternal(TimeFrameIndex const time) {
    auto * owning = _storage.tryGetMutableOwning();
    if (!owning) {
        return;// Caller should ensure mutable storage
    }

    for (size_t i = 0; i < owning->size(); ++i) {
        TimeFrameInterval const existing = owning->getInterval(i);
        if (is_contained(existing, time)) {
            if (time == existing.start && time == existing.end) {
                owning->removeAt(i);
            } else if (time == existing.start) {
                owning->setInterval(i, TimeFrameInterval{time + TimeFrameIndex{1}, existing.end});
            } else if (time == existing.end) {
                owning->setInterval(i, TimeFrameInterval{existing.start, time - TimeFrameIndex{1}});
            } else {
                // Split the interval
                auto preceding_event = TimeFrameInterval{existing.start, time - TimeFrameIndex{1}};
                auto following_event = TimeFrameInterval{time + TimeFrameIndex{1}, existing.end};
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
    auto * owning = _storage.tryGetMutableOwning();
    if (!owning) {
        return;// Can't rebuild IDs on non-owning storage
    }

    for (size_t i = 0; i < owning->size(); ++i) {
        if (_identity_registry) {
            TimeFrameInterval const interval = owning->getInterval(i);
            EntityId const id = ensureIntervalEntityId(*_identity_registry, _identity_data_key, interval);
            owning->setEntityId(i, id);
        } else {
            owning->setEntityId(i, EntityId{0});
        }
    }
}

// ========== Entity-Based Bulk Operations ==========

std::size_t DigitalIntervalSeries::copyByEntityIds(
        DigitalIntervalSeries & target,
        std::unordered_set<EntityId> const & entity_ids,
        NotifyObservers const notify) {
    std::size_t count = 0;

    for (size_t i = 0; i < _storage.size(); ++i) {
        EntityId const eid = _storage.getEntityId(i);
        if (entity_ids.contains(eid)) {
            // Use addEvent so the target handles merging and EntityId assignment
            target.addEvent(_storage.getInterval(i));
            ++count;
        }
    }

    if (notify == NotifyObservers::Yes && count > 0) {
        target.notifyObservers();
    }
    return count;
}

// ========== Entity Lookup Methods ==========

std::optional<ClockTicksInterval> DigitalIntervalSeries::getIntervalByEntityId(EntityId entity_id) const {
    if (!_identity_registry) {
        return std::nullopt;
    }

    auto descriptor = _identity_registry->get(entity_id);
    if (!descriptor || descriptor->kind != EntityKind::IntervalEntity || descriptor->data_key != _identity_data_key) {
        return std::nullopt;
    }

    if (auto const idx = _storage.findByEntityId(entity_id)) {
        TimeFrameInterval interval = _storage.getInterval(*idx);
        return toClockTicksInterval(interval, *_time_frame);
    }

    return std::nullopt;
}

std::vector<std::pair<EntityId, ClockTicksInterval>> DigitalIntervalSeries::getIntervalsByEntityIds(std::vector<EntityId> const & entity_ids) const {
    std::vector<std::pair<EntityId, ClockTicksInterval>> result;
    result.reserve(entity_ids.size());

    for (EntityId const entity_id: entity_ids) {
        auto interval = getIntervalByEntityId(entity_id);
        if (interval) {
            result.emplace_back(entity_id, *interval);
        }
    }

    return result;
}


std::pair<ClockTicks, ClockTicks> DigitalIntervalSeries::_getTimeRangeFromIndices(
        TimeFrameIndex start_index,
        TimeFrameIndex stop_index) const {

    if (_time_frame) {
        auto start_time_value = _time_frame->getTimeAtIndex(start_index);
        auto stop_time_value = _time_frame->getTimeAtIndex(stop_index);
        return {start_time_value, stop_time_value};
    } else {
        // We should never get here
        throw std::runtime_error("No time frame set for DigitalIntervalSeries");
    }
}

std::vector<ClockTicksInterval> DigitalIntervalSeries::_getClockTicksIntervalsClipped(
        TimeFrameIndex start_time,
        TimeFrameIndex stop_time) const {
    TimeFrame const * time_frame = _time_frame.get();
    assert(time_frame != nullptr && "_getClockTicksIntervalsClipped requires series time frame");

    std::vector<ClockTicksInterval> result;
    for (TimeFrameInterval const & interval: _getIntervalsAsVectorClipped(start_time, stop_time)) {
        result.push_back(toClockTicksInterval(interval, *time_frame));
    }
    return result;
}

int find_closest_preceding_event(DigitalIntervalSeries * digital_series, TimeFrameIndex time) {
    int closest_index = -1;
    for (size_t i = 0; i < digital_series->size(); ++i) {
        TimeFrameInterval const interval = digital_series->getStoredInterval(i);
        if (interval.start <= time) {
            closest_index = static_cast<int>(i);
            if (time <= interval.end) {
                return static_cast<int>(i);
            }
        } else {
            break;
        }
    }
    return closest_index;
}

// ========== Storage Integration ==========

void DigitalIntervalSeries::_cacheOptimizationPointers() {
    _cached_storage = _storage.tryGetCache();
}

void DigitalIntervalSeries::_syncStorageDisjointHint() {
    if (auto * owning = _storage.tryGetMutableOwning()) {
        owning->setAssumeDisjointIntervals(_layout == IntervalLayout::Disjoint);
    }
}

// ========== Factory Methods ==========

std::shared_ptr<DigitalIntervalSeries> DigitalIntervalSeries::createView(
        std::shared_ptr<DigitalIntervalSeries const> const & source,
        TimeFrameIndex start,
        TimeFrameIndex end) {
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
    result->_layout = source->layout();
    result->_storage = DigitalIntervalStorageWrapper{std::move(view)};
    result->_time_frame = source->_time_frame;
    result->_identity_data_key = source->_identity_data_key;
    result->_identity_registry = source->_identity_registry;
    result->_cacheOptimizationPointers();
    return result;
}

std::shared_ptr<DigitalIntervalSeries> DigitalIntervalSeries::createView(
        std::shared_ptr<DigitalIntervalSeries const> const & source,
        std::unordered_set<EntityId> const & entity_ids) {
    auto result = std::make_shared<DigitalIntervalSeries>();
    result->_layout = source->layout();
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
    result->_layout = _layout;
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
    result->_syncStorageDisjointHint();
    result->_cacheOptimizationPointers();
    return result;
}
