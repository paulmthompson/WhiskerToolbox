#ifndef RAGGED_STORAGE_HPP
#define RAGGED_STORAGE_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// Forward declarations
template<typename TData> class OwningRaggedStorage;
template<typename TData> class ViewRaggedStorage;

/**
 * @brief Storage type enumeration for runtime type identification
 */
enum class RaggedStorageType {
    Owning,  ///< Owns the data in SoA layout
    View,    ///< References another storage via indices
    Lazy     ///< Lazy-evaluated transform (future support)
};

// =============================================================================
// Cache Optimization Structure
// =============================================================================

/**
 * @brief Cache structure for fast-path access to contiguous storage
 * 
 * When storage is contiguous (OwningRaggedStorage), iterators can use
 * cached pointers for zero-overhead access. For non-contiguous storage
 * (ViewRaggedStorage, LazyRaggedStorage), the cache is invalid and
 * iterators fall back to virtual dispatch.
 * 
 * This optimization mirrors the pattern used in AnalogTimeSeries where
 * contiguous storage gets pointer caching for fast iteration.
 * 
 * @tparam TData The data type stored (e.g., Mask2D, Line2D, Point2D<float>)
 */
template<typename TData>
struct RaggedStorageCache {
    TimeFrameIndex const* times_ptr = nullptr;
    TData const* data_ptr = nullptr;
    EntityId const* entity_ids_ptr = nullptr;
    size_t cache_size = 0;
    bool is_contiguous = false;  ///< True if storage is contiguous (owning)
    
    /**
     * @brief Check if the cache is valid for fast-path access
     * 
     * A valid cache indicates that the underlying storage is contiguous
     * and pointer arithmetic can be used for iteration. Note that an
     * empty owning storage still has a valid cache (is_contiguous=true),
     * it just has cache_size=0.
     * 
     * @return true if storage is contiguous (can use direct pointer access)
     * @return false if storage is non-contiguous (must use virtual dispatch)
     */
    [[nodiscard]] constexpr bool isValid() const noexcept {
        return is_contiguous;
    }
    
    // Convenience accessors for cached data (only valid if isValid() && idx < cache_size)
    
    [[nodiscard]] TimeFrameIndex getTime(size_t idx) const noexcept {
        return times_ptr[idx];
    }
    
    [[nodiscard]] TData const& getData(size_t idx) const noexcept {
        return data_ptr[idx];
    }
    
    [[nodiscard]] EntityId getEntityId(size_t idx) const noexcept {
        return entity_ids_ptr[idx];
    }
};

// =============================================================================
// CRTP Base Class
// =============================================================================

/**
 * @brief CRTP base class for ragged storage implementations
 * 
 * Uses Curiously Recurring Template Pattern to eliminate virtual function overhead
 * while maintaining a polymorphic interface. Derived classes implement the actual
 * storage strategy (owning SoA or view/filter).
 * 
 * The SoA (Structure of Arrays) layout stores parallel vectors:
 * - TimeFrameIndex times[]
 * - TData data[]
 * - EntityId entity_ids[]
 * 
 * This provides:
 * - O(1) EntityId lookup via hash map
 * - Cache-friendly sequential iteration
 * - Efficient view creation without data copying
 * 
 * @tparam Derived The concrete storage implementation type
 * @tparam TData The data type stored (e.g., Mask2D, Line2D, Point2D<float>)
 */
template<typename Derived, typename TData>
class RaggedStorageBase {
public:
    // ========== Size & Bounds ==========
    
    /**
     * @brief Get total number of entries across all times
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
     * @brief Get the TimeFrameIndex at a flat index
     * @param idx Flat index in [0, size())
     */
    [[nodiscard]] TimeFrameIndex getTime(size_t idx) const {
        return static_cast<Derived const*>(this)->getTimeImpl(idx);
    }
    
    /**
     * @brief Get const reference to data at a flat index
     * @param idx Flat index in [0, size())
     */
    [[nodiscard]] TData const& getData(size_t idx) const {
        return static_cast<Derived const*>(this)->getDataImpl(idx);
    }
    
