#ifndef RAGGED_ANALOG_STORAGE_HPP
#define RAGGED_ANALOG_STORAGE_HPP

#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

// Forward declarations
class OwningRaggedAnalogStorage;
class ViewRaggedAnalogStorage;

/**
 * @brief Storage type enumeration for ragged analog storage
 */
enum class RaggedAnalogStorageType {
    Owning,  ///< Owns the data in SoA layout
    View,    ///< References another storage via indices
    Lazy     ///< Lazy-evaluated transform
};

// =============================================================================
// Cache Optimization Structure
// =============================================================================

/**
 * @brief Cache structure for fast-path access to contiguous analog storage
 * 
 * Unlike RaggedTimeSeries which has EntityIds, RaggedAnalogTimeSeries
 * only needs time and value pointers. The storage is organized as:
 * - times[i] - TimeFrameIndex for entry i
 * - values[i] - float value at entry i
 * 
 * @note This is simpler than RaggedStorageCache<TData> since we don't
 *       need EntityId support.
 */
struct RaggedAnalogStorageCache {
    TimeFrameIndex const* times_ptr = nullptr;
    float const* values_ptr = nullptr;
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
    [[nodiscard]] TimeFrameIndex getTime(size_t idx) const noexcept {
        return times_ptr[idx];
    }
    
    [[nodiscard]] float getValue(size_t idx) const noexcept {
        return values_ptr[idx];
    }
};

// =============================================================================
// CRTP Base Class
// =============================================================================

/**
 * @brief CRTP base class for ragged analog storage implementations
 * 
 * Uses Curiously Recurring Template Pattern to eliminate virtual function overhead
 * while maintaining a polymorphic interface. This is specialized for float data
 * without EntityId support.
 * 
 * The SoA (Structure of Arrays) layout stores parallel vectors:
 * - TimeFrameIndex times[]
 * - float values[]
 * 
 * Time ranges are stored as a map from TimeFrameIndex to (start, end) index pairs,
 * where each time can have multiple float values.
 * 
 * @tparam Derived The concrete storage implementation type
 */
template<typename Derived>
class RaggedAnalogStorageBase {
public:
    // ========== Size & Bounds ==========
    
    /**
     * @brief Get total number of float entries across all times
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
     * @brief Get the float value at a flat index
     * @param idx Flat index in [0, size())
     */
    [[nodiscard]] float getValue(size_t idx) const {
        return static_cast<Derived const*>(this)->getValueImpl(idx);
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
    
    /**
     * @brief Check if data exists at a specific time
     */
    [[nodiscard]] bool hasDataAtTime(TimeFrameIndex time) const {
        auto [start, end] = getTimeRange(time);
        return start < end;
    }
    
    /**
     * @brief Get values at a specific time as a span
     * 
     * @param time The time to query
     * @return span of float values, empty if no data at this time
     */
    [[nodiscard]] std::span<float const> getValuesAtTime(TimeFrameIndex time) const {
        return static_cast<Derived const*>(this)->getValuesAtTimeImpl(time);
    }

    // ========== Storage Type ==========
    
    /**
     * @brief Get the storage type identifier
     */
    [[nodiscard]] RaggedAnalogStorageType getStorageType() const {
        return static_cast<Derived const*>(this)->getStorageTypeImpl();
    }
    
    /**
     * @brief Check if this is a view (doesn't own data)
     */
    [[nodiscard]] bool isView() const {
        return getStorageType() == RaggedAnalogStorageType::View;
    }
    
    /**
     * @brief Check if this is lazy storage
     */
    [[nodiscard]] bool isLazy() const {
        return getStorageType() == RaggedAnalogStorageType::Lazy;
    }

    // ========== Cache Optimization ==========
    
    /**
     * @brief Try to get cached pointers for fast-path access
     * 
     * @return RaggedAnalogStorageCache with valid pointers if contiguous, invalid otherwise
     */
    [[nodiscard]] RaggedAnalogStorageCache tryGetCache() const {
        return static_cast<Derived const*>(this)->tryGetCacheImpl();
    }

protected:
    ~RaggedAnalogStorageBase() = default;
};

// =============================================================================
// Owning Storage (SoA Layout)
// =============================================================================

/**
 * @brief Owning ragged analog storage using Structure of Arrays layout
 * 
 * Stores float data in parallel vectors for cache-friendly access:
 * - _times[i] - TimeFrameIndex for entry i
 * - _values[i] - float value for entry i
 * 
 * Maintains acceleration structures:
 * - _time_ranges: O(log n) time range lookup
 * 
 * Unlike RaggedTimeSeries storage, this doesn't track EntityIds since
 * RaggedAnalogTimeSeries doesn't use them.
 */
class OwningRaggedAnalogStorage : public RaggedAnalogStorageBase<OwningRaggedAnalogStorage> {
public:
    OwningRaggedAnalogStorage() = default;
    
