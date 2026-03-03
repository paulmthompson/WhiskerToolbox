#include "Digital_Event_Series.hpp"

#include "Entity/EntityRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>// std::sort

DigitalEventSeries::DigitalEventSeries()
    : _storage(),
      _cached_storage() {
    _cacheOptimizationPointers();
}

DigitalEventSeries::DigitalEventSeries(std::vector<TimeFrameIndex> event_vector) {
    // Use owning storage with the provided events
    _storage = DigitalEventStorageWrapper{
            OwningDigitalEventStorage{std::move(event_vector)}};
    _cacheOptimizationPointers();
}

void DigitalEventSeries::addEvent(TimeFrameIndex const event_time) {
    // Check if storage is mutable (owning)
    auto * owning = _storage.tryGetMutableOwning();
    if (!owning) {
        throw std::runtime_error("Cannot add events to view or lazy storage");
    }

    // Generate EntityId if registry is set
    EntityId entity_id{0};
    if (_identity_registry) {
        // Use size as local index for consistent ID generation
        size_t const local_idx = owning->size();
        entity_id = _identity_registry->ensureId(
                _identity_data_key,
                EntityKind::EventEntity,
                event_time,
                static_cast<int>(local_idx));
    }

    bool const added = owning->addEvent(event_time, entity_id);

    if (added) {
        _cacheOptimizationPointers();
        notifyObservers();
    }
}

bool DigitalEventSeries::removeEvent(TimeFrameIndex const event_time) {
    auto * owning = _storage.tryGetMutableOwning();
    if (!owning) {
        throw std::runtime_error("Cannot remove events from view or lazy storage");
    }

    bool const removed = owning->removeEvent(event_time);

    if (removed) {
        _cacheOptimizationPointers();
        notifyObservers();
    }

    return removed;
}

void DigitalEventSeries::addEvent(TimeFrameIndex const event_time, EntityId const entity_id, NotifyObservers const notify) {
    auto * owning = _storage.tryGetMutableOwning();
    if (!owning) {
        throw std::runtime_error("Cannot add events to view or lazy storage");
    }

    bool const added = owning->addEvent(event_time, entity_id);

    if (added) {
        _cacheOptimizationPointers();
        if (notify == NotifyObservers::Yes) {
            notifyObservers();
        }
    }
}

void DigitalEventSeries::clear() {
    auto * owning = _storage.tryGetMutableOwning();
    if (!owning) {
        throw std::runtime_error("Cannot clear view or lazy storage");
    }

    owning->clear();
    _cacheOptimizationPointers();
    notifyObservers();
}

std::size_t DigitalEventSeries::moveByEntityIds(
        DigitalEventSeries & target,
        std::unordered_set<EntityId> const & entity_ids,
        NotifyObservers const notify) {
    // Collect events to move (iterate in order to preserve ordering)
    struct EventEntry {
        TimeFrameIndex time;
        EntityId id;
    };
    std::vector<EventEntry> to_move;

    for (size_t i = 0; i < _storage.size(); ++i) {
        EntityId const eid = _storage.getEntityId(i);
        if (entity_ids.contains(eid)) {
            to_move.push_back({_storage.getEvent(i), eid});
        }
    }

    // Add to target (preserving EntityIds)
    for (auto const & entry: to_move) {
        target.addEvent(entry.time, entry.id, NotifyObservers::No);
    }

    // Remove from source by EntityId
    for (auto const & entry: to_move) {
        _storage.removeByEntityId(entry.id);
    }
    _cacheOptimizationPointers();

    if (notify == NotifyObservers::Yes && !to_move.empty()) {
        target.notifyObservers();
        notifyObservers();
    }
    return to_move.size();
}

std::size_t DigitalEventSeries::copyByEntityIds(
        DigitalEventSeries & target,
        std::unordered_set<EntityId> const & entity_ids,
        NotifyObservers const notify) {
    std::size_t count = 0;

    for (size_t i = 0; i < _storage.size(); ++i) {
        EntityId const eid = _storage.getEntityId(i);
        if (entity_ids.contains(eid)) {
            // Use addEvent (without EntityId) so target generates new EntityIds
            target.addEvent(_storage.getEvent(i));
            ++count;
        }
    }

    if (notify == NotifyObservers::Yes && count > 0) {
        target.notifyObservers();
    }
    return count;
}

