#include "OwningDigitalEventStorage.hpp"

#include <algorithm>
#include <stdexcept>
#include <numeric>

bool OwningDigitalEventStorage::addEvent(TimeFrameIndex time, EntityId entity_id) {
        // Check for duplicate
        auto it = std::ranges::lower_bound(_events, time);
        if (it != _events.end() && *it == time) {
            return false;  // Duplicate
        }

        // Insert at sorted position
        auto const idx = static_cast<size_t>(std::distance(_events.begin(), it));
        _events.insert(it, time);
        _entity_ids.insert(_entity_ids.begin() + static_cast<std::ptrdiff_t>(idx), entity_id);

        // Update index
        _entity_id_to_index[entity_id] = idx;
        // Update indices for moved elements
        for (size_t i = idx + 1; i < _entity_ids.size(); ++i) {
            _entity_id_to_index[_entity_ids[i]] = i;
        }

        return true;
    }

bool OwningDigitalEventStorage::removeEvent(TimeFrameIndex time) {
        auto it = std::ranges::lower_bound(_events, time);
        if (it == _events.end() || *it != time) {
            return false;
        }

        auto const idx = static_cast<size_t>(std::distance(_events.begin(), it));

        // Remove from entity ID index
        if (idx < _entity_ids.size()) {
            _entity_id_to_index.erase(_entity_ids[idx]);
        }

        _events.erase(it);
        _entity_ids.erase(_entity_ids.begin() + static_cast<std::ptrdiff_t>(idx));

        // Update indices for moved elements
        for (size_t i = idx; i < _entity_ids.size(); ++i) {
            _entity_id_to_index[_entity_ids[i]] = i;
        }

        return true;
    }

bool OwningDigitalEventStorage::removeByEntityId(EntityId id) {
        auto it = _entity_id_to_index.find(id);
        if (it == _entity_id_to_index.end()) {
            return false;
        }

        size_t const idx = it->second;
        _entity_id_to_index.erase(it);

        _events.erase(_events.begin() + static_cast<std::ptrdiff_t>(idx));
        _entity_ids.erase(_entity_ids.begin() + static_cast<std::ptrdiff_t>(idx));

        // Update indices for moved elements
        for (size_t i = idx; i < _entity_ids.size(); ++i) {
            _entity_id_to_index[_entity_ids[i]] = i;
        }

        return true;
    }

void OwningDigitalEventStorage::reserve(size_t capacity) {
        _events.reserve(capacity);
        _entity_ids.reserve(capacity);
    }

void OwningDigitalEventStorage::clear() {
        _events.clear();
        _entity_ids.clear();
        _entity_id_to_index.clear();
    }

void OwningDigitalEventStorage::setEntityIds(std::vector<EntityId> ids) {
        if (ids.size() != _events.size()) {
            throw std::invalid_argument("EntityId count must match event count");
        }
        _entity_ids = std::move(ids);
        _rebuildEntityIdIndex();
    }



    // ========== CRTP Implementation ==========


void OwningDigitalEventStorage::_sortEvents() {
        std::ranges::sort(_events);
    }

void OwningDigitalEventStorage::_sortEventsWithEntityIds() {
        // Create index array and sort by event time
        std::vector<size_t> indices(_events.size());
        std::iota(indices.begin(), indices.end(), 0);

        std::ranges::sort(indices, [this](size_t a, size_t b) {
            return _events[a] < _events[b];
        });

        // Apply permutation to both arrays
        std::vector<TimeFrameIndex> sorted_events;
        sorted_events.reserve(_events.size());
        std::vector<EntityId> sorted_ids;
        sorted_ids.reserve(_entity_ids.size());

        for (size_t i = 0; i < indices.size(); ++i) {
            sorted_events.push_back(_events[indices[i]]);
            sorted_ids.push_back(_entity_ids[indices[i]]);
        }

        _events = std::move(sorted_events);
        _entity_ids = std::move(sorted_ids);
    }

void OwningDigitalEventStorage::_rebuildEntityIdIndex() {
        _entity_id_to_index.clear();
        for (size_t i = 0; i < _entity_ids.size(); ++i) {
            _entity_id_to_index[_entity_ids[i]] = i;
        }
    }