    // ========== Modification ==========
    
    /**
     * @brief Append a single float value at a specific time
     * 
     * @param time The TimeFrameIndex for this entry
     * @param value The float value to store
     */
    void append(TimeFrameIndex time, float value) {
        size_t const idx = _times.size();
        
        _times.push_back(time);
        _values.push_back(value);
        
        _updateTimeRanges(time, idx);
    }
    
    /**
     * @brief Append multiple float values at a specific time
     * 
     * More efficient than calling append() multiple times.
     * 
     * @param time The TimeFrameIndex for these entries
     * @param values Vector of float values to store
     */
    void appendBatch(TimeFrameIndex time, std::vector<float> const& values) {
        if (values.empty()) return;
        
        size_t const start_idx = _times.size();
        _times.reserve(_times.size() + values.size());
        _values.reserve(_values.size() + values.size());
        
        for (float v : values) {
            _times.push_back(time);
            _values.push_back(v);
        }
        
        // Update time ranges once for the whole batch
        auto it = _time_ranges.find(time);
        if (it == _time_ranges.end()) {
            _time_ranges[time] = {start_idx, _times.size()};
        } else {
            it->second.second = _times.size();
        }
    }
    
    /**
     * @brief Append multiple float values at a specific time (move version)
     */
    void appendBatch(TimeFrameIndex time, std::vector<float>&& values) {
        if (values.empty()) return;
        
        size_t const start_idx = _times.size();
        _times.reserve(_times.size() + values.size());
        _values.reserve(_values.size() + values.size());
        
        for (float v : values) {
            _times.push_back(time);
            _values.push_back(v);
        }
        
        auto it = _time_ranges.find(time);
        if (it == _time_ranges.end()) {
            _time_ranges[time] = {start_idx, _times.size()};
        } else {
            it->second.second = _times.size();
        }
    }
    
    /**
     * @brief Set/replace all data at a specific time
     * 
     * If data already exists at this time, it will be replaced.
     * This is less efficient than append when building from scratch.
     * 
     * @param time The TimeFrameIndex to set data for
     * @param values Vector of float values
     */
    void setAtTime(TimeFrameIndex time, std::vector<float> const& values) {
        // If time already exists, we need to rebuild
        auto it = _time_ranges.find(time);
        if (it != _time_ranges.end()) {
            // Remove existing data at this time
            removeAtTime(time);
        }
        
        // Append the new data
        appendBatch(time, values);
    }
    
    /**
     * @brief Remove all entries at a specific time
     * @param time The TimeFrameIndex to remove all entries for
     * @return Number of entries removed
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
        _values.erase(_values.begin() + static_cast<std::ptrdiff_t>(start),
                      _values.begin() + static_cast<std::ptrdiff_t>(end));
        
        // Rebuild acceleration structures
        _rebuildTimeRanges();
        
        return count;
    }
    
    /**
     * @brief Reserve capacity for expected number of entries
     */
    void reserve(size_t capacity) {
        _times.reserve(capacity);
        _values.reserve(capacity);
    }
    
    /**
     * @brief Clear all data
     */
    void clear() {
        _times.clear();
        _values.clear();
        _time_ranges.clear();
    }

    // ========== CRTP Implementation ==========
    
    [[nodiscard]] size_t sizeImpl() const { return _times.size(); }
    
    [[nodiscard]] TimeFrameIndex getTimeImpl(size_t idx) const { return _times[idx]; }
    
    [[nodiscard]] float getValueImpl(size_t idx) const { return _values[idx]; }
    
    [[nodiscard]] std::pair<size_t, size_t> getTimeRangeImpl(TimeFrameIndex time) const {
        auto it = _time_ranges.find(time);
        return it != _time_ranges.end() ? it->second : std::pair<size_t, size_t>{0, 0};
    }
    
    [[nodiscard]] size_t getTimeCountImpl() const { return _time_ranges.size(); }
    
    [[nodiscard]] RaggedAnalogStorageType getStorageTypeImpl() const { 
        return RaggedAnalogStorageType::Owning; 
    }
    
