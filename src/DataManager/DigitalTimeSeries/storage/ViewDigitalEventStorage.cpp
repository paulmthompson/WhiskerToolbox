#include "ViewDigitalEventStorage.hpp"

#include <algorithm>
#include <stdexcept>
#include <numeric>

TimeFrameIndex ViewDigitalEventStorage::getEventImpl(size_t idx) const {
        return _source->getEvent(_indices[idx]);
    }

EntityId ViewDigitalEventStorage::getEntityIdImpl(size_t idx) const {
        return _source->getEntityId(_indices[idx]);
    }

std::optional<size_t> ViewDigitalEventStorage::findByEntityIdImpl(EntityId id) const {
        auto it = _local_entity_id_to_index.find(id);
        return it != _local_entity_id_to_index.end() ? std::optional{it->second} : std::nullopt;
    }

DigitalEventStorageCache ViewDigitalEventStorage::tryGetCacheImpl() const {
        if (_indices.empty()) {
            return DigitalEventStorageCache{nullptr, nullptr, 0, true};
        }

        // Check if indices form a contiguous range
        size_t const start_idx = _indices[0];
        bool is_contiguous = true;

        for (size_t i = 1; i < _indices.size(); ++i) {
            if (_indices[i] != start_idx + i) {
                is_contiguous = false;
                break;
            }
        }

        if (is_contiguous) {
            return DigitalEventStorageCache{
                _source->events().data() + start_idx,
                _source->entityIds().data() + start_idx,
                _indices.size(),
                true
            };
        }

        return DigitalEventStorageCache{};  // Invalid
    }

std::optional<size_t> ViewDigitalEventStorage::findByTimeImpl(TimeFrameIndex time) const {
        // Binary search since events are sorted
        auto it = std::ranges::lower_bound(_indices, time, {}, [this](size_t idx) {
            return _source->getEvent(idx);
        });

        if (it != _indices.end() && _source->getEvent(*it) == time) {
            return static_cast<size_t>(std::distance(_indices.begin(), it));
        }
        return std::nullopt;
    }

std::pair<size_t, size_t> ViewDigitalEventStorage::getTimeRangeImpl(TimeFrameIndex start, TimeFrameIndex end) const {
        auto it_start = std::ranges::lower_bound(_indices, start, {}, [this](size_t idx) {
            return _source->getEvent(idx);
        });
        auto it_end = std::ranges::upper_bound(_indices, end, {}, [this](size_t idx) {
            return _source->getEvent(idx);
        });

        return {
            static_cast<size_t>(std::distance(_indices.begin(), it_start)),
            static_cast<size_t>(std::distance(_indices.begin(), it_end))
        };
    }
