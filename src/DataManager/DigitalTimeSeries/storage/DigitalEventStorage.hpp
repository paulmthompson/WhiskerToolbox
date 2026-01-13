#ifndef DIGITAL_EVENT_STORAGE_HPP
#define DIGITAL_EVENT_STORAGE_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
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
 * @brief Hash specialization for TimeFrameIndex to allow use in unordered containers
 */
template<>
struct std::hash<TimeFrameIndex> {
    size_t operator()(TimeFrameIndex const& idx) const noexcept {
        return std::hash<int64_t>{}(idx.getValue());
    }
};

// Forward declarations
class OwningDigitalEventStorage;
class ViewDigitalEventStorage;

/**
 * @brief Storage type enumeration for digital event storage
 */
enum class DigitalEventStorageType {
    Owning,  ///< Owns the data in SoA layout
    View,    ///< References another storage via indices
    Lazy     ///< Lazy-evaluated transform
};

// =============================================================================
// Cache Optimization Structure
// =============================================================================

/**
 * @brief Cache structure for fast-path access to contiguous digital event storage
 * 
 * Digital events store TimeFrameIndex values (event times) with associated
 * EntityIds. The storage is organized as parallel arrays:
 * - events[i] - TimeFrameIndex for event i
 * - entity_ids[i] - EntityId for event i
 * 
 * @note Digital events are always sorted by time.
 */
struct DigitalEventStorageCache {
    TimeFrameIndex const* events_ptr = nullptr;
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
    [[nodiscard]] TimeFrameIndex getEvent(size_t idx) const noexcept {
        return events_ptr[idx];
    }
    
    [[nodiscard]] EntityId getEntityId(size_t idx) const noexcept {
        return entity_ids_ptr[idx];
    }
};

// =============================================================================
// CRTP Base Class
// =============================================================================

/**
 * @brief CRTP base class for digital event storage implementations
 * 
 * Uses Curiously Recurring Template Pattern to eliminate virtual function overhead
 * while maintaining a polymorphic interface. Events are stored as TimeFrameIndex
 * values (representing event times) with associated EntityIds.
 * 
 * The SoA (Structure of Arrays) layout stores parallel vectors:
 * - TimeFrameIndex events[] - sorted event times
 * - EntityId entity_ids[] - corresponding entity identifiers
 * 
 * Unlike ragged storage, digital events have at most one entry per time
 * (events at the same time are distinct entries sorted together).
 * 
 * @tparam Derived The concrete storage implementation type
 */
template<typename Derived>
class DigitalEventStorageBase {
public:
    // ========== Size & Bounds ==========
    