    [[nodiscard]] std::span<float const> getValuesAtTimeImpl(TimeFrameIndex time) const {
        auto it = _time_ranges.find(time);
        if (it == _time_ranges.end()) {
            return {};
        }
        auto [start, end] = it->second;
        return std::span<float const>(_values.data() + start, end - start);
    }

    /**
     * @brief Get cache with pointers to contiguous data
     * 
     * OwningRaggedAnalogStorage stores data contiguously in SoA layout,
     * so it always returns a valid cache for fast-path iteration.
     */
    [[nodiscard]] RaggedAnalogStorageCache tryGetCacheImpl() const {
        return RaggedAnalogStorageCache{
            _times.data(),
            _values.data(),
            _times.size(),
            true  // is_contiguous
        };
    }

    // ========== Direct Array Access ==========
    
    [[nodiscard]] std::vector<TimeFrameIndex> const& times() const { return _times; }
    [[nodiscard]] std::vector<float> const& values() const { return _values; }
    
    [[nodiscard]] std::span<TimeFrameIndex const> timesSpan() const { return _times; }
    [[nodiscard]] std::span<float const> valuesSpan() const { return _values; }
    
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
            _time_ranges[time] = {idx, idx + 1};
        } else {
            it->second.second = idx + 1;
        }
    }
    
    void _rebuildTimeRanges() {
        _time_ranges.clear();
        for (size_t i = 0; i < _times.size(); ++i) {
            _updateTimeRanges(_times[i], i);
        }
    }

    std::vector<TimeFrameIndex> _times;
    std::vector<float> _values;
    std::map<TimeFrameIndex, std::pair<size_t, size_t>> _time_ranges;
};

// =============================================================================
// Lazy Storage (View-based Computation on Demand)
// =============================================================================

/**
 * @brief Lazy ragged analog storage that computes values on-demand from a view
 * 
 * Stores a computation pipeline as a random-access view that transforms data
 * on-demand. Enables efficient composition of transforms without materializing
 * intermediate results.
 * 
 * The view must yield pairs of (TimeFrameIndex, float).
 * 
 * @tparam ViewType Type of the random-access range view
 */
template<typename ViewType>
class LazyRaggedAnalogStorage : public RaggedAnalogStorageBase<LazyRaggedAnalogStorage<ViewType>> {
public:
    /**
     * @brief Construct lazy storage from a random-access view
     * 
     * @param view Random-access range view yielding (TimeFrameIndex, float) pairs
     * @param num_elements Number of elements in the view
     */
    explicit LazyRaggedAnalogStorage(ViewType view, size_t num_elements)
        : _view(std::move(view))
        , _num_elements(num_elements) {
        static_assert(std::ranges::random_access_range<ViewType>,
                      "LazyRaggedAnalogStorage requires random access range");
        _buildLocalIndices();
    }

    virtual ~LazyRaggedAnalogStorage() = default;

    // ========== CRTP Implementation ==========

    [[nodiscard]] size_t sizeImpl() const { return _num_elements; }

    [[nodiscard]] TimeFrameIndex getTimeImpl(size_t idx) const {
        auto element = _view[idx];
        if constexpr (requires { element.first; }) {
            return element.first;
        } else {
            return std::get<0>(element);
        }
    }

    [[nodiscard]] float getValueImpl(size_t idx) const {
        auto element = _view[idx];
        if constexpr (requires { element.second; }) {
            return element.second;
        } else {
            return std::get<1>(element);
        }
    }

    [[nodiscard]] std::pair<size_t, size_t> getTimeRangeImpl(TimeFrameIndex time) const {
        auto it = _time_ranges.find(time);
        return it != _time_ranges.end() ? it->second : std::pair<size_t, size_t>{0, 0};
    }

    [[nodiscard]] size_t getTimeCountImpl() const { return _time_ranges.size(); }

    [[nodiscard]] RaggedAnalogStorageType getStorageTypeImpl() const { 
        return RaggedAnalogStorageType::Lazy; 
    }
    
    [[nodiscard]] std::span<float const> getValuesAtTimeImpl(TimeFrameIndex time) const {
        // Lazy storage can't return a span - would need to materialize
        // Return empty span; caller should iterate using getValue() instead
        (void)time;
        return {};
    }

    /**
     * @brief Lazy storage is never contiguous in memory
     * 
     * Returns an invalid cache, forcing callers to use virtual dispatch.
     */
    [[nodiscard]] RaggedAnalogStorageCache tryGetCacheImpl() const {
        return RaggedAnalogStorageCache{};  // Invalid cache
    }