    /**
     * @brief Get the EntityId at a flat index
     * @param idx Flat index in [0, size())
     */
    [[nodiscard]] EntityId getEntityId(size_t idx) const {
        return static_cast<Derived const*>(this)->getEntityIdImpl(idx);
    }

    // ========== EntityId Lookup ==========
    
    /**
     * @brief Find flat index by EntityId
     * @param id The EntityId to find
     * @return Flat index if found, std::nullopt otherwise
     * @note O(1) for OwningRaggedStorage, O(1) for ViewRaggedStorage
     */
    [[nodiscard]] std::optional<size_t> findByEntityId(EntityId id) const {
        return static_cast<Derived const*>(this)->findByEntityIdImpl(id);
    }

    // ========== Time-based Access ==========
    
    /**
     * @brief Get range of flat indices for a specific time
     * @param time The TimeFrameIndex to query
     * @return Pair of (start_idx, end_idx) where end is exclusive, or (0,0) if not found
     */
    [[nodiscard]] std::pair<size_t, size_t> getTimeRange(TimeFrameIndex time) const {
        return static_cast<Derived const*>(this)->getTimeRangeImpl(time);
    }
    
    /**
     * @brief Get number of distinct times with data
     */
    [[nodiscard]] size_t getTimeCount() const {
        return static_cast<Derived const*>(this)->getTimeCountImpl();
    }

    // ========== Storage Type ==========
    
    /**
     * @brief Get the storage type identifier
     */
    [[nodiscard]] RaggedStorageType getStorageType() const {
        return static_cast<Derived const*>(this)->getStorageTypeImpl();
    }
    
    /**
     * @brief Check if this is a view (doesn't own data)
     */
    [[nodiscard]] bool isView() const {
        return getStorageType() == RaggedStorageType::View;
    }

    // ========== Cache Optimization ==========
    
    /**
     * @brief Try to get cached pointers for fast-path access
     * 
     * Returns a cache structure with direct pointers to contiguous data.
     * If the storage is non-contiguous (e.g., ViewRaggedStorage), returns
     * an invalid cache and callers must use the virtual dispatch path.
     * 
     * @return RaggedStorageCache<TData> with valid pointers if contiguous, invalid otherwise
     */
    [[nodiscard]] RaggedStorageCache<TData> tryGetCache() const {
        return static_cast<Derived const*>(this)->tryGetCacheImpl();
    }

protected:
    ~RaggedStorageBase() = default;
};

// =============================================================================
// Owning Storage (SoA Layout)
// =============================================================================

/**
 * @brief Owning ragged storage using Structure of Arrays layout
 * 
 * Stores data in parallel vectors for cache-friendly access:
 * - _times[i] - TimeFrameIndex for entry i
 * - _data[i] - TData for entry i
 * - _entity_ids[i] - EntityId for entry i
 * 
 * Maintains acceleration structures:
 * - _entity_to_index: O(1) EntityId lookup
 * - _time_ranges: O(log n) time range lookup
 * 
 * @tparam TData The data type stored (e.g., Mask2D, Line2D, Point2D<float>)
 */
template<typename TData>
class OwningRaggedStorage : public RaggedStorageBase<OwningRaggedStorage<TData>, TData> {
public:
    OwningRaggedStorage() = default;
    
    // ========== Modification ==========
    
    /**
     * @brief Append a new entry (most efficient insertion)
     * 
     * Entries should be appended in time order for optimal time_ranges performance.
     * 
     * @param time The TimeFrameIndex for this entry
     * @param data The data to store (will be moved)
     * @param entity_id The EntityId for this entry
     */
    void append(TimeFrameIndex time, TData&& data, EntityId entity_id) {
        size_t const idx = _times.size();
        
        _times.push_back(time);
        _data.push_back(std::move(data));
        _entity_ids.push_back(entity_id);
        
        // Update acceleration structures
        _entity_to_index[entity_id] = idx;
        _updateTimeRanges(time, idx);
    }
    
