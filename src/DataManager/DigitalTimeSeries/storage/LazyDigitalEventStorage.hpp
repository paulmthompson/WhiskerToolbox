#ifndef LAZY_DIGITAL_EVENT_STORAGE_HPP
#define LAZY_DIGITAL_EVENT_STORAGE_HPP

#include "DigitalEventStorageBase.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <vector>
#include <optional>
#include <utility>
#include <functional>

/**
 * @brief Lazy digital event storage that computes events on-demand from a view
 *
 * Stores a computation pipeline as a random-access view that transforms data
 * on-demand. Enables efficient composition of transforms without materializing
 * intermediate results.
 *
 * The view must yield objects with .event_time and .entity_id members
 * (or first/second for pair-like types).
 *
 * @tparam ViewType Type of the random-access range view
 */
template<typename ViewType>
class LazyDigitalEventStorage : public DigitalEventStorageBase<LazyDigitalEventStorage<ViewType>> {
public:
    /**
     * @brief Construct lazy storage from a random-access view
     *
     * @param view Random-access range view yielding event-like objects
     * @param num_elements Number of elements in the view
     */
    explicit LazyDigitalEventStorage(ViewType view, size_t num_elements)
        : _view(std::move(view))
        , _num_elements(num_elements) {
        static_assert(std::ranges::random_access_range<ViewType>,
                      "LazyDigitalEventStorage requires random access range");
        _buildLocalIndices();
    }

    virtual ~LazyDigitalEventStorage() = default;

    // ========== CRTP Implementation ==========

    [[nodiscard]] size_t sizeImpl() const { return _num_elements; }

    [[nodiscard]] TimeFrameIndex getEventImpl(size_t idx) const {
        auto const& element = _view[idx];
        if constexpr (requires { element.event_time; }) {
            return element.event_time;
        } else if constexpr (requires { element.first; }) {
            return element.first;
        } else {
            return std::get<0>(element);
        }
    }

    [[nodiscard]] EntityId getEntityIdImpl(size_t idx) const {
        auto const& element = _view[idx];
        if constexpr (requires { element.entity_id; }) {
            return element.entity_id;
        } else if constexpr (requires { element.second; }) {
            return element.second;
        } else {
            return std::get<1>(element);
        }
    }

    [[nodiscard]] std::optional<size_t> findByTimeImpl(TimeFrameIndex time) const {
        auto it = _time_to_index.find(time);
        return it != _time_to_index.end() ? std::optional{it->second} : std::nullopt;
    }

    [[nodiscard]] std::optional<size_t> findByEntityIdImpl(EntityId id) const {
        auto it = _entity_id_to_index.find(id);
        return it != _entity_id_to_index.end() ? std::optional{it->second} : std::nullopt;
    }

    [[nodiscard]] std::pair<size_t, size_t> getTimeRangeImpl(TimeFrameIndex start, TimeFrameIndex end) const {
        // Linear scan for lazy storage (could be optimized if sorted)
        size_t start_idx = _num_elements;
        size_t end_idx = 0;

        for (size_t i = 0; i < _num_elements; ++i) {
            TimeFrameIndex const t = getEventImpl(i);
            if (t >= start && t <= end) {
                start_idx = std::min(start_idx, i);
                end_idx = std::max(end_idx, i + 1);
            }
        }

        return start_idx <= end_idx ? std::pair{start_idx, end_idx} : std::pair<size_t, size_t>{0, 0};
    }

    [[nodiscard]] DigitalEventStorageType getStorageTypeImpl() const {
        return DigitalEventStorageType::Lazy;
    }

    /**
     * @brief Lazy storage is never contiguous in memory
     *
     * Returns an invalid cache, forcing callers to use virtual dispatch.
     */
    [[nodiscard]] DigitalEventStorageCache tryGetCacheImpl() const {
        return DigitalEventStorageCache{};  // Invalid cache
    }

    /**
     * @brief Get reference to underlying view
     */
    [[nodiscard]] ViewType const& getView() const {
        return _view;
    }

private:
    /**
     * @brief Build local indices on construction
     */
    void _buildLocalIndices() {
        _time_to_index.clear();
        _entity_id_to_index.clear();

        for (size_t i = 0; i < _num_elements; ++i) {
            TimeFrameIndex const time = getEventImpl(i);
            EntityId const id = getEntityIdImpl(i);

            // Only store first occurrence for time (events should be unique per time)
            if (!_time_to_index.contains(time)) {
                _time_to_index[time] = i;
            }

            _entity_id_to_index[id] = i;
        }
    }

    ViewType _view;
    size_t _num_elements;
    std::unordered_map<TimeFrameIndex, size_t> _time_to_index;
    std::unordered_map<EntityId, size_t> _entity_id_to_index;
};

#endif // LAZY_DIGITAL_EVENT_STORAGE_HPP