    /**
     * @brief Get reference to underlying view
     */
    [[nodiscard]] ViewType const& getView() const {
        return _view;
    }

private:
    /**
     * @brief Build local time range indices on construction
     */
    void _buildLocalIndices() {
        _time_ranges.clear();
        
        for (size_t i = 0; i < _num_elements; ++i) {
            TimeFrameIndex time = getTimeImpl(i);
            
            auto it = _time_ranges.find(time);
            if (it == _time_ranges.end()) {
                _time_ranges[time] = {i, i + 1};
            } else {
                it->second.second = i + 1;
            }
        }
    }

    ViewType _view;
    size_t _num_elements;
    std::map<TimeFrameIndex, std::pair<size_t, size_t>> _time_ranges;
};

// =============================================================================
// View Storage (References Source via Indices)
// =============================================================================

/**
 * @brief View-based ragged analog storage that references another storage
 * 
 * Holds a shared_ptr to a source OwningRaggedAnalogStorage and a vector of indices
 * into that source. Enables zero-copy filtered views.
 */
class ViewRaggedAnalogStorage : public RaggedAnalogStorageBase<ViewRaggedAnalogStorage> {
public:
    /**
     * @brief Construct a view referencing source storage
     * 
     * @param source Shared pointer to source storage
     */
    explicit ViewRaggedAnalogStorage(std::shared_ptr<OwningRaggedAnalogStorage const> source)
        : _source(std::move(source)) {}
    
    /**
     * @brief Set the indices this view includes
     */
    void setIndices(std::vector<size_t> indices) {
        _indices = std::move(indices);
        _rebuildLocalTimeRanges();
    }
    
    /**
     * @brief Create view of all entries
     */
    void setAllIndices() {
        _indices.resize(_source->size());
        for (size_t i = 0; i < _source->size(); ++i) {
            _indices[i] = i;
        }
        _rebuildLocalTimeRanges();
    }
    
    /**
     * @brief Filter by time range [start, end] inclusive
     */
    void filterByTimeRange(TimeFrameIndex start, TimeFrameIndex end) {
        _indices.clear();
        
        for (auto const& [time, range] : _source->timeRanges()) {
            if (time >= start && time <= end) {
                for (size_t i = range.first; i < range.second; ++i) {
                    _indices.push_back(i);
                }
            }
        }
        
        _rebuildLocalTimeRanges();
    }
    
    /**
     * @brief Get the source storage
     */
    [[nodiscard]] std::shared_ptr<OwningRaggedAnalogStorage const> source() const {
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
    
    [[nodiscard]] float getValueImpl(size_t idx) const {
        return _source->getValue(_indices[idx]);
    }
    
    [[nodiscard]] std::pair<size_t, size_t> getTimeRangeImpl(TimeFrameIndex time) const {
        auto it = _local_time_ranges.find(time);
        return it != _local_time_ranges.end() ? it->second : std::pair<size_t, size_t>{0, 0};
    }
    
    [[nodiscard]] size_t getTimeCountImpl() const { return _local_time_ranges.size(); }
    
    [[nodiscard]] RaggedAnalogStorageType getStorageTypeImpl() const { 
        return RaggedAnalogStorageType::View; 
    }
    
    [[nodiscard]] std::span<float const> getValuesAtTimeImpl(TimeFrameIndex time) const {
        // Only return span if indices are contiguous for this time range
        auto it = _local_time_ranges.find(time);
        if (it == _local_time_ranges.end()) {
            return {};
        }
        
        auto [start, end] = it->second;
        if (start >= end) return {};
        
        // Check if the view indices for this time are contiguous in source
        size_t const src_start = _indices[start];
        bool contiguous = true;
        for (size_t i = start + 1; i < end; ++i) {
            if (_indices[i] != src_start + (i - start)) {
                contiguous = false;
                break;
            }
        }
        
        if (contiguous) {
            return std::span<float const>(_source->values().data() + src_start, end - start);
        }
        
        return {};  // Non-contiguous
    }

    /**
     * @brief Return cache if view is contiguous
     */
    [[nodiscard]] RaggedAnalogStorageCache tryGetCacheImpl() const {
        if (_indices.empty()) {
            return RaggedAnalogStorageCache{nullptr, nullptr, 0, true};
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
            return RaggedAnalogStorageCache{
                _source->times().data() + start_idx,
                _source->values().data() + start_idx,
                _indices.size(),
                true
            };
        }
        
        return RaggedAnalogStorageCache{};  // Invalid
    }

private:
    void _rebuildLocalTimeRanges() {
        _local_time_ranges.clear();
        
        for (size_t i = 0; i < _indices.size(); ++i) {
            TimeFrameIndex const time = _source->getTime(_indices[i]);
            auto it = _local_time_ranges.find(time);
            if (it == _local_time_ranges.end()) {
                _local_time_ranges[time] = {i, i + 1};
            } else {
                it->second.second = i + 1;
            }
        }
    }
    
    std::shared_ptr<OwningRaggedAnalogStorage const> _source;
    std::vector<size_t> _indices;
    std::map<TimeFrameIndex, std::pair<size_t, size_t>> _local_time_ranges;
};

// =============================================================================
// Type-Erased Storage Wrapper (Virtual Dispatch)
// =============================================================================

/**
 * @brief Type-erased storage wrapper for ragged analog storage
 * 
 * Provides a uniform interface for any storage backend while hiding the
 * concrete storage type. Supports lazy transforms with unbounded ViewType.
 */
class RaggedAnalogStorageWrapper {
public:
    /**
     * @brief Construct wrapper from any storage implementation
     */
    template<typename StorageImpl>
    explicit RaggedAnalogStorageWrapper(StorageImpl storage)
        : _impl(std::make_unique<StorageModel<StorageImpl>>(std::move(storage))) {}