    /**
     * @brief Append a new entry (copy version)
     */
    void append(TimeFrameIndex time, TData const& data, EntityId entity_id) {
        size_t const idx = _times.size();
        
        _times.push_back(time);
        _data.push_back(data);
        _entity_ids.push_back(entity_id);
        
        _entity_to_index[entity_id] = idx;
        _updateTimeRanges(time, idx);
    }
    
    /**
     * @brief Reserve capacity for expected number of entries
     */
    void reserve(size_t capacity) {
        _times.reserve(capacity);
        _data.reserve(capacity);
        _entity_ids.reserve(capacity);
    }
    
    /**
     * @brief Clear all data
     */
    void clear() {
        _times.clear();
        _data.clear();
        _entity_ids.clear();
        _entity_to_index.clear();
        _time_ranges.clear();
    }
    
    /**
     * @brief Remove entry by EntityId
     * @param entity_id The EntityId to remove
     * @return true if found and removed, false otherwise
     * 
     * @note This is O(n) due to vector erasure. For bulk removal, 
     * consider collecting indices and using removeByIndices().
     */
    bool removeByEntityId(EntityId entity_id) {
        auto it = _entity_to_index.find(entity_id);
        if (it == _entity_to_index.end()) {
            return false;
        }
        
        size_t const idx = it->second;
        
        // Erase from arrays
        _times.erase(_times.begin() + static_cast<std::ptrdiff_t>(idx));
        _data.erase(_data.begin() + static_cast<std::ptrdiff_t>(idx));
        _entity_ids.erase(_entity_ids.begin() + static_cast<std::ptrdiff_t>(idx));
        
        // Rebuild acceleration structures (indices shifted)
        _rebuildAccelerationStructures();
        
        return true;
    }
    
    /**
     * @brief Remove all entries at a specific time
     * @param time The TimeFrameIndex to remove all entries for
     * @return Number of entries removed
     * 
     * @note More efficient than calling removeByEntityId multiple times
     */
    size_t removeAtTime(TimeFrameIndex time) {
        auto it = _time_ranges.find(time);
        if (it == _time_ranges.end()) {
            return 0;
        }
        
        auto [start, end] = it->second;
        size_t const count = end - start;
        
        // Erase the range from all vectors
        _times.erase(_times.begin() + static_cast<std::ptrdiff_t>(start),
                     _times.begin() + static_cast<std::ptrdiff_t>(end));
        _data.erase(_data.begin() + static_cast<std::ptrdiff_t>(start),
                    _data.begin() + static_cast<std::ptrdiff_t>(end));
        _entity_ids.erase(_entity_ids.begin() + static_cast<std::ptrdiff_t>(start),
                          _entity_ids.begin() + static_cast<std::ptrdiff_t>(end));
        
        // Rebuild acceleration structures
        _rebuildAccelerationStructures();
        
        return count;
    }

    // ========== CRTP Implementation ==========
    
    [[nodiscard]] size_t sizeImpl() const { return _times.size(); }
    
    [[nodiscard]] TimeFrameIndex getTimeImpl(size_t idx) const { return _times[idx]; }
    
    [[nodiscard]] TData const& getDataImpl(size_t idx) const { return _data[idx]; }
    
    [[nodiscard]] EntityId getEntityIdImpl(size_t idx) const { return _entity_ids[idx]; }
    
    [[nodiscard]] std::optional<size_t> findByEntityIdImpl(EntityId id) const {
        auto it = _entity_to_index.find(id);
        return it != _entity_to_index.end() ? std::optional{it->second} : std::nullopt;
    }
    
    [[nodiscard]] std::pair<size_t, size_t> getTimeRangeImpl(TimeFrameIndex time) const {
        auto it = _time_ranges.find(time);
        return it != _time_ranges.end() ? it->second : std::pair<size_t, size_t>{0, 0};
    }
    
    [[nodiscard]] size_t getTimeCountImpl() const { return _time_ranges.size(); }
    
