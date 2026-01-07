#ifndef DIGITAL_INTERVAL_STORAGE_HPP
#define DIGITAL_INTERVAL_STORAGE_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>
#include <unordered_map>
#include <unordered_set>

/**
 * @brief Hash specialization for Interval to allow use in unordered containers
 */
template<>
struct std::hash<Interval> {
    size_t operator()(Interval const& interval) const noexcept {
        // Combine start and end using a simple hash combination
        size_t h1 = std::hash<int64_t>{}(interval.start);
        size_t h2 = std::hash<int64_t>{}(interval.end);
        return h1 ^ (h2 << 1);
    }
};

// Forward declarations
class OwningDigitalIntervalStorage;
class ViewDigitalIntervalStorage;

/**
 * @brief Storage type enumeration for digital interval storage
 */
enum class DigitalIntervalStorageType {
    Owning,  ///< Owns the data in SoA layout
    View,    ///< References another storage via indices
    Lazy     ///< Lazy-evaluated transform
};

// =============================================================================
// Cache Optimization Structure
// =============================================================================

/**
 * @brief Cache structure for fast-path access to contiguous digital interval storage
 * 
 * Digital intervals store Interval values (start/end times) with associated
 * EntityIds. The storage is organized as parallel arrays:
 * - intervals[i] - Interval for entry i
 * - entity_ids[i] - EntityId for entry i
 * 
 * @note Digital intervals are always sorted by start time.
 */
struct DigitalIntervalStorageCache {
    Interval const* intervals_ptr = nullptr;
    EntityId const* entity_ids_ptr = nullptr;
    size_t cache_size = 0;
    bool is_contiguous = false;  ///< True if storage is contiguous (owning)
    
    /**
     * @brief Check if the cache is valid for fast-path access
     * 
     * @return true if storage is contiguous (can use direct pointer access)
     * @return false if storage is non-contiguous (must use virtual dispatch)
     */
    [[nodiscard]] constexpr bool isValid() const noexcept {
        return is_contiguous;
    }
    
    // Convenience accessors for cached data
    [[nodiscard]] Interval const& getInterval(size_t idx) const noexcept {
        return intervals_ptr[idx];
    }
    
    [[nodiscard]] EntityId getEntityId(size_t idx) const noexcept {
        return entity_ids_ptr[idx];
    }
};

// =============================================================================
// CRTP Base Class
// =============================================================================

/**
 * @brief CRTP base class for digital interval storage implementations
 * 
 * Uses Curiously Recurring Template Pattern to eliminate virtual function overhead
 * while maintaining a polymorphic interface. Intervals are stored as Interval
 * values (representing start/end times) with associated EntityIds.
 * 
 * The SoA (Structure of Arrays) layout stores parallel vectors:
 * - Interval intervals[] - sorted by start time
 * - EntityId entity_ids[] - corresponding entity identifiers
 * 
 * @tparam Derived The concrete storage implementation type
 */
template<typename Derived>
class DigitalIntervalStorageBase {
public:
    // ========== Size & Bounds ==========
    
    /**
     * @brief Get total number of intervals
     */
    [[nodiscard]] size_t size() const {
        return static_cast<Derived const*>(this)->sizeImpl();
    }
    
    /**
     * @brief Check if storage is empty
     */
    [[nodiscard]] bool empty() const {
        return size() == 0;
    }

    // ========== Element Access ==========
    
    /**
     * @brief Get the interval at a flat index
     * @param idx Flat index in [0, size())
     */
    [[nodiscard]] Interval const& getInterval(size_t idx) const {
        return static_cast<Derived const*>(this)->getIntervalImpl(idx);
    }
    
    /**
     * @brief Get the EntityId at a flat index
     * @param idx Flat index in [0, size())
     */
    [[nodiscard]] EntityId getEntityId(size_t idx) const {
        return static_cast<Derived const*>(this)->getEntityIdImpl(idx);
    }

    // ========== Lookup Operations ==========
    
    /**
     * @brief Find the index of an interval by its exact start/end times
     * @param interval The exact Interval to find
     * @return Index of the interval, or std::nullopt if not found
     */
    [[nodiscard]] std::optional<size_t> findByInterval(Interval const& interval) const {
        return static_cast<Derived const*>(this)->findByIntervalImpl(interval);
    }
    
    /**
     * @brief Find the index of an interval by its EntityId
     * @param id The EntityId to search for
     * @return Index of the interval, or std::nullopt if not found
     */
    [[nodiscard]] std::optional<size_t> findByEntityId(EntityId id) const {
        return static_cast<Derived const*>(this)->findByEntityIdImpl(id);
    }
    