    // Default constructor creates empty owning storage
    RaggedAnalogStorageWrapper()
        : _impl(std::make_unique<StorageModel<OwningRaggedAnalogStorage>>(
              OwningRaggedAnalogStorage{})) {}

    // Move-only semantics
    RaggedAnalogStorageWrapper(RaggedAnalogStorageWrapper&&) noexcept = default;
    RaggedAnalogStorageWrapper& operator=(RaggedAnalogStorageWrapper&&) noexcept = default;
    RaggedAnalogStorageWrapper(RaggedAnalogStorageWrapper const&) = delete;
    RaggedAnalogStorageWrapper& operator=(RaggedAnalogStorageWrapper const&) = delete;

    // ========== Unified Interface ==========
    
    [[nodiscard]] size_t size() const { return _impl->size(); }
    [[nodiscard]] bool empty() const { return _impl->size() == 0; }

    [[nodiscard]] TimeFrameIndex getTime(size_t idx) const {
        return _impl->getTime(idx);
    }

    [[nodiscard]] float getValue(size_t idx) const {
        return _impl->getValue(idx);
    }

    [[nodiscard]] std::pair<size_t, size_t> getTimeRange(TimeFrameIndex time) const {
        return _impl->getTimeRange(time);
    }

    [[nodiscard]] size_t getTimeCount() const {
        return _impl->getTimeCount();
    }
    
    [[nodiscard]] bool hasDataAtTime(TimeFrameIndex time) const {
        auto [start, end] = getTimeRange(time);
        return start < end;
    }
    
    [[nodiscard]] std::span<float const> getValuesAtTime(TimeFrameIndex time) const {
        return _impl->getValuesAtTime(time);
    }

    [[nodiscard]] RaggedAnalogStorageType getStorageType() const {
        return _impl->getStorageType();
    }

    [[nodiscard]] bool isView() const {
        return getStorageType() == RaggedAnalogStorageType::View;
    }
    
    [[nodiscard]] bool isLazy() const {
        return getStorageType() == RaggedAnalogStorageType::Lazy;
    }

    // ========== Cache Optimization ==========
    
    [[nodiscard]] RaggedAnalogStorageCache tryGetCache() const {
        return _impl->tryGetCache();
    }

    // ========== Mutation Operations ==========
    
    void append(TimeFrameIndex time, float value) {
        _impl->append(time, value);
    }

    void appendBatch(TimeFrameIndex time, std::vector<float> const& values) {
        _impl->appendBatch(time, values);
    }
    
    void appendBatch(TimeFrameIndex time, std::vector<float>&& values) {
        _impl->appendBatchMove(time, std::move(values));
    }
    
    void setAtTime(TimeFrameIndex time, std::vector<float> const& values) {
        _impl->setAtTime(time, values);
    }

    size_t removeAtTime(TimeFrameIndex time) {
        return _impl->removeAtTime(time);
    }

    void reserve(size_t capacity) {
        _impl->reserve(capacity);
    }

    void clear() {
        _impl->clear();
    }
    