    [[nodiscard]] RaggedStorageType getStorageTypeImpl() const { return RaggedStorageType::Owning; }

    /**
     * @brief Get cache with pointers to contiguous data
     * 
     * OwningRaggedStorage stores data contiguously in SoA layout,
     * so it always returns a valid cache for fast-path iteration.
     */
    [[nodiscard]] RaggedStorageCache<TData> tryGetCacheImpl() const {
        return RaggedStorageCache<TData>{
            _times.data(),
            _data.data(),
            _entity_ids.data(),
            _times.size(),
            true  // is_contiguous - owning storage is always contiguous
        };
    }

    // ========== Direct Array Access (for views and iteration) ==========
    
    [[nodiscard]] std::vector<TimeFrameIndex> const& times() const { return _times; }
    [[nodiscard]] std::vector<TData> const& data() const { return _data; }
    [[nodiscard]] std::vector<EntityId> const& entityIds() const { return _entity_ids; }
    
    [[nodiscard]] std::span<TimeFrameIndex const> timesSpan() const { return _times; }
    [[nodiscard]] std::span<TData const> dataSpan() const { return _data; }
    [[nodiscard]] std::span<EntityId const> entityIdsSpan() const { return _entity_ids; }
    
    /**
     * @brief Get mutable reference to data (use with caution)
     * 
     * Modifications through this reference do not update acceleration structures.
     * Only use for in-place modifications that don't change EntityId or time.
     */
    [[nodiscard]] TData& getMutableData(size_t idx) { return _data[idx]; }
    
    /**
     * @brief Get the time ranges map for iteration
     */
    [[nodiscard]] std::map<TimeFrameIndex, std::pair<size_t, size_t>> const& timeRanges() const {
        return _time_ranges;
    }

private:
    void _updateTimeRanges(TimeFrameIndex time, size_t idx) {
        auto it = _time_ranges.find(time);
        if (it == _time_ranges.end()) {
            // New time - start and end are both idx
            _time_ranges[time] = {idx, idx + 1};
        } else {
            // Existing time - extend end (assumes appending in order)
            it->second.second = idx + 1;
        }
    }
    
    void _rebuildAccelerationStructures() {
        _entity_to_index.clear();
        _time_ranges.clear();
        
        for (size_t i = 0; i < _times.size(); ++i) {
            _entity_to_index[_entity_ids[i]] = i;
            _updateTimeRanges(_times[i], i);
        }
    }

    std::vector<TimeFrameIndex> _times;
    std::vector<TData> _data;
    std::vector<EntityId> _entity_ids;
    std::unordered_map<EntityId, size_t> _entity_to_index;
    std::map<TimeFrameIndex, std::pair<size_t, size_t>> _time_ranges;
};

// =============================================================================
// View Storage (References Source via Indices)
// =============================================================================

/**
 * @brief View-based ragged storage that references another storage
 * 
 * Holds a shared_ptr to a source OwningRaggedStorage and a vector of indices
 * into that source. This enables:
 * - Zero-copy filtered views (e.g., by EntityId set, time range)
 * - 83% memory savings vs full copy (just indices + local EntityId map)
 * - Automatic invalidation when source is destroyed
 * 
 * @tparam TData The data type stored (e.g., Mask2D, Line2D, Point2D<float>)
 */
template<typename TData>
class ViewRaggedStorage : public RaggedStorageBase<ViewRaggedStorage<TData>, TData> {
public:
    /**
     * @brief Construct a view referencing source storage
     * 
     * @param source Shared pointer to source storage (must remain valid)
     */
    explicit ViewRaggedStorage(std::shared_ptr<OwningRaggedStorage<TData> const> source)
        : _source(std::move(source)) {}
    
    /**
     * @brief Set the indices this view includes
     * 
     * The indices refer to positions in the source storage.
     * Call this after construction to define what the view shows.
     * 
     * @param indices Vector of source indices (will be moved)
     */
    void setIndices(std::vector<size_t> indices) {
        _indices = std::move(indices);
        _rebuildLocalEntityIndex();
    }
    