    /**
     * @brief Check if any interval contains the specified time
     * @param time The time to check
     * @return true if any interval contains the time
     */
    [[nodiscard]] bool hasIntervalAtTime(int64_t time) const {
        return static_cast<Derived const*>(this)->hasIntervalAtTimeImpl(time);
    }
    
    /**
     * @brief Get range of indices for intervals overlapping [start, end]
     * @param start Start time (inclusive)
     * @param end End time (inclusive)
     * @return Pair of (start_idx, end_idx) where end is exclusive
     * 
     * @note Returns intervals where interval.start <= end && interval.end >= start
     */
    [[nodiscard]] std::pair<size_t, size_t> getOverlappingRange(int64_t start, int64_t end) const {
        return static_cast<Derived const*>(this)->getOverlappingRangeImpl(start, end);
    }
    
    /**
     * @brief Get range of indices for intervals fully contained in [start, end]
     * @param start Start time (inclusive)
     * @param end End time (inclusive)
     * @return Pair of (start_idx, end_idx) where end is exclusive
     * 
     * @note Returns intervals where interval.start >= start && interval.end <= end
     */
    [[nodiscard]] std::pair<size_t, size_t> getContainedRange(int64_t start, int64_t end) const {
        return static_cast<Derived const*>(this)->getContainedRangeImpl(start, end);
    }

    // ========== Storage Type ==========
    
    /**
     * @brief Get the storage type identifier
     */
    [[nodiscard]] DigitalIntervalStorageType getStorageType() const {
        return static_cast<Derived const*>(this)->getStorageTypeImpl();
    }
    
    /**
     * @brief Check if this is a view (doesn't own data)
     */
    [[nodiscard]] bool isView() const {
        return getStorageType() == DigitalIntervalStorageType::View;
    }
    
    /**
     * @brief Check if this is lazy storage
     */
    [[nodiscard]] bool isLazy() const {
        return getStorageType() == DigitalIntervalStorageType::Lazy;
    }

    // ========== Cache Optimization ==========
    
    /**
     * @brief Try to get cached pointers for fast-path access
     * 
     * @return DigitalIntervalStorageCache with valid pointers if contiguous, invalid otherwise
     */
    [[nodiscard]] DigitalIntervalStorageCache tryGetCache() const {
        return static_cast<Derived const*>(this)->tryGetCacheImpl();
    }

protected:
    ~DigitalIntervalStorageBase() = default;
};

// =============================================================================
// Owning Storage (SoA Layout)
// =============================================================================

/**
 * @brief Owning digital interval storage using Structure of Arrays layout
 * 
 * Stores interval data in parallel vectors for cache-friendly access:
 * - _intervals[i] - Interval for entry i (sorted by start time)
 * - _entity_ids[i] - EntityId for entry i
 * 
 * Maintains acceleration structures:
 * - Intervals are always sorted by start time
 * - O(log n) lookup by start time using binary search
 * - O(1) lookup by EntityId using hash map
 */
class OwningDigitalIntervalStorage : public DigitalIntervalStorageBase<OwningDigitalIntervalStorage> {
public:
    OwningDigitalIntervalStorage() = default;
    
    /**
     * @brief Construct from existing interval vector (will sort)
     */
    explicit OwningDigitalIntervalStorage(std::vector<Interval> intervals) {
        _intervals = std::move(intervals);
        _sortIntervals();
        // Entity IDs will be empty until rebuilt
        _entity_ids.resize(_intervals.size(), EntityId{0});
    }
    
    /**
     * @brief Construct from existing interval and entity ID vectors
     * @note Both vectors must be the same size
     */
    OwningDigitalIntervalStorage(std::vector<Interval> intervals, std::vector<EntityId> entity_ids) {
        if (intervals.size() != entity_ids.size()) {
            throw std::invalid_argument("Intervals and entity_ids must have same size");
        }
        _intervals = std::move(intervals);
        _entity_ids = std::move(entity_ids);
        _sortIntervalsWithEntityIds();
        _rebuildEntityIdIndex();
    }
    
    // ========== Modification ==========
    
