#ifndef LAZY_DIGITAL_INTERVAL_STORAGE_HPP
#define LAZY_DIGITAL_INTERVAL_STORAGE_HPP

#include "DigitalIntervalStorageBase.hpp"
#include "DigitalIntervalStorageCache.hpp"

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <algorithm>
#include <numeric>
#include <ranges>

// =============================================================================
// Lazy Storage (View-based Computation on Demand)
// =============================================================================

/**
 * @brief Lazy digital interval storage that computes intervals on-demand from a view
 * 
 * Stores a computation pipeline as a random-access view that transforms data
 * on-demand. Enables efficient composition of transforms without materializing
 * intermediate results.
 * 
 * The view must yield objects with .interval and .entity_id members
 * (or convertible to Interval/EntityId pair).
 * 
 * @tparam ViewType Type of the random-access range view
 */
template<typename ViewType>
class LazyDigitalIntervalStorage : public DigitalIntervalStorageBase<LazyDigitalIntervalStorage<ViewType>> {
public:
    /**
     * @brief Construct lazy storage from a random-access view
     * 
     * @param view Random-access range view yielding interval-like objects
     * @param num_elements Number of elements in the view
     */
    explicit LazyDigitalIntervalStorage(ViewType view, size_t num_elements)
        : _view(std::move(view)),
          _num_elements(num_elements) {
        static_assert(std::ranges::random_access_range<ViewType>,
                      "LazyDigitalIntervalStorage requires random access range");
        _buildLocalIndices();
    }

    virtual ~LazyDigitalIntervalStorage() = default;

    // ========== CRTP Implementation ==========

    [[nodiscard]] size_t sizeImpl() const { return _num_elements; }

    [[nodiscard]] Interval const & getIntervalImpl(size_t idx) const {
        auto const & element = _view[idx];
        if constexpr (requires { element.interval; }) {
            _cached_interval = element.interval;
        } else if constexpr (requires { element.first; }) {
            _cached_interval = element.first;
        } else {
            _cached_interval = std::get<0>(element);
        }
        return _cached_interval;
    }

    [[nodiscard]] EntityId getEntityIdImpl(size_t idx) const {
        auto const & element = _view[idx];
        if constexpr (requires { element.entity_id; }) {
            return element.entity_id;
        } else if constexpr (requires { element.second; }) {
            return element.second;
        } else {
            return std::get<1>(element);
        }
    }

    [[nodiscard]] std::optional<size_t> findByIntervalImpl(Interval const & interval) const {
        // Linear search
        for (size_t i = 0; i < _num_elements; ++i) {
            Interval const & iv = getIntervalImpl(i);
            if (iv.start == interval.start && iv.end == interval.end) {
                return i;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<size_t> findByEntityIdImpl(EntityId id) const {
        auto it = _entity_id_to_index.find(id);
        return it != _entity_id_to_index.end() ? std::optional{it->second} : std::nullopt;
    }

    [[nodiscard]] bool hasIntervalAtTimeImpl(int64_t time) const {
        for (size_t i = 0; i < _num_elements; ++i) {
            Interval const & interval = getIntervalImpl(i);
            if (interval.start <= time && time <= interval.end) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] std::pair<size_t, size_t> getOverlappingRangeImpl(int64_t start, int64_t end) const {
        if (_num_elements == 0 || start > end) {
            return {0, 0};
        }

        // Lazy storage also maintains sorted disjoint intervals, so we can use binary search.
        // Use std::views::iota to create an index range for binary searching.
        auto indices = std::views::iota(size_t{0}, _num_elements);

        // Find first index where interval ends at or after start
        auto it_start = std::ranges::lower_bound(indices, start, {},
                                                 [this](size_t idx) { return getIntervalImpl(idx).end; });

        // Find first index where interval starts strictly after end
        auto it_end = std::ranges::upper_bound(indices, end, {},
                                               [this](size_t idx) { return getIntervalImpl(idx).start; });

        size_t start_idx = *it_start;
        size_t end_idx = *it_end;

        // Handle edge cases where iterators are at end
        if (it_start == indices.end()) {
            return {0, 0};
        }
        if (it_end == indices.end()) {
            end_idx = _num_elements;
        }

        return {start_idx, end_idx};
    }

    [[nodiscard]] std::pair<size_t, size_t> getContainedRangeImpl(int64_t start, int64_t end) const {
        if (_num_elements == 0 || start > end) {
            return {0, 0};
        }

        // Linear scan for lazy storage
        size_t start_idx = _num_elements;
        size_t end_idx = 0;

        for (size_t i = 0; i < _num_elements; ++i) {
            Interval const & interval = getIntervalImpl(i);
            if (interval.start >= start && interval.end <= end) {
                start_idx = std::min(start_idx, i);
                end_idx = std::max(end_idx, i + 1);
            }
        }

        return start_idx <= end_idx ? std::pair{start_idx, end_idx} : std::pair<size_t, size_t>{0, 0};
    }

    [[nodiscard]] DigitalIntervalStorageType getStorageTypeImpl() const {
        return DigitalIntervalStorageType::Lazy;
    }

    /**
     * @brief Lazy storage is never contiguous in memory
     * 
     * Returns an invalid cache, forcing callers to use virtual dispatch.
     */
    [[nodiscard]] DigitalIntervalStorageCache tryGetCacheImpl() const {
        return DigitalIntervalStorageCache{};// Invalid cache
    }

    /**
     * @brief Get reference to underlying view
     */
    [[nodiscard]] ViewType const & getView() const {
        return _view;
    }

private:
    /**
     * @brief Build local indices on construction
     */
    void _buildLocalIndices() {
        _entity_id_to_index.clear();

        for (size_t i = 0; i < _num_elements; ++i) {
            EntityId const id = getEntityIdImpl(i);
            _entity_id_to_index[id] = i;
        }
    }

    ViewType _view;
    size_t _num_elements;
    std::unordered_map<EntityId, size_t> _entity_id_to_index;
    mutable Interval _cached_interval;// For returning reference from lazy access
};

#endif// LAZY_DIGITAL_INTERVAL_STORAGE_HPP