#include "Digital_Event_Series.hpp"

#include "Entity/EntityRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>// std::sort

DigitalEventSeries::DigitalEventSeries()
    : _storage()
    , _cached_storage()
{
    _cacheOptimizationPointers();
}

DigitalEventSeries::DigitalEventSeries(std::vector<TimeFrameIndex> event_vector) {
    // Use owning storage with the provided events
    _storage = DigitalEventStorageWrapper{
        OwningDigitalEventStorage{std::move(event_vector)}
    };
    _cacheOptimizationPointers();
}

std::vector<TimeFrameIndex> const & DigitalEventSeries::getEventSeries() const {
    // Build legacy vector on demand
    if (!_legacy_vector_valid) {
        _legacy_event_vector.clear();
        _legacy_event_vector.reserve(_storage.size());
        
        for (size_t i = 0; i < _storage.size(); ++i) {
            _legacy_event_vector.push_back(_storage.getEvent(i));
        }
        _legacy_vector_valid = true;
    }
    return _legacy_event_vector;
}

void DigitalEventSeries::addEvent(TimeFrameIndex const event_time) {
    // Check if storage is mutable (owning)
    auto* owning = _storage.tryGetMutableOwning();
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
            static_cast<int>(local_idx)
        );
    }
    
    bool const added = owning->addEvent(event_time, entity_id);
    
    if (added) {
        _legacy_vector_valid = false;
        _legacy_entity_id_valid = false;
        _cacheOptimizationPointers();
        notifyObservers();
    }
}

bool DigitalEventSeries::removeEvent(TimeFrameIndex const event_time) {
    auto* owning = _storage.tryGetMutableOwning();
    if (!owning) {
        throw std::runtime_error("Cannot remove events from view or lazy storage");
    }
    
    bool const removed = owning->removeEvent(event_time);
    
    if (removed) {
        _legacy_vector_valid = false;
        _legacy_entity_id_valid = false;
        _cacheOptimizationPointers();
        notifyObservers();
    }
    
    return removed;
}

void DigitalEventSeries::clear() {
    auto* owning = _storage.tryGetMutableOwning();
    if (!owning) {
        throw std::runtime_error("Cannot clear view or lazy storage");
    }
    
    owning->clear();
    _legacy_vector_valid = false;
    _legacy_entity_id_valid = false;
    _cacheOptimizationPointers();
    notifyObservers();
}

void DigitalEventSeries::_cacheOptimizationPointers() {
    _cached_storage = _storage.tryGetCache();
}

void DigitalEventSeries::rebuildAllEntityIds() {
    auto* owning = _storage.tryGetMutableOwning();
    if (!owning) {
        // For view/lazy storage, just invalidate the cache
        _legacy_entity_id_valid = false;
        return;
    }
    
    if (!_identity_registry) {
        // Clear all entity IDs to zero
        std::vector<EntityId> zero_ids(owning->size(), EntityId{0});
        owning->setEntityIds(std::move(zero_ids));
        _legacy_entity_id_valid = false;
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
            static_cast<int>(i)
        );
        new_ids.push_back(id);
    }
    
    owning->setEntityIds(std::move(new_ids));
    _legacy_entity_id_valid = false;
    _cacheOptimizationPointers();
}

std::vector<EntityId> const& DigitalEventSeries::getEntityIds() const {
    // Build legacy entity ID vector on demand
    if (!_legacy_entity_id_valid) {
        _legacy_entity_id_vector.clear();
        _legacy_entity_id_vector.reserve(_storage.size());
        
        for (size_t i = 0; i < _storage.size(); ++i) {
            _legacy_entity_id_vector.push_back(_storage.getEntityId(i));
        }
        _legacy_entity_id_valid = true;
    }
    return _legacy_entity_id_vector;
}

// ========== Events with EntityIDs ==========

std::vector<EventWithId> DigitalEventSeries::getEventsWithIdsInRange(TimeFrameIndex start_time, TimeFrameIndex stop_time) const {
    auto [start_idx, end_idx] = _storage.getTimeRange(start_time, stop_time);
    
    std::vector<EventWithId> result;
    if (end_idx > start_idx) {
        result.reserve(end_idx - start_idx);
        
        for (size_t i = start_idx; i < end_idx; ++i) {
            result.emplace_back(_storage.getEvent(i), _storage.getEntityId(i));
        }
    }
    return result;
}

std::vector<EventWithId> DigitalEventSeries::getEventsWithIdsInRange(TimeFrameIndex start_index,
                                                                     TimeFrameIndex stop_index,
                                                                     TimeFrame const & source_time_frame) const {
    if (&source_time_frame == _time_frame.get()) {
        return getEventsWithIdsInRange(start_index, stop_index);
    }

    // If either timeframe is null, fall back to original behavior
    if (!_time_frame.get()) {
        return getEventsWithIdsInRange(start_index, stop_index);
    }

    auto [target_start_index, target_stop_index] = convertTimeFrameRange(start_index, stop_index, source_time_frame, *_time_frame);
    return getEventsWithIdsInRange(target_start_index, target_stop_index);
}

// ========== View Factory Methods ==========

std::shared_ptr<DigitalEventSeries> DigitalEventSeries::createView(
    std::shared_ptr<DigitalEventSeries const> source,
    TimeFrameIndex start,
    TimeFrameIndex end)
{
    // Get source's owning storage
    auto const* src_owning = source->_storage.tryGetOwning();
    if (!src_owning) {
        // Source is view or lazy - materialize first
        auto materialized = source->materialize();
        return createView(materialized, start, end);
    }
    
    // Create view storage
    auto view_storage = ViewDigitalEventStorage{
        std::make_shared<OwningDigitalEventStorage const>(*src_owning)
    };
    view_storage.filterByTimeRange(start, end);
    
    auto result = std::make_shared<DigitalEventSeries>();
    result->_storage = DigitalEventStorageWrapper{std::move(view_storage)};
    result->_time_frame = source->_time_frame;
    result->_cacheOptimizationPointers();
    
    return result;
}

std::shared_ptr<DigitalEventSeries> DigitalEventSeries::createView(
    std::shared_ptr<DigitalEventSeries const> source,
    std::unordered_set<EntityId> const& entity_ids)
{
    // Get source's owning storage
    auto const* src_owning = source->_storage.tryGetOwning();
    if (!src_owning) {
        // Source is view or lazy - materialize first
        auto materialized = source->materialize();
        return createView(materialized, entity_ids);
    }
    
    // Create view storage
    auto view_storage = ViewDigitalEventStorage{
        std::make_shared<OwningDigitalEventStorage const>(*src_owning)
    };
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
        OwningDigitalEventStorage{std::move(events), std::move(entity_ids)}
    };
    result->_time_frame = _time_frame;
    result->_cacheOptimizationPointers();
    
    return result;
}