    /**
     * @brief Get the time ranges map (owning storage only)
     */
    [[nodiscard]] std::map<TimeFrameIndex, std::pair<size_t, size_t>> const& timeRanges() const {
        return _impl->timeRanges();
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

private:
    struct StorageConcept {
        virtual ~StorageConcept() = default;
        
        virtual size_t size() const = 0;
        virtual TimeFrameIndex getTime(size_t idx) const = 0;
        virtual float getValue(size_t idx) const = 0;
        virtual std::pair<size_t, size_t> getTimeRange(TimeFrameIndex time) const = 0;
        virtual size_t getTimeCount() const = 0;
        virtual std::span<float const> getValuesAtTime(TimeFrameIndex time) const = 0;
        virtual RaggedAnalogStorageType getStorageType() const = 0;
        virtual RaggedAnalogStorageCache tryGetCache() const = 0;
        
        // Mutation
        virtual void append(TimeFrameIndex time, float value) = 0;
        virtual void appendBatch(TimeFrameIndex time, std::vector<float> const& values) = 0;
        virtual void appendBatchMove(TimeFrameIndex time, std::vector<float>&& values) = 0;
        virtual void setAtTime(TimeFrameIndex time, std::vector<float> const& values) = 0;
        virtual size_t removeAtTime(TimeFrameIndex time) = 0;
        virtual void reserve(size_t capacity) = 0;
        virtual void clear() = 0;
        virtual std::map<TimeFrameIndex, std::pair<size_t, size_t>> const& timeRanges() const = 0;
    };

    template<typename StorageImpl>
    struct StorageModel : StorageConcept {
        StorageImpl _storage;

        explicit StorageModel(StorageImpl storage)
            : _storage(std::move(storage)) {}

        size_t size() const override { return _storage.size(); }
        
        TimeFrameIndex getTime(size_t idx) const override {
            return _storage.getTime(idx);
        }
        
        float getValue(size_t idx) const override {
            return _storage.getValue(idx);
        }
        
        std::pair<size_t, size_t> getTimeRange(TimeFrameIndex time) const override {
            return _storage.getTimeRange(time);
        }
        
        size_t getTimeCount() const override {
            return _storage.getTimeCount();
        }
        
        std::span<float const> getValuesAtTime(TimeFrameIndex time) const override {
            return _storage.getValuesAtTime(time);
        }
        
        RaggedAnalogStorageType getStorageType() const override {
            return _storage.getStorageType();
        }
        
        RaggedAnalogStorageCache tryGetCache() const override {
            if constexpr (requires { _storage.tryGetCache(); }) {
                return _storage.tryGetCache();
            } else {
                return RaggedAnalogStorageCache{};
            }
        }

        // Mutation - only for OwningRaggedAnalogStorage
        void append(TimeFrameIndex time, float value) override {
            if constexpr (requires { _storage.append(time, value); }) {
                _storage.append(time, value);
            } else {
                throw std::runtime_error("append() not supported for view/lazy storage");
            }
        }
        
        void appendBatch(TimeFrameIndex time, std::vector<float> const& values) override {
            if constexpr (requires { _storage.appendBatch(time, values); }) {
                _storage.appendBatch(time, values);
            } else {
                throw std::runtime_error("appendBatch() not supported for view/lazy storage");
            }
        }
        
        void appendBatchMove(TimeFrameIndex time, std::vector<float>&& values) override {
            if constexpr (requires { _storage.appendBatch(time, std::move(values)); }) {
                _storage.appendBatch(time, std::move(values));
            } else {
                throw std::runtime_error("appendBatch() not supported for view/lazy storage");
            }
        }
        
        void setAtTime(TimeFrameIndex time, std::vector<float> const& values) override {
            if constexpr (requires { _storage.setAtTime(time, values); }) {
                _storage.setAtTime(time, values);
            } else {
                throw std::runtime_error("setAtTime() not supported for view/lazy storage");
            }
        }
        
        size_t removeAtTime(TimeFrameIndex time) override {
            if constexpr (requires { _storage.removeAtTime(time); }) {
                return _storage.removeAtTime(time);
            } else {
                throw std::runtime_error("removeAtTime() not supported for view/lazy storage");
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
        
        std::map<TimeFrameIndex, std::pair<size_t, size_t>> const& timeRanges() const override {
            if constexpr (requires { _storage.timeRanges(); }) {
                return _storage.timeRanges();
            } else {
                static std::map<TimeFrameIndex, std::pair<size_t, size_t>> const empty_map{};
                return empty_map;
            }
        }
    };

    std::unique_ptr<StorageConcept> _impl;
};

#endif // RAGGED_ANALOG_STORAGE_HPP