    /**
     * @brief Create view of all entries (useful as starting point for chained operations)
     */
    void setAllIndices() {
        _indices.resize(_source->size());
        for (size_t i = 0; i < _source->size(); ++i) {
            _indices[i] = i;
        }
        _rebuildLocalEntityIndex();
    }
    
    /**
     * @brief Filter by EntityId set
     * 
     * Creates indices for all entries whose EntityId is in the set.
     * 
     * @param entity_ids Set of EntityIds to include
     */
    template<typename EntityIdContainer>
    void filterByEntityIds(EntityIdContainer const& entity_ids) {
        _indices.clear();
        _indices.reserve(entity_ids.size()); // Approximate
        
        for (EntityId const& eid : entity_ids) {
            if (auto idx = _source->findByEntityId(eid)) {
                _indices.push_back(*idx);
            }
        }
        
        // Sort for cache-friendly access
        std::sort(_indices.begin(), _indices.end());
        _rebuildLocalEntityIndex();
    }
    
    /**
     * @brief Filter by time range [start, end] inclusive
     * 
     * @param start Start TimeFrameIndex (inclusive)
     * @param end End TimeFrameIndex (inclusive)
     */
    void filterByTimeRange(TimeFrameIndex start, TimeFrameIndex end) {
        _indices.clear();
        
        // Use time_ranges from source for efficiency
        for (auto const& [time, range] : _source->timeRanges()) {
            if (time >= start && time <= end) {
                for (size_t i = range.first; i < range.second; ++i) {
                    _indices.push_back(i);
                }
            }
        }
        
        _rebuildLocalEntityIndex();
    }
    
    /**
     * @brief Get the source storage
     */
    [[nodiscard]] std::shared_ptr<OwningRaggedStorage<TData> const> source() const {
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
    
    [[nodiscard]] TimeFrameIndex getTimeImpl(size_t idx) const {
        return _source->getTime(_indices[idx]);
    }
    
    [[nodiscard]] TData const& getDataImpl(size_t idx) const {
        return _source->getData(_indices[idx]);
    }
    
    [[nodiscard]] EntityId getEntityIdImpl(size_t idx) const {
        return _source->getEntityId(_indices[idx]);
    }
    
    [[nodiscard]] std::optional<size_t> findByEntityIdImpl(EntityId id) const {
        auto it = _local_entity_to_index.find(id);
        return it != _local_entity_to_index.end() ? std::optional{it->second} : std::nullopt;
    }
    
    [[nodiscard]] std::pair<size_t, size_t> getTimeRangeImpl(TimeFrameIndex time) const {
        auto it = _local_time_ranges.find(time);
        return it != _local_time_ranges.end() ? it->second : std::pair<size_t, size_t>{0, 0};
    }
    
    [[nodiscard]] size_t getTimeCountImpl() const { return _local_time_ranges.size(); }
    
    [[nodiscard]] RaggedStorageType getStorageTypeImpl() const { return RaggedStorageType::View; }

    /**
     * @brief Return invalid cache (views are non-contiguous)
     * 
     * ViewRaggedStorage accesses source data through an indirection array,
     * so contiguous pointer access is not possible. Returns an invalid cache
     * to signal that callers must use virtual dispatch for element access.
     */
    [[nodiscard]] RaggedStorageCache<TData> tryGetCacheImpl() const {
        // Views cannot provide contiguous access - return invalid cache
        return RaggedStorageCache<TData>{};
    }

private:
    void _rebuildLocalEntityIndex() {
        _local_entity_to_index.clear();
        _local_time_ranges.clear();
        
        for (size_t i = 0; i < _indices.size(); ++i) {
            size_t const src_idx = _indices[i];
            _local_entity_to_index[_source->getEntityId(src_idx)] = i;
            
            TimeFrameIndex const time = _source->getTime(src_idx);
            auto it = _local_time_ranges.find(time);
            if (it == _local_time_ranges.end()) {
                _local_time_ranges[time] = {i, i + 1};
            } else {
                it->second.second = i + 1;
            }
        }
    }
    
    std::shared_ptr<OwningRaggedStorage<TData> const> _source;
    std::vector<size_t> _indices;
    std::unordered_map<EntityId, size_t> _local_entity_to_index;
    std::map<TimeFrameIndex, std::pair<size_t, size_t>> _local_time_ranges;
};

// =============================================================================
// Storage Variant (Type-Erased Wrapper)
// =============================================================================

/**
 * @brief Type-erased storage wrapper using std::variant
 * 
 * Provides a unified interface for both owning and view storage.
 * Uses std::visit for dispatch, which the compiler can optimize well
 * when all types are known at compile time.
 * 
 * Performance characteristics (vs virtual dispatch):
 * - 2× faster for lightweight data (Point2D)
 * - ~1.05× faster for heavy data (Mask2D)
 * - 1.47× faster for EntityId lookups
 * 
 * @tparam TData The data type stored (e.g., Mask2D, Line2D, Point2D<float>)
 */
template<typename TData>
class RaggedStorageVariant {
public:
    using OwningType = OwningRaggedStorage<TData>;
    using ViewType = ViewRaggedStorage<TData>;
    using VariantType = std::variant<OwningType, ViewType>;
    