    /**
     * @brief Add an interval
     * 
     * If an interval with the same start/end already exists, it is not added (returns false).
     * 
     * @param interval The interval to add
     * @param entity_id The EntityId for this interval
     * @return true if added, false if duplicate
     */
    bool addInterval(Interval const& interval, EntityId entity_id) {
        // Find insertion position by start time
        auto it = std::ranges::lower_bound(_intervals, interval.start, {}, 
            [](Interval const& i) { return i.start; });
        
        // Check for exact duplicate
        while (it != _intervals.end() && it->start == interval.start) {
            if (it->end == interval.end) {
                return false;  // Duplicate
            }
            ++it;
        }
        
        // Insert at sorted position
        it = std::ranges::lower_bound(_intervals, interval.start, {},
            [](Interval const& i) { return i.start; });
        auto const idx = static_cast<size_t>(std::distance(_intervals.begin(), it));
        _intervals.insert(it, interval);
        _entity_ids.insert(_entity_ids.begin() + static_cast<std::ptrdiff_t>(idx), entity_id);
        
        // Update index
        _entity_id_to_index[entity_id] = idx;
        // Update indices for moved elements
        for (size_t i = idx + 1; i < _entity_ids.size(); ++i) {
            _entity_id_to_index[_entity_ids[i]] = i;
        }
        
        return true;
    }
    
    /**
     * @brief Remove an interval by exact match
     * @return true if removed, false if not found
     */
    bool removeInterval(Interval const& interval) {
        auto opt_idx = findByIntervalImpl(interval);
        if (!opt_idx) {
            return false;
        }
        
        size_t const idx = *opt_idx;
        
        // Remove from entity ID index
        if (idx < _entity_ids.size()) {
            _entity_id_to_index.erase(_entity_ids[idx]);
        }
        
        _intervals.erase(_intervals.begin() + static_cast<std::ptrdiff_t>(idx));
        _entity_ids.erase(_entity_ids.begin() + static_cast<std::ptrdiff_t>(idx));
        
        // Update indices for moved elements
        for (size_t i = idx; i < _entity_ids.size(); ++i) {
            _entity_id_to_index[_entity_ids[i]] = i;
        }
        
        return true;
    }
    
    /**
     * @brief Remove an interval by EntityId
     * @return true if removed, false if not found
     */
    bool removeByEntityId(EntityId id) {
        auto it = _entity_id_to_index.find(id);
        if (it == _entity_id_to_index.end()) {
            return false;
        }
        
        size_t const idx = it->second;
        _entity_id_to_index.erase(it);
        
        _intervals.erase(_intervals.begin() + static_cast<std::ptrdiff_t>(idx));
        _entity_ids.erase(_entity_ids.begin() + static_cast<std::ptrdiff_t>(idx));
        
        // Update indices for moved elements
        for (size_t i = idx; i < _entity_ids.size(); ++i) {
            _entity_id_to_index[_entity_ids[i]] = i;
        }
        
        return true;
    }
    
    /**
     * @brief Clear all intervals
     */
    void clear() {
        _intervals.clear();
        _entity_ids.clear();
        _entity_id_to_index.clear();
    }
    
    /**
     * @brief Reserve capacity for expected number of intervals
     */
    void reserve(size_t capacity) {
        _intervals.reserve(capacity);
        _entity_ids.reserve(capacity);
    }
    
    /**
     * @brief Set all entity IDs (must match interval count)
     */
    void setEntityIds(std::vector<EntityId> ids) {
        if (ids.size() != _intervals.size()) {
            throw std::invalid_argument("EntityId count must match interval count");
        }
        _entity_ids = std::move(ids);
        _rebuildEntityIdIndex();
    }

    // ========== CRTP Implementation ==========
    
    [[nodiscard]] size_t sizeImpl() const { return _intervals.size(); }
    
    [[nodiscard]] Interval const& getIntervalImpl(size_t idx) const { return _intervals[idx]; }
    
    [[nodiscard]] EntityId getEntityIdImpl(size_t idx) const {
        return idx < _entity_ids.size() ? _entity_ids[idx] : EntityId{0};
    }
    
    [[nodiscard]] std::optional<size_t> findByIntervalImpl(Interval const& interval) const {
        // Binary search by start time, then linear search for exact match
        auto it = std::ranges::lower_bound(_intervals, interval.start, {},
            [](Interval const& i) { return i.start; });
        
        while (it != _intervals.end() && it->start == interval.start) {
            if (it->end == interval.end) {
                return static_cast<size_t>(std::distance(_intervals.begin(), it));
            }
            ++it;
        }
        return std::nullopt;
    }
    
    [[nodiscard]] std::optional<size_t> findByEntityIdImpl(EntityId id) const {
        auto it = _entity_id_to_index.find(id);
        return it != _entity_id_to_index.end() ? std::optional{it->second} : std::nullopt;
    }
    
    [[nodiscard]] bool hasIntervalAtTimeImpl(int64_t time) const {
        // Binary search to find intervals that might contain this time
        // An interval contains time if interval.start <= time <= interval.end
        
        // Find first interval where start > time (all previous could contain time)
        auto it = std::ranges::upper_bound(_intervals, time, {},
            [](Interval const& i) { return i.start; });
        
        // Check intervals from beginning up to it
        for (auto check = _intervals.begin(); check != it; ++check) {
            if (check->end >= time) {
                return true;
            }
        }
        return false;
    }
    