std::size_t DigitalEventSeries::deleteByEntityIds(
        std::unordered_set<EntityId> const & entity_ids,
        NotifyObservers const notify) {
    // Collect EntityIds to delete (avoid modifying while iterating)
    std::vector<EntityId> to_delete;
    for (size_t i = 0; i < _storage.size(); ++i) {
        EntityId const eid = _storage.getEntityId(i);
        if (entity_ids.contains(eid)) {
            to_delete.push_back(eid);
        }
    }

    for (auto const & eid: to_delete) {
        _storage.removeByEntityId(eid);
    }
    _cacheOptimizationPointers();

    if (notify == NotifyObservers::Yes && !to_delete.empty()) {
        notifyObservers();
    }
    return to_delete.size();
}

void DigitalEventSeries::_cacheOptimizationPointers() {
    _cached_storage = _storage.tryGetCache();
}

void DigitalEventSeries::rebuildAllEntityIds() {
    auto * owning = _storage.tryGetMutableOwning();
    if (!owning) {
        // For view/lazy storage, just invalidate the cache
        return;
    }

    if (!_identity_registry) {
        // Clear all entity IDs to zero
        std::vector<EntityId> zero_ids(owning->size(), EntityId{0});
        owning->setEntityIds(std::move(zero_ids));
        _cacheOptimizationPointers();
        return;
    }

    std::vector<EntityId> new_ids;
    new_ids.reserve(owning->size());

    for (size_t i = 0; i < owning->size(); ++i) {
        // Use index as local_index for stable ID generation
        EntityId const id = _identity_registry->ensureId(
                _identity_data_key,
                EntityKind::EventEntity,
                TimeFrameIndex{static_cast<int64_t>(i)},
                static_cast<int>(i));
        new_ids.push_back(id);
    }

    owning->setEntityIds(std::move(new_ids));
    _cacheOptimizationPointers();
}

// ========== View Factory Methods ==========

std::shared_ptr<DigitalEventSeries> DigitalEventSeries::createView(
        const std::shared_ptr<DigitalEventSeries const>& source,
        TimeFrameIndex start,
        TimeFrameIndex end) {
    // Get shared owning storage from source (zero-copy)
    auto shared_storage = source->_storage.getSharedOwningStorage();
    if (!shared_storage) {
        // Source is lazy storage - materialize first
        auto materialized = source->materialize();
        return createView(materialized, start, end);
    }

    // Create view storage referencing the shared source (no copy!)
    auto view_storage = ViewDigitalEventStorage{shared_storage};
    view_storage.filterByTimeRange(start, end);

    auto result = std::make_shared<DigitalEventSeries>();
    result->_storage = DigitalEventStorageWrapper{std::move(view_storage)};
    result->_time_frame = source->_time_frame;
    result->_cacheOptimizationPointers();

    return result;
}

std::shared_ptr<DigitalEventSeries> DigitalEventSeries::createView(
        const std::shared_ptr<DigitalEventSeries const>& source,
        std::unordered_set<EntityId> const & entity_ids) {
    // Get shared owning storage from source (zero-copy)
    auto shared_storage = source->_storage.getSharedOwningStorage();
    if (!shared_storage) {
        // Source is lazy storage - materialize first
        auto materialized = source->materialize();
        return createView(materialized, entity_ids);
    }

    // Create view storage referencing the shared source (no copy!)
    auto view_storage = ViewDigitalEventStorage{shared_storage};
    view_storage.filterByEntityIds(entity_ids);

    auto result = std::make_shared<DigitalEventSeries>();
    result->_storage = DigitalEventStorageWrapper{std::move(view_storage)};
    result->_time_frame = source->_time_frame;
    result->_cacheOptimizationPointers();

    return result;
}

std::shared_ptr<DigitalEventSeries> DigitalEventSeries::materialize() const {
    std::vector<TimeFrameIndex> events;
    std::vector<EntityId> entity_ids;

    events.reserve(_storage.size());
    entity_ids.reserve(_storage.size());

    for (size_t i = 0; i < _storage.size(); ++i) {
        events.push_back(_storage.getEvent(i));
        entity_ids.push_back(_storage.getEntityId(i));
    }

    auto result = std::make_shared<DigitalEventSeries>();
    result->_storage = DigitalEventStorageWrapper{
            OwningDigitalEventStorage{std::move(events), std::move(entity_ids)}};
    result->_time_frame = _time_frame;
    result->_cacheOptimizationPointers();

    return result;
}