    /**
     * @brief Default construct with empty owning storage
     */
    RaggedStorageVariant() : _storage(OwningType{}) {}
    
    /**
     * @brief Construct from owning storage (move)
     */
    explicit RaggedStorageVariant(OwningType storage) 
        : _storage(std::move(storage)) {}
    
    /**
     * @brief Construct from view storage (move)
     */
    explicit RaggedStorageVariant(ViewType storage)
        : _storage(std::move(storage)) {}
    
    // ========== Unified Interface ==========
    
    [[nodiscard]] size_t size() const {
        return std::visit([](auto const& s) { return s.size(); }, _storage);
    }
    
    [[nodiscard]] bool empty() const {
        return std::visit([](auto const& s) { return s.empty(); }, _storage);
    }
    
    [[nodiscard]] TimeFrameIndex getTime(size_t idx) const {
        return std::visit([idx](auto const& s) { return s.getTime(idx); }, _storage);
    }
    
    [[nodiscard]] TData const& getData(size_t idx) const {
        return std::visit([idx](auto const& s) -> TData const& { 
            return s.getData(idx); 
        }, _storage);
    }
    
    [[nodiscard]] EntityId getEntityId(size_t idx) const {
        return std::visit([idx](auto const& s) { return s.getEntityId(idx); }, _storage);
    }
    
    [[nodiscard]] std::optional<size_t> findByEntityId(EntityId id) const {
        return std::visit([id](auto const& s) { return s.findByEntityId(id); }, _storage);
    }
    
    [[nodiscard]] std::pair<size_t, size_t> getTimeRange(TimeFrameIndex time) const {
        return std::visit([time](auto const& s) { return s.getTimeRange(time); }, _storage);
    }
    
    [[nodiscard]] size_t getTimeCount() const {
        return std::visit([](auto const& s) { return s.getTimeCount(); }, _storage);
    }
    
    [[nodiscard]] RaggedStorageType getStorageType() const {
        return std::visit([](auto const& s) { return s.getStorageType(); }, _storage);
    }
    
    [[nodiscard]] bool isView() const {
        return getStorageType() == RaggedStorageType::View;
    }
    
    // ========== Type-Specific Access ==========
    
    /**
     * @brief Check if storage is owning type
     */
    [[nodiscard]] bool isOwning() const {
        return std::holds_alternative<OwningType>(_storage);
    }
    
    /**
     * @brief Get owning storage if present
     * @return Pointer to owning storage, or nullptr if view
     */
    [[nodiscard]] OwningType* getOwning() {
        return std::get_if<OwningType>(&_storage);
    }
    
    [[nodiscard]] OwningType const* getOwning() const {
        return std::get_if<OwningType>(&_storage);
    }
    