    /**
     * @brief Get total number of events
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
     * @brief Get the event time at a flat index
     * @param idx Flat index in [0, size())
     */
    [[nodiscard]] TimeFrameIndex getEvent(size_t idx) const {
        return static_cast<Derived const*>(this)->getEventImpl(idx);
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
     * @brief Find the index of an event by its exact time
     * @param time The exact TimeFrameIndex to find
     * @return Index of the event, or std::nullopt if not found
     */
    [[nodiscard]] std::optional<size_t> findByTime(TimeFrameIndex time) const {
        return static_cast<Derived const*>(this)->findByTimeImpl(time);
    }
    
    /**
     * @brief Find the index of an event by its EntityId
     * @param id The EntityId to search for
     * @return Index of the event, or std::nullopt if not found
     */
    [[nodiscard]] std::optional<size_t> findByEntityId(EntityId id) const {
        return static_cast<Derived const*>(this)->findByEntityIdImpl(id);
    }
    
    /**
     * @brief Check if an event exists at a specific time
     */
    [[nodiscard]] bool hasEventAtTime(TimeFrameIndex time) const {
        return findByTime(time).has_value();
    }
    
    /**
     * @brief Get range of indices for events in [start, end] inclusive
     * @param start Start time (inclusive)
     * @param end End time (inclusive)
     * @return Pair of (start_idx, end_idx) where end is exclusive
     */
    [[nodiscard]] std::pair<size_t, size_t> getTimeRange(TimeFrameIndex start, TimeFrameIndex end) const {
        return static_cast<Derived const*>(this)->getTimeRangeImpl(start, end);
    }

    // ========== Storage Type ==========
    
    /**
     * @brief Get the storage type identifier
     */
    [[nodiscard]] DigitalEventStorageType getStorageType() const {
        return static_cast<Derived const*>(this)->getStorageTypeImpl();
    }
    
    /**
     * @brief Check if this is a view (doesn't own data)
     */
    [[nodiscard]] bool isView() const {
        return getStorageType() == DigitalEventStorageType::View;
    }
    
    /**
     * @brief Check if this is lazy storage
     */
    [[nodiscard]] bool isLazy() const {
        return getStorageType() == DigitalEventStorageType::Lazy;
    }

    // ========== Cache Optimization ==========
    
    /**
     * @brief Try to get cached pointers for fast-path access
     * 
     * @return DigitalEventStorageCache with valid pointers if contiguous, invalid otherwise
     */
    [[nodiscard]] DigitalEventStorageCache tryGetCache() const {
        return static_cast<Derived const*>(this)->tryGetCacheImpl();
    }

protected:
    ~DigitalEventStorageBase() = default;
};

// =============================================================================
// Owning Storage (SoA Layout)
// =============================================================================

/**
 * @brief Owning digital event storage using Structure of Arrays layout
 * 
 * Stores event data in parallel vectors for cache-friendly access:
 * - _events[i] - TimeFrameIndex for event i (sorted)
 * - _entity_ids[i] - EntityId for event i
 * 
 * Maintains acceleration structures:
 * - Events are always sorted by time
 * - O(log n) lookup by time using binary search
 * - O(1) lookup by EntityId using hash map
 */
class OwningDigitalEventStorage : public DigitalEventStorageBase<OwningDigitalEventStorage> {
public:
    OwningDigitalEventStorage() = default;
    
    /**
     * @brief Construct from existing event vector (will sort)
     */
    explicit OwningDigitalEventStorage(std::vector<TimeFrameIndex> events) {
        _events = std::move(events);
        _sortEvents();
        // Entity IDs will be empty until rebuilt
        _entity_ids.resize(_events.size(), EntityId{0});
    }
    
    /**
     * @brief Construct from existing event and entity ID vectors
     * @note Both vectors must be the same size
     */
    OwningDigitalEventStorage(std::vector<TimeFrameIndex> events, std::vector<EntityId> entity_ids) {
        if (events.size() != entity_ids.size()) {
            throw std::invalid_argument("Events and entity_ids must have same size");
        }
        _events = std::move(events);
        _entity_ids = std::move(entity_ids);
        _sortEventsWithEntityIds();
        _rebuildEntityIdIndex();
    }
    
    // ========== Modification ==========
    