    [[nodiscard]] std::pair<size_t, size_t> getOverlappingRangeImpl(int64_t start, int64_t end) const {
        if (_intervals.empty() || start > end) {
            return {0, 0};
        }
        
        // An interval overlaps [start, end] if interval.start <= end && interval.end >= start
        // Find first interval where start > end (these cannot overlap)
        auto it_end = std::ranges::upper_bound(_intervals, end, {},
            [](Interval const& i) { return i.start; });
        
        // Find first interval in [begin, it_end) where interval.end >= start
        size_t start_idx = 0;
        for (size_t i = 0; i < static_cast<size_t>(std::distance(_intervals.begin(), it_end)); ++i) {
            if (_intervals[i].end >= start) {
                start_idx = i;
                break;
            }
            if (i + 1 == static_cast<size_t>(std::distance(_intervals.begin(), it_end))) {
                // No overlapping intervals found
                return {0, 0};
            }
        }
        
        return {start_idx, static_cast<size_t>(std::distance(_intervals.begin(), it_end))};
    }
    
    [[nodiscard]] std::pair<size_t, size_t> getContainedRangeImpl(int64_t start, int64_t end) const {
        if (_intervals.empty() || start > end) {
            return {0, 0};
        }
        
        // An interval is contained in [start, end] if interval.start >= start && interval.end <= end
        // Find first interval where start >= start
        auto it_start = std::ranges::lower_bound(_intervals, start, {},
            [](Interval const& i) { return i.start; });
        
        if (it_start == _intervals.end()) {
            return {0, 0};
        }
        
        size_t start_idx = static_cast<size_t>(std::distance(_intervals.begin(), it_start));
        size_t end_idx = start_idx;
        
        // Find last interval where end <= end
        for (size_t i = start_idx; i < _intervals.size() && _intervals[i].start <= end; ++i) {
            if (_intervals[i].end <= end) {
                end_idx = i + 1;
            }
        }
        
        return {start_idx, end_idx};
    }
    
    [[nodiscard]] DigitalIntervalStorageType getStorageTypeImpl() const { 
        return DigitalIntervalStorageType::Owning; 
    }

    /**
     * @brief Get cache with pointers to contiguous data
     * 
     * OwningDigitalIntervalStorage stores data contiguously in SoA layout,
     * so it always returns a valid cache for fast-path iteration.
     */
    [[nodiscard]] DigitalIntervalStorageCache tryGetCacheImpl() const {
        return DigitalIntervalStorageCache{
            _intervals.data(),
            _entity_ids.data(),
            _intervals.size(),
            true  // is_contiguous
        };
    }

    // ========== Direct Array Access ==========
    
    [[nodiscard]] std::vector<Interval> const& intervals() const { return _intervals; }
    [[nodiscard]] std::vector<EntityId> const& entityIds() const { return _entity_ids; }
    
    [[nodiscard]] std::span<Interval const> intervalsSpan() const { return _intervals; }
    [[nodiscard]] std::span<EntityId const> entityIdsSpan() const { return _entity_ids; }

private:
    void _sortIntervals() {
        std::ranges::sort(_intervals, [](Interval const& a, Interval const& b) {
            return a.start < b.start;
        });
    }
    
    void _sortIntervalsWithEntityIds() {
        // Create index array and sort by interval start time
        std::vector<size_t> indices(_intervals.size());
        std::iota(indices.begin(), indices.end(), 0);
        
        std::ranges::sort(indices, [this](size_t a, size_t b) {
            return _intervals[a].start < _intervals[b].start;
        });
        
        // Apply permutation to both arrays
        std::vector<Interval> sorted_intervals;
        sorted_intervals.reserve(_intervals.size());
        std::vector<EntityId> sorted_ids;
        sorted_ids.reserve(_entity_ids.size());
        
        for (size_t i = 0; i < indices.size(); ++i) {
            sorted_intervals.push_back(_intervals[indices[i]]);
            sorted_ids.push_back(_entity_ids[indices[i]]);
        }
        
        _intervals = std::move(sorted_intervals);
        _entity_ids = std::move(sorted_ids);
    }
    
    void _rebuildEntityIdIndex() {
        _entity_id_to_index.clear();
        for (size_t i = 0; i < _entity_ids.size(); ++i) {
            _entity_id_to_index[_entity_ids[i]] = i;
        }
    }