    /**
     * @brief Get view storage if present
     * @return Pointer to view storage, or nullptr if owning
     */
    [[nodiscard]] ViewType* getView() {
        return std::get_if<ViewType>(&_storage);
    }
    
    [[nodiscard]] ViewType const* getView() const {
        return std::get_if<ViewType>(&_storage);
    }
    
    /**
     * @brief Access the underlying variant for advanced use
     */
    [[nodiscard]] VariantType& variant() { return _storage; }
    [[nodiscard]] VariantType const& variant() const { return _storage; }
    
    /**
     * @brief Apply a visitor to the storage
     */
    template<typename Visitor>
    decltype(auto) visit(Visitor&& visitor) {
        return std::visit(std::forward<Visitor>(visitor), _storage);
    }
    
    template<typename Visitor>
    decltype(auto) visit(Visitor&& visitor) const {
        return std::visit(std::forward<Visitor>(visitor), _storage);
    }

private:
    VariantType _storage;
};

// =============================================================================
// Type-Erased Storage Wrapper (Virtual Dispatch)
// =============================================================================

/**
 * @brief Type-erased storage wrapper using virtual dispatch
 * 
 * This wrapper provides a uniform interface for any storage backend while
 * hiding the concrete storage type. Unlike RaggedStorageVariant (which uses
 * std::variant and requires a closed set of types), this wrapper can hold
 * any storage type including future lazy transform storage that has unbounded
 * template parameters.
 * 
 * The trade-off is virtual dispatch overhead per access. However, the
 * tryGetCache() optimization allows iterators to bypass virtual dispatch
 * when storage is contiguous (OwningRaggedStorage), achieving zero-overhead
 * iteration for the common case.
 * 
 * Design rationale (from AnalogTimeSeries::DataStorageWrapper):
 * - Lazy transforms like `LazyViewStorage<ViewType>` cannot be stored in
 *   std::variant because ViewType is unbounded
 * - Type erasure allows open extension without modifying the wrapper
 * - Cache optimization mitigates virtual dispatch overhead for hot paths
 * 
 * @tparam TData The data type stored (e.g., Mask2D, Line2D, Point2D<float>)
 */
template<typename TData>
class RaggedStorageWrapper {
public:
    /**
     * @brief Construct wrapper from any storage implementation
     * 
     * The storage is moved into a heap-allocated wrapper that provides
     * virtual dispatch to the actual storage methods.
     * 
     * @tparam StorageImpl Concrete storage type (must satisfy storage interface)
     * @param storage Storage implementation (will be moved)
     */
    template<typename StorageImpl>
    explicit RaggedStorageWrapper(StorageImpl storage)
        : _impl(std::make_unique<StorageModel<StorageImpl>>(std::move(storage))) {}

    // Default constructor creates empty owning storage
    RaggedStorageWrapper()
        : _impl(std::make_unique<StorageModel<OwningRaggedStorage<TData>>>(
              OwningRaggedStorage<TData>{})) {}

    // Move-only semantics (unique_ptr member)
    RaggedStorageWrapper(RaggedStorageWrapper&&) noexcept = default;
    RaggedStorageWrapper& operator=(RaggedStorageWrapper&&) noexcept = default;
    RaggedStorageWrapper(RaggedStorageWrapper const&) = delete;
    RaggedStorageWrapper& operator=(RaggedStorageWrapper const&) = delete;

    // ========== Unified Interface (Virtual Dispatch) ==========
    
    [[nodiscard]] size_t size() const { 
        return _impl->size(); 
    }
    
    [[nodiscard]] bool empty() const { 
        return _impl->size() == 0; 
    }

    [[nodiscard]] TimeFrameIndex getTime(size_t idx) const {
        return _impl->getTime(idx);
    }

    [[nodiscard]] TData const& getData(size_t idx) const {
        return _impl->getData(idx);
    }

    [[nodiscard]] EntityId getEntityId(size_t idx) const {
        return _impl->getEntityId(idx);
    }