    /**
     * @brief Add an event at the specified time
     * 
     * If an event already exists at this exact time, it is not added (returns false).
     * 
     * @param time The event time
     * @param entity_id The EntityId for this event
     * @return true if added, false if duplicate time
     */
    bool addEvent(TimeFrameIndex time, EntityId entity_id) {
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
    
    /**
     * @brief Remove an event at the specified time
     * @return true if removed, false if not found
     */
    bool removeEvent(TimeFrameIndex time) {
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
    
    /**
     * @brief Remove an event by EntityId
     * @return true if removed, false if not found
     */
    bool removeByEntityId(EntityId id) {
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
    
    /**
     * @brief Clear all events
     */
    void clear() {
        _events.clear();
        _entity_ids.clear();
        _entity_id_to_index.clear();
    }
    
    /**
     * @brief Reserve capacity for expected number of events
     */
    void reserve(size_t capacity) {
        _events.reserve(capacity);
        _entity_ids.reserve(capacity);
    }
    
    /**
     * @brief Set all entity IDs (must match event count)
     */
    void setEntityIds(std::vector<EntityId> ids) {
        if (ids.size() != _events.size()) {
            throw std::invalid_argument("EntityId count must match event count");
        }
        _entity_ids = std::move(ids);
        _rebuildEntityIdIndex();
    }

    // ========== CRTP Implementation ==========
    
    [[nodiscard]] size_t sizeImpl() const { return _events.size(); }
    
    [[nodiscard]] TimeFrameIndex getEventImpl(size_t idx) const { return _events[idx]; }
    
    [[nodiscard]] EntityId getEntityIdImpl(size_t idx) const {
        return idx < _entity_ids.size() ? _entity_ids[idx] : EntityId{0};
    }
    
    [[nodiscard]] std::optional<size_t> findByTimeImpl(TimeFrameIndex time) const {
        auto it = std::ranges::lower_bound(_events, time);
        if (it != _events.end() && *it == time) {
            return static_cast<size_t>(std::distance(_events.begin(), it));
        }
        return std::nullopt;
    }
    
    [[nodiscard]] std::optional<size_t> findByEntityIdImpl(EntityId id) const {
        auto it = _entity_id_to_index.find(id);
        return it != _entity_id_to_index.end() ? std::optional{it->second} : std::nullopt;
    }
    
    [[nodiscard]] std::pair<size_t, size_t> getTimeRangeImpl(TimeFrameIndex start, TimeFrameIndex end) const {
        auto it_start = std::ranges::lower_bound(_events, start);
        auto it_end = std::ranges::upper_bound(_events, end);
        
        return {
            static_cast<size_t>(std::distance(_events.begin(), it_start)),
            static_cast<size_t>(std::distance(_events.begin(), it_end))
        };
    }
    
    [[nodiscard]] DigitalEventStorageType getStorageTypeImpl() const { 
        return DigitalEventStorageType::Owning; 
    }

    /**
     * @brief Get cache with pointers to contiguous data
     * 
     * OwningDigitalEventStorage stores data contiguously in SoA layout,
     * so it always returns a valid cache for fast-path iteration.
     */
    [[nodiscard]] DigitalEventStorageCache tryGetCacheImpl() const {
        return DigitalEventStorageCache{
            _events.data(),
            _entity_ids.data(),
            _events.size(),
            true  // is_contiguous
        };
    }

    // ========== Direct Array Access ==========
    
    [[nodiscard]] std::vector<TimeFrameIndex> const& events() const { return _events; }
    [[nodiscard]] std::vector<EntityId> const& entityIds() const { return _entity_ids; }
    
    [[nodiscard]] std::span<TimeFrameIndex const> eventsSpan() const { return _events; }
    [[nodiscard]] std::span<EntityId const> entityIdsSpan() const { return _entity_ids; }

private:
    void _sortEvents() {
        std::ranges::sort(_events);
    }
    
    void _sortEventsWithEntityIds() {
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
    
    void _rebuildEntityIdIndex() {
        _entity_id_to_index.clear();
        for (size_t i = 0; i < _entity_ids.size(); ++i) {
            _entity_id_to_index[_entity_ids[i]] = i;
        }
    }

    std::vector<TimeFrameIndex> _events;
    std::vector<EntityId> _entity_ids;
    std::unordered_map<EntityId, size_t> _entity_id_to_index;
};

// =============================================================================
// View Storage (References Source via Indices)
// =============================================================================

/**
 * @brief View-based digital event storage that references another storage
 * 
 * Holds a shared_ptr to a source OwningDigitalEventStorage and a vector of indices
 * into that source. Enables zero-copy filtered views.
 */
class ViewDigitalEventStorage : public DigitalEventStorageBase<ViewDigitalEventStorage> {
public:
    /**
     * @brief Construct a view referencing source storage
     * 
     * @param source Shared pointer to source storage
     */
    explicit ViewDigitalEventStorage(std::shared_ptr<OwningDigitalEventStorage const> source)
        : _source(std::move(source)) {}
    
    /**
     * @brief Set the indices this view includes
     */
    void setIndices(std::vector<size_t> indices) {
        _indices = std::move(indices);
        _rebuildLocalIndices();
    }
    
    /**
     * @brief Create view of all events
     */
    void setAllIndices() {
        _indices.resize(_source->size());
        std::iota(_indices.begin(), _indices.end(), 0);
        _rebuildLocalIndices();
    }
    
    /**
     * @brief Filter by time range [start, end] inclusive
     */
    void filterByTimeRange(TimeFrameIndex start, TimeFrameIndex end) {
        auto [src_start, src_end] = _source->getTimeRange(start, end);
        
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
    [[nodiscard]] std::shared_ptr<OwningDigitalEventStorage const> source() const {
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
    
    [[nodiscard]] TimeFrameIndex getEventImpl(size_t idx) const {
        return _source->getEvent(_indices[idx]);
    }
    
    [[nodiscard]] EntityId getEntityIdImpl(size_t idx) const {
        return _source->getEntityId(_indices[idx]);
    }
    
    [[nodiscard]] std::optional<size_t> findByTimeImpl(TimeFrameIndex time) const {
        // Binary search since events are sorted
        auto it = std::ranges::lower_bound(_indices, time, {}, [this](size_t idx) {
            return _source->getEvent(idx);
        });
        
        if (it != _indices.end() && _source->getEvent(*it) == time) {
            return static_cast<size_t>(std::distance(_indices.begin(), it));
        }
        return std::nullopt;
    }
    
    [[nodiscard]] std::optional<size_t> findByEntityIdImpl(EntityId id) const {
        auto it = _local_entity_id_to_index.find(id);
        return it != _local_entity_id_to_index.end() ? std::optional{it->second} : std::nullopt;
    }
    
    [[nodiscard]] std::pair<size_t, size_t> getTimeRangeImpl(TimeFrameIndex start, TimeFrameIndex end) const {
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
    
    [[nodiscard]] DigitalEventStorageType getStorageTypeImpl() const { 
        return DigitalEventStorageType::View; 
    }

    /**
     * @brief Return cache if view is contiguous
     */
    [[nodiscard]] DigitalEventStorageCache tryGetCacheImpl() const {
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

private:
    void _rebuildLocalIndices() {
        _local_entity_id_to_index.clear();
        for (size_t i = 0; i < _indices.size(); ++i) {
            EntityId const id = _source->getEntityId(_indices[i]);
            _local_entity_id_to_index[id] = i;
        }
    }
    
    std::shared_ptr<OwningDigitalEventStorage const> _source;
    std::vector<size_t> _indices;
    std::unordered_map<EntityId, size_t> _local_entity_id_to_index;
};

// =============================================================================
// Lazy Storage (View-based Computation on Demand)
// =============================================================================

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

// =============================================================================
// Type-Erased Storage Wrapper (Virtual Dispatch)
// =============================================================================

/**
 * @brief Type-erased storage wrapper for digital event storage
 * 
 * Provides a uniform interface for any storage backend while hiding the
 * concrete storage type. Supports lazy transforms with unbounded ViewType.
 */
class DigitalEventStorageWrapper {
public:
    /**
     * @brief Construct wrapper from any storage implementation
     */
    template<typename StorageImpl>
    explicit DigitalEventStorageWrapper(StorageImpl storage)
        : _impl(std::make_unique<StorageModel<StorageImpl>>(std::move(storage))) {}

    // Default constructor creates empty owning storage
    DigitalEventStorageWrapper()
        : _impl(std::make_unique<StorageModel<OwningDigitalEventStorage>>(
              OwningDigitalEventStorage{})) {}

    // Move-only semantics
    DigitalEventStorageWrapper(DigitalEventStorageWrapper&&) noexcept = default;
    DigitalEventStorageWrapper& operator=(DigitalEventStorageWrapper&&) noexcept = default;
    DigitalEventStorageWrapper(DigitalEventStorageWrapper const&) = delete;
    DigitalEventStorageWrapper& operator=(DigitalEventStorageWrapper const&) = delete;

    // ========== Unified Interface ==========
    
    [[nodiscard]] size_t size() const { return _impl->size(); }
    [[nodiscard]] bool empty() const { return _impl->size() == 0; }

    [[nodiscard]] TimeFrameIndex getEvent(size_t idx) const {
        return _impl->getEvent(idx);
    }

    [[nodiscard]] EntityId getEntityId(size_t idx) const {
        return _impl->getEntityId(idx);
    }

    [[nodiscard]] std::optional<size_t> findByTime(TimeFrameIndex time) const {
        return _impl->findByTime(time);
    }
    
    [[nodiscard]] std::optional<size_t> findByEntityId(EntityId id) const {
        return _impl->findByEntityId(id);
    }

    [[nodiscard]] std::pair<size_t, size_t> getTimeRange(TimeFrameIndex start, TimeFrameIndex end) const {
        return _impl->getTimeRange(start, end);
    }
    
    [[nodiscard]] bool hasEventAtTime(TimeFrameIndex time) const {
        return findByTime(time).has_value();
    }

    [[nodiscard]] DigitalEventStorageType getStorageType() const {
        return _impl->getStorageType();
    }

    [[nodiscard]] bool isView() const {
        return getStorageType() == DigitalEventStorageType::View;
    }
    
    [[nodiscard]] bool isLazy() const {
        return getStorageType() == DigitalEventStorageType::Lazy;
    }

    // ========== Cache Optimization ==========
    
    [[nodiscard]] DigitalEventStorageCache tryGetCache() const {
        return _impl->tryGetCache();
    }

    // ========== Mutation Operations ==========
    
    bool addEvent(TimeFrameIndex time, EntityId entity_id) {
        return _impl->addEvent(time, entity_id);
    }

    bool removeEvent(TimeFrameIndex time) {
        return _impl->removeEvent(time);
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
    [[nodiscard]] OwningDigitalEventStorage* tryGetMutableOwning() {
        return tryGet<OwningDigitalEventStorage>();
    }
    
    [[nodiscard]] OwningDigitalEventStorage const* tryGetOwning() const {
        return tryGet<OwningDigitalEventStorage>();
    }

private:
    struct StorageConcept {
        virtual ~StorageConcept() = default;
        
        virtual size_t size() const = 0;
        virtual TimeFrameIndex getEvent(size_t idx) const = 0;
        virtual EntityId getEntityId(size_t idx) const = 0;
        virtual std::optional<size_t> findByTime(TimeFrameIndex time) const = 0;
        virtual std::optional<size_t> findByEntityId(EntityId id) const = 0;
        virtual std::pair<size_t, size_t> getTimeRange(TimeFrameIndex start, TimeFrameIndex end) const = 0;
        virtual DigitalEventStorageType getStorageType() const = 0;
        virtual DigitalEventStorageCache tryGetCache() const = 0;
        
        // Mutation
        virtual bool addEvent(TimeFrameIndex time, EntityId entity_id) = 0;
        virtual bool removeEvent(TimeFrameIndex time) = 0;
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
        
        TimeFrameIndex getEvent(size_t idx) const override {
            return _storage.getEvent(idx);
        }
        
        EntityId getEntityId(size_t idx) const override {
            return _storage.getEntityId(idx);
        }
        
        std::optional<size_t> findByTime(TimeFrameIndex time) const override {
            return _storage.findByTime(time);
        }
        
        std::optional<size_t> findByEntityId(EntityId id) const override {
            return _storage.findByEntityId(id);
        }
        
        std::pair<size_t, size_t> getTimeRange(TimeFrameIndex start, TimeFrameIndex end) const override {
            return _storage.getTimeRange(start, end);
        }
        
        DigitalEventStorageType getStorageType() const override {
            return _storage.getStorageType();
        }
        
        DigitalEventStorageCache tryGetCache() const override {
            if constexpr (requires { _storage.tryGetCache(); }) {
                return _storage.tryGetCache();
            } else {
                return DigitalEventStorageCache{};
            }
        }

        // Mutation - only for OwningDigitalEventStorage
        bool addEvent(TimeFrameIndex time, EntityId entity_id) override {
            if constexpr (requires { _storage.addEvent(time, entity_id); }) {
                return _storage.addEvent(time, entity_id);
            } else {
                throw std::runtime_error("addEvent() not supported for view/lazy storage");
            }
        }
        
        bool removeEvent(TimeFrameIndex time) override {
            if constexpr (requires { _storage.removeEvent(time); }) {
                return _storage.removeEvent(time);
            } else {
                throw std::runtime_error("removeEvent() not supported for view/lazy storage");
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

#endif // DIGITAL_EVENT_STORAGE_HPP
