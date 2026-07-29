#ifndef LAZY_DIGITAL_INTERVAL_STORAGE_HPP
#define LAZY_DIGITAL_INTERVAL_STORAGE_HPP

#include "DigitalIntervalStorageBase.hpp"
#include "DigitalIntervalStorageCache.hpp"

#include "Entity/EntityTypes.hpp"     // EntityId with hash specialization
#include "TimeFrame/interval_data.hpp"// Interval struct

#include <algorithm>    // std::ranges::lower_bound, std::ranges::upper_bound, std::min, std::max
#include <optional>     // std::optional
#include <ranges>       // std::ranges::random_access_range, std::ranges::views::iota
#include <unordered_map>// std::unordered_map

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
 * ## Range-query limitation
 *
 * @ref getOverlappingRangeImpl() always uses an O(n) linear scan. Unlike
 * @ref OwningDigitalIntervalStorage and @ref ViewDigitalIntervalStorage, lazy storage
 * does not expose `assumeDisjointIntervals()` and therefore does not use the O(log n)
 * binary-search fast path, even when the underlying view yields disjoint intervals.
 * This is intentional for transform intermediates (`IntervalLayout::Overlapping`) where
 * overlaps are possible; a disjoint fast path may be added later if needed.
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

    [[nodiscard]] TimeFrameInterval const & getIntervalImpl(size_t idx) const {
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

    [[nodiscard]] std::optional<size_t> findByIntervalImpl(TimeFrameInterval const & interval) const {
        // Linear search
        for (size_t i = 0; i < _num_elements; ++i) {
            TimeFrameInterval const & iv = getIntervalImpl(i);
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

    [[nodiscard]] bool hasIntervalAtTimeImpl(TimeFrameIndex time) const {
        for (size_t i = 0; i < _num_elements; ++i) {
            TimeFrameInterval const & interval = getIntervalImpl(i);
            if (interval.start <= time && time <= interval.end) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Get index range of intervals overlapping [start, end].
     *
     * **Always O(n) linear scan** over computed elements. Correct for overlapping
     * intervals. Does not use the disjoint binary-search optimization available on
     * @ref OwningDigitalIntervalStorage::getOverlappingRangeImpl() and
     * @ref ViewDigitalIntervalStorage::getOverlappingRangeImpl().
     *
     * @note Lazy storage has no `assumeDisjointIntervals()` hook; disjoint transform
     *       views still pay linear cost for this query.
     *
     * @see OwningDigitalIntervalStorage::getOverlappingRangeImpl()
     * @see ViewDigitalIntervalStorage::getOverlappingRangeImpl()
     */
    [[nodiscard]] std::pair<size_t, size_t> getOverlappingRangeImpl(TimeFrameIndex start, TimeFrameIndex end) const {
        if (_num_elements == 0 || start > end) {
            return {0, 0};
        }

        // Linear scan: lazy storage backs transform intermediates that may overlap.
        size_t start_idx = _num_elements;
        size_t end_idx = 0;

        for (size_t i = 0; i < _num_elements; ++i) {
            TimeFrameInterval const & interval = getIntervalImpl(i);
            if (interval.start <= end && interval.end >= start) {
                start_idx = std::min(start_idx, i);
                end_idx = std::max(end_idx, i + 1);
            }
        }

        return start_idx <= end_idx ? std::pair{start_idx, end_idx} : std::pair<size_t, size_t>{0, 0};
    }

    [[nodiscard]] std::pair<size_t, size_t> getContainedRangeImpl(TimeFrameIndex start, TimeFrameIndex end) const {
        if (_num_elements == 0 || start > end) {
            return {0, 0};
        }

        // Linear scan for lazy storage
        size_t start_idx = _num_elements;
        size_t end_idx = 0;

        for (size_t i = 0; i < _num_elements; ++i) {
            TimeFrameInterval const & interval = getIntervalImpl(i);
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
    mutable TimeFrameInterval _cached_interval{TimeFrameIndex{0}, TimeFrameIndex{0}};

};

#endif// LAZY_DIGITAL_INTERVAL_STORAGE_HPP