    [[nodiscard]] std::optional<size_t> findByEntityId(EntityId id) const {
        return _impl->findByEntityId(id);
    }

    [[nodiscard]] std::pair<size_t, size_t> getTimeRange(TimeFrameIndex time) const {
        return _impl->getTimeRange(time);
    }

    [[nodiscard]] size_t getTimeCount() const {
        return _impl->getTimeCount();
    }

    [[nodiscard]] RaggedStorageType getStorageType() const {
        return _impl->getStorageType();
    }

    [[nodiscard]] bool isView() const {
        return getStorageType() == RaggedStorageType::View;
    }

    // ========== Cache Optimization ==========
    
    /**
     * @brief Try to get cached pointers for fast-path iteration
     * 
     * If the underlying storage is contiguous, returns a valid cache
     * with direct pointers for zero-overhead iteration. Otherwise,
     * returns an invalid cache and callers must use virtual dispatch.
     * 
     * @return RaggedStorageCache<TData> Valid if contiguous, invalid otherwise
     */
    [[nodiscard]] RaggedStorageCache<TData> tryGetCache() const {
        return _impl->tryGetCache();
    }

    // ========== Type Access ==========
    
    /**
     * @brief Try to get underlying storage as specific type
     * 
     * Returns nullptr if the underlying storage is not the requested type.
     * Use sparingly - prefer the virtual interface for most operations.
     * 
     * @tparam StorageType Expected concrete storage type
     * @return Pointer to underlying storage, or nullptr
     */
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

private:
    /**
     * @brief Abstract interface for storage operations (type-erased)
     */
    struct StorageConcept {
        virtual ~StorageConcept() = default;
        
        // Size & bounds
        virtual size_t size() const = 0;
        
        // Element access
        virtual TimeFrameIndex getTime(size_t idx) const = 0;
        virtual TData const& getData(size_t idx) const = 0;
        virtual EntityId getEntityId(size_t idx) const = 0;
        
        // Lookups
        virtual std::optional<size_t> findByEntityId(EntityId id) const = 0;
        virtual std::pair<size_t, size_t> getTimeRange(TimeFrameIndex time) const = 0;
        virtual size_t getTimeCount() const = 0;
        
        // Type identification
        virtual RaggedStorageType getStorageType() const = 0;
        
        // Cache optimization
        virtual RaggedStorageCache<TData> tryGetCache() const = 0;
    };

    /**
     * @brief Concrete storage model wrapping a specific implementation
     * 
     * @tparam StorageImpl Concrete storage type (OwningRaggedStorage, ViewRaggedStorage, etc.)
     */
    template<typename StorageImpl>
    struct StorageModel : StorageConcept {
        StorageImpl _storage;

        explicit StorageModel(StorageImpl storage)
            : _storage(std::move(storage)) {}

        size_t size() const override {
            return _storage.size();
        }

        TimeFrameIndex getTime(size_t idx) const override {
            return _storage.getTime(idx);
        }

        TData const& getData(size_t idx) const override {
            return _storage.getData(idx);
        }

        EntityId getEntityId(size_t idx) const override {
            return _storage.getEntityId(idx);
        }

        std::optional<size_t> findByEntityId(EntityId id) const override {
            return _storage.findByEntityId(id);
        }

        std::pair<size_t, size_t> getTimeRange(TimeFrameIndex time) const override {
            return _storage.getTimeRange(time);
        }

        size_t getTimeCount() const override {
            return _storage.getTimeCount();
        }

        RaggedStorageType getStorageType() const override {
            return _storage.getStorageType();
        }

        RaggedStorageCache<TData> tryGetCache() const override {
            // Use if-constexpr to call tryGetCache if it exists,
            // otherwise return invalid cache
            if constexpr (requires { _storage.tryGetCache(); }) {
                return _storage.tryGetCache();
            } else {
                return RaggedStorageCache<TData>{};
            }
        }
    };

    std::unique_ptr<StorageConcept> _impl;
};

#endif // RAGGED_STORAGE_HPP