    std::vector<Interval> _intervals;
    std::vector<EntityId> _entity_ids;
    std::unordered_map<EntityId, size_t> _entity_id_to_index;
};

// =============================================================================
// View Storage (References Source via Indices)
// =============================================================================

/**
 * @brief View-based digital interval storage that references another storage
 * 
 * Holds a shared_ptr to a source OwningDigitalIntervalStorage and a vector of indices
 * into that source. Enables zero-copy filtered views.
 */
class ViewDigitalIntervalStorage : public DigitalIntervalStorageBase<ViewDigitalIntervalStorage> {
public:
    /**
     * @brief Construct a view referencing source storage
     * 
     * @param source Shared pointer to source storage
     */
    explicit ViewDigitalIntervalStorage(std::shared_ptr<OwningDigitalIntervalStorage const> source)
        : _source(std::move(source)) {}
    
    /**
     * @brief Set the indices this view includes
     */
    void setIndices(std::vector<size_t> indices) {
        _indices = std::move(indices);
        _rebuildLocalIndices();
    }
    
    /**
     * @brief Create view of all intervals
     */
    void setAllIndices() {
        _indices.resize(_source->size());
        std::iota(_indices.begin(), _indices.end(), 0);
        _rebuildLocalIndices();
    }
    
    /**
     * @brief Filter by overlapping time range [start, end]
     */
    void filterByOverlappingRange(int64_t start, int64_t end) {
        auto [src_start, src_end] = _source->getOverlappingRange(start, end);
        
        _indices.clear();
        _indices.reserve(src_end - src_start);
        
        for (size_t i = src_start; i < src_end; ++i) {
            _indices.push_back(i);
        }
        
        _rebuildLocalIndices();
    }
    
    /**
     * @brief Filter by contained time range [start, end]
     */
    void filterByContainedRange(int64_t start, int64_t end) {
        auto [src_start, src_end] = _source->getContainedRange(start, end);
        
        _indices.clear();
        _indices.reserve(src_end - src_start);
        
        for (size_t i = src_start; i < src_end; ++i) {
            _indices.push_back(i);
        }
        
        _rebuildLocalIndices();
    }
    
    /**
     * @brief Filter by EntityId set
     */
    void filterByEntityIds(std::unordered_set<EntityId> const& ids) {
        _indices.clear();
        
        for (size_t i = 0; i < _source->size(); ++i) {
            if (ids.contains(_source->getEntityId(i))) {
                _indices.push_back(i);
            }
        }
        
        _rebuildLocalIndices();
    }
    
    /**
     * @brief Get the source storage
     */
    [[nodiscard]] std::shared_ptr<OwningDigitalIntervalStorage const> source() const {
        return _source;
    }
    
    /**
     * @brief Get the indices vector
     */
    [[nodiscard]] std::vector<size_t> const& indices() const {
        return _indices;
    }

    // ========== CRTP Implementation ==========
    
    [[nodiscard]] size_t sizeImpl() const { return _indices.size(); }
    
    [[nodiscard]] Interval const& getIntervalImpl(size_t idx) const {
        return _source->getInterval(_indices[idx]);
    }
    
    [[nodiscard]] EntityId getEntityIdImpl(size_t idx) const {
        return _source->getEntityId(_indices[idx]);
    }
    
    [[nodiscard]] std::optional<size_t> findByIntervalImpl(Interval const& interval) const {
        // Linear search through view indices
        for (size_t i = 0; i < _indices.size(); ++i) {
            Interval const& src_interval = _source->getInterval(_indices[i]);
            if (src_interval.start == interval.start && src_interval.end == interval.end) {
                return i;
            }
        }
        return std::nullopt;
    }
    
    [[nodiscard]] std::optional<size_t> findByEntityIdImpl(EntityId id) const {
        auto it = _local_entity_id_to_index.find(id);
        return it != _local_entity_id_to_index.end() ? std::optional{it->second} : std::nullopt;
    }
    
    [[nodiscard]] bool hasIntervalAtTimeImpl(int64_t time) const {
        for (size_t idx : _indices) {
            Interval const& interval = _source->getInterval(idx);
            if (interval.start <= time && time <= interval.end) {
                return true;
            }
        }
        return false;
    }
    
    [[nodiscard]] std::pair<size_t, size_t> getOverlappingRangeImpl(int64_t start, int64_t end) const {
        if (_indices.empty() || start > end) {
            return {0, 0};
        }
        
        // Linear scan for view (indices may not be contiguous)
        size_t start_idx = _indices.size();
        size_t end_idx = 0;
        
        for (size_t i = 0; i < _indices.size(); ++i) {
            Interval const& interval = _source->getInterval(_indices[i]);
            if (interval.start <= end && interval.end >= start) {
                start_idx = std::min(start_idx, i);
                end_idx = std::max(end_idx, i + 1);
            }
        }
        
        return start_idx <= end_idx ? std::pair{start_idx, end_idx} : std::pair<size_t, size_t>{0, 0};
    }
    
    [[nodiscard]] std::pair<size_t, size_t> getContainedRangeImpl(int64_t start, int64_t end) const {
        if (_indices.empty() || start > end) {
            return {0, 0};
        }
        
        // Linear scan for view
        size_t start_idx = _indices.size();
        size_t end_idx = 0;
        
        for (size_t i = 0; i < _indices.size(); ++i) {
            Interval const& interval = _source->getInterval(_indices[i]);
            if (interval.start >= start && interval.end <= end) {
                start_idx = std::min(start_idx, i);
                end_idx = std::max(end_idx, i + 1);
            }
        }
        
        return start_idx <= end_idx ? std::pair{start_idx, end_idx} : std::pair<size_t, size_t>{0, 0};
    }
    
    [[nodiscard]] DigitalIntervalStorageType getStorageTypeImpl() const { 
        return DigitalIntervalStorageType::View; 
    }

    /**
     * @brief Return cache if view is contiguous
     */
    [[nodiscard]] DigitalIntervalStorageCache tryGetCacheImpl() const {
        if (_indices.empty()) {
            return DigitalIntervalStorageCache{nullptr, nullptr, 0, true};
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
            return DigitalIntervalStorageCache{
                _source->intervals().data() + start_idx,
                _source->entityIds().data() + start_idx,
                _indices.size(),
                true
            };
        }
        
        return DigitalIntervalStorageCache{};  // Invalid
    }

private:
    void _rebuildLocalIndices() {
        _local_entity_id_to_index.clear();
        for (size_t i = 0; i < _indices.size(); ++i) {
            EntityId const id = _source->getEntityId(_indices[i]);
            _local_entity_id_to_index[id] = i;
        }
    }
    
    std::shared_ptr<OwningDigitalIntervalStorage const> _source;
    std::vector<size_t> _indices;
    std::unordered_map<EntityId, size_t> _local_entity_id_to_index;
};

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
        : _view(std::move(view))
        , _num_elements(num_elements) {
        static_assert(std::ranges::random_access_range<ViewType>,
                      "LazyDigitalIntervalStorage requires random access range");
        _buildLocalIndices();
    }

    virtual ~LazyDigitalIntervalStorage() = default;

    // ========== CRTP Implementation ==========

    [[nodiscard]] size_t sizeImpl() const { return _num_elements; }

    [[nodiscard]] Interval const& getIntervalImpl(size_t idx) const {
        auto const& element = _view[idx];
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
        auto const& element = _view[idx];
        if constexpr (requires { element.entity_id; }) {
            return element.entity_id;
        } else if constexpr (requires { element.second; }) {
            return element.second;
        } else {
            return std::get<1>(element);
        }
    }

    [[nodiscard]] std::optional<size_t> findByIntervalImpl(Interval const& interval) const {
        // Linear search
        for (size_t i = 0; i < _num_elements; ++i) {
            Interval const& iv = getIntervalImpl(i);
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
            Interval const& interval = getIntervalImpl(i);
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
        
        // Linear scan for lazy storage
        size_t start_idx = _num_elements;
        size_t end_idx = 0;
        
        for (size_t i = 0; i < _num_elements; ++i) {
            Interval const& interval = getIntervalImpl(i);
            if (interval.start <= end && interval.end >= start) {
                start_idx = std::min(start_idx, i);
                end_idx = std::max(end_idx, i + 1);
            }
        }
        
        return start_idx <= end_idx ? std::pair{start_idx, end_idx} : std::pair<size_t, size_t>{0, 0};
    }
    
    [[nodiscard]] std::pair<size_t, size_t> getContainedRangeImpl(int64_t start, int64_t end) const {
        if (_num_elements == 0 || start > end) {
            return {0, 0};
        }
        
        // Linear scan for lazy storage
        size_t start_idx = _num_elements;
        size_t end_idx = 0;
        
        for (size_t i = 0; i < _num_elements; ++i) {
            Interval const& interval = getIntervalImpl(i);
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
        return DigitalIntervalStorageCache{};  // Invalid cache
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
        _entity_id_to_index.clear();
        
        for (size_t i = 0; i < _num_elements; ++i) {
            EntityId const id = getEntityIdImpl(i);
            _entity_id_to_index[id] = i;
        }
    }

    ViewType _view;
    size_t _num_elements;
    std::unordered_map<EntityId, size_t> _entity_id_to_index;
    mutable Interval _cached_interval;  // For returning reference from lazy access
};

// =============================================================================
// Type-Erased Storage Wrapper (Virtual Dispatch)
// =============================================================================

/**
 * @brief Type-erased storage wrapper for digital interval storage
 * 
 * Provides a uniform interface for any storage backend while hiding the
 * concrete storage type. Supports lazy transforms with unbounded ViewType.
 */
class DigitalIntervalStorageWrapper {
public:
    /**
     * @brief Construct wrapper from any storage implementation
     */
    template<typename StorageImpl>
    explicit DigitalIntervalStorageWrapper(StorageImpl storage)
        : _impl(std::make_unique<StorageModel<StorageImpl>>(std::move(storage))) {}

    // Default constructor creates empty owning storage
    DigitalIntervalStorageWrapper()
        : _impl(std::make_unique<StorageModel<OwningDigitalIntervalStorage>>(
              OwningDigitalIntervalStorage{})) {}

    // Move-only semantics
    DigitalIntervalStorageWrapper(DigitalIntervalStorageWrapper&&) noexcept = default;
    DigitalIntervalStorageWrapper& operator=(DigitalIntervalStorageWrapper&&) noexcept = default;
    DigitalIntervalStorageWrapper(DigitalIntervalStorageWrapper const&) = delete;
    DigitalIntervalStorageWrapper& operator=(DigitalIntervalStorageWrapper const&) = delete;

    // ========== Unified Interface ==========
    
    [[nodiscard]] size_t size() const { return _impl->size(); }
    [[nodiscard]] bool empty() const { return _impl->size() == 0; }

    [[nodiscard]] Interval const& getInterval(size_t idx) const {
        return _impl->getInterval(idx);
    }

    [[nodiscard]] EntityId getEntityId(size_t idx) const {
        return _impl->getEntityId(idx);
    }

    [[nodiscard]] std::optional<size_t> findByInterval(Interval const& interval) const {
        return _impl->findByInterval(interval);
    }
    
    [[nodiscard]] std::optional<size_t> findByEntityId(EntityId id) const {
        return _impl->findByEntityId(id);
    }
    
    [[nodiscard]] bool hasIntervalAtTime(int64_t time) const {
        return _impl->hasIntervalAtTime(time);
    }

    [[nodiscard]] std::pair<size_t, size_t> getOverlappingRange(int64_t start, int64_t end) const {
        return _impl->getOverlappingRange(start, end);
    }
    
    [[nodiscard]] std::pair<size_t, size_t> getContainedRange(int64_t start, int64_t end) const {
        return _impl->getContainedRange(start, end);
    }

    [[nodiscard]] DigitalIntervalStorageType getStorageType() const {
        return _impl->getStorageType();
    }

    [[nodiscard]] bool isView() const {
        return getStorageType() == DigitalIntervalStorageType::View;
    }
    
    [[nodiscard]] bool isLazy() const {
        return getStorageType() == DigitalIntervalStorageType::Lazy;
    }

    // ========== Cache Optimization ==========
    
    [[nodiscard]] DigitalIntervalStorageCache tryGetCache() const {
        return _impl->tryGetCache();
    }

    // ========== Mutation Operations ==========
    
    bool addInterval(Interval const& interval, EntityId entity_id) {
        return _impl->addInterval(interval, entity_id);
    }

    bool removeInterval(Interval const& interval) {
        return _impl->removeInterval(interval);
    }
    
    bool removeByEntityId(EntityId id) {
        return _impl->removeByEntityId(id);
    }

    void reserve(size_t capacity) {
        _impl->reserve(capacity);
    }

    void clear() {
        _impl->clear();
    }
    
    void setEntityIds(std::vector<EntityId> ids) {
        _impl->setEntityIds(std::move(ids));
    }

    // ========== Type Access ==========
    
    template<typename StorageType>
    [[nodiscard]] StorageType* tryGet() {
        auto* model = dynamic_cast<StorageModel<StorageType>*>(_impl.get());
        return model ? &model->_storage : nullptr;
    }

    template<typename StorageType>
    [[nodiscard]] StorageType const* tryGet() const {
        auto const* model = dynamic_cast<StorageModel<StorageType> const*>(_impl.get());
        return model ? &model->_storage : nullptr;
    }
    
    /**
     * @brief Try to get mutable owning storage
     */
    [[nodiscard]] OwningDigitalIntervalStorage* tryGetMutableOwning() {
        return tryGet<OwningDigitalIntervalStorage>();
    }
    
    [[nodiscard]] OwningDigitalIntervalStorage const* tryGetOwning() const {
        return tryGet<OwningDigitalIntervalStorage>();
    }

private:
    struct StorageConcept {
        virtual ~StorageConcept() = default;
        
        virtual size_t size() const = 0;
        virtual Interval const& getInterval(size_t idx) const = 0;
        virtual EntityId getEntityId(size_t idx) const = 0;
        virtual std::optional<size_t> findByInterval(Interval const& interval) const = 0;
        virtual std::optional<size_t> findByEntityId(EntityId id) const = 0;
        virtual bool hasIntervalAtTime(int64_t time) const = 0;
        virtual std::pair<size_t, size_t> getOverlappingRange(int64_t start, int64_t end) const = 0;
        virtual std::pair<size_t, size_t> getContainedRange(int64_t start, int64_t end) const = 0;
        virtual DigitalIntervalStorageType getStorageType() const = 0;
        virtual DigitalIntervalStorageCache tryGetCache() const = 0;
        
        // Mutation
        virtual bool addInterval(Interval const& interval, EntityId entity_id) = 0;
        virtual bool removeInterval(Interval const& interval) = 0;
        virtual bool removeByEntityId(EntityId id) = 0;
        virtual void reserve(size_t capacity) = 0;
        virtual void clear() = 0;
        virtual void setEntityIds(std::vector<EntityId> ids) = 0;
    };

    template<typename StorageImpl>
    struct StorageModel : StorageConcept {
        StorageImpl _storage;

        explicit StorageModel(StorageImpl storage)
            : _storage(std::move(storage)) {}

        size_t size() const override { return _storage.size(); }
        
        Interval const& getInterval(size_t idx) const override {
            return _storage.getInterval(idx);
        }
        
        EntityId getEntityId(size_t idx) const override {
            return _storage.getEntityId(idx);
        }
        
        std::optional<size_t> findByInterval(Interval const& interval) const override {
            return _storage.findByInterval(interval);
        }
        
        std::optional<size_t> findByEntityId(EntityId id) const override {
            return _storage.findByEntityId(id);
        }
        
        bool hasIntervalAtTime(int64_t time) const override {
            return _storage.hasIntervalAtTime(time);
        }
        
        std::pair<size_t, size_t> getOverlappingRange(int64_t start, int64_t end) const override {
            return _storage.getOverlappingRange(start, end);
        }
        
        std::pair<size_t, size_t> getContainedRange(int64_t start, int64_t end) const override {
            return _storage.getContainedRange(start, end);
        }
        
        DigitalIntervalStorageType getStorageType() const override {
            return _storage.getStorageType();
        }
        
        DigitalIntervalStorageCache tryGetCache() const override {
            if constexpr (requires { _storage.tryGetCache(); }) {
                return _storage.tryGetCache();
            } else {
                return DigitalIntervalStorageCache{};
            }
        }

        // Mutation - only for OwningDigitalIntervalStorage
        bool addInterval(Interval const& interval, EntityId entity_id) override {
            if constexpr (requires { _storage.addInterval(interval, entity_id); }) {
                return _storage.addInterval(interval, entity_id);
            } else {
                throw std::runtime_error("addInterval() not supported for view/lazy storage");
            }
        }
        
        bool removeInterval(Interval const& interval) override {
            if constexpr (requires { _storage.removeInterval(interval); }) {
                return _storage.removeInterval(interval);
            } else {
                throw std::runtime_error("removeInterval() not supported for view/lazy storage");
            }
        }
        
        bool removeByEntityId(EntityId id) override {
            if constexpr (requires { _storage.removeByEntityId(id); }) {
                return _storage.removeByEntityId(id);
            } else {
                throw std::runtime_error("removeByEntityId() not supported for view/lazy storage");
            }
        }
        
        void reserve(size_t capacity) override {
            if constexpr (requires { _storage.reserve(capacity); }) {
                _storage.reserve(capacity);
            }
            // No-op for storage types that don't support reserve
        }
        
        void clear() override {
            if constexpr (requires { _storage.clear(); }) {
                _storage.clear();
            } else {
                throw std::runtime_error("clear() not supported for view/lazy storage");
            }
        }
        
        void setEntityIds(std::vector<EntityId> ids) override {
            if constexpr (requires { _storage.setEntityIds(std::move(ids)); }) {
                _storage.setEntityIds(std::move(ids));
            } else {
                throw std::runtime_error("setEntityIds() not supported for view/lazy storage");
            }
        }
    };

    std::unique_ptr<StorageConcept> _impl;
};

#endif // DIGITAL_INTERVAL_STORAGE_HPP
