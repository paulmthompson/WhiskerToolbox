#ifndef RAGGED_TIME_SERIES_HPP
#define RAGGED_TIME_SERIES_HPP

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "utils/map_timeseries.hpp"
#include "utils/RaggedStorage.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

template <typename TData> class RaggedTimeSeriesView;

/**
 * @brief Base template class for ragged time series data structures.
 *
 * RaggedTimeSeries provides a unified interface for time series data where
 * multiple unique entries can exist at each timestamp. Each entry has a unique
 * EntityId for tracking and manipulation.
 *
 * This base class manages:
 * - Time series storage as a map from TimeFrameIndex to vectors of DataEntry<TData>
 * - Image size metadata
 * - TimeFrame association
 * - Identity context (data key and EntityRegistry) for automatic EntityId management
 *
 * Derived classes (LineData, MaskData, PointData) specialize the template parameter
 * TData and provide domain-specific operations.
 *
 * @tparam TData The data type stored in each entry (e.g., Line2D, Mask2D, Point2D<float>)
 */
template <typename TData>
class RaggedTimeSeries : public ObserverData {
public:
    // ========== Constructors ==========
    RaggedTimeSeries() = default;

    virtual ~RaggedTimeSeries() = default;

    // Delete copy constructor and copy assignment
    RaggedTimeSeries(RaggedTimeSeries const &) = delete;
    RaggedTimeSeries & operator=(RaggedTimeSeries const &) = delete;

    /**
     * @brief Construct from a range of time-data pairs
     * 
     * Accepts any input range that yields pairs of (TimeFrameIndex, TData).
     * This enables efficient construction from transformed views, such as
     * pipeline outputs.
     * 
     * EntityIds will be automatically assigned during construction if an
     * identity context is set. Otherwise, EntityIds will be EntityId(0).
     * 
     * @tparam R Range type (must satisfy std::ranges::input_range)
     * @param time_data_pairs Range of (TimeFrameIndex, TData) pairs
     * 
     * @example
     * ```cpp
     * auto transformed = mask_data.elements() 
     *     | std::views::transform([](auto [time, entry]) { 
     *         return std::pair{time, skeletonize(entry.get().data)};
     *     });
     * auto new_masks = std::make_shared<MaskData>();
     * *new_masks = MaskData(transformed);  // Construct from view
     * ```
     */
    template<std::ranges::input_range R>
    requires requires(std::ranges::range_value_t<R> pair) {
        { pair.first } -> std::convertible_to<TimeFrameIndex>;
        { pair.second } -> std::convertible_to<TData>;
    }
    explicit RaggedTimeSeries(R&& time_data_pairs) : RaggedTimeSeries() {
        // Process the range, adding each element with automatic EntityId assignment
        for (auto&& [time, data] : time_data_pairs) {
            // Use addAtTime which handles EntityId assignment via identity context
            if constexpr (std::is_rvalue_reference_v<decltype(data)>) {
                addAtTime(time, std::forward<decltype(data)>(data), NotifyObservers::No);
            } else {
                addAtTime(time, data, NotifyObservers::No);
            }
        }
        
        // Single notification at end after all data is loaded
        if (!_storage.empty()) {
            notifyObservers();
        }
    }

    // ========== Time Frame ==========
    /**
     * @brief Set the time frame for this data structure
     * 
     * @param time_frame Shared pointer to the TimeFrame to associate with this data
     */
    void setTimeFrame(std::shared_ptr<TimeFrame> time_frame) { 
        _time_frame = std::move(time_frame); 
    }

    /**
     * @brief Get the current time frame
     * 
     * @return Shared pointer to the associated TimeFrame, or nullptr if none is set
     */
    [[nodiscard]] std::shared_ptr<TimeFrame> getTimeFrame() const {
        return _time_frame;
    }

    // ========== Image Size ==========
    /**
     * @brief Get the image size associated with this data
     * 
     * @return The current ImageSize
     */
    [[nodiscard]] ImageSize getImageSize() const { 
        return _image_size; 
    }

    /**
     * @brief Set the image size for this data
     * 
     * @param image_size The ImageSize to set
     */
    void setImageSize(ImageSize const & image_size) { 
        _image_size = image_size; 
    }

    // ========== Identity Context ==========
    /**
     * @brief Set identity context for automatic EntityId maintenance.
     * 
     * This establishes the connection to an EntityRegistry that will manage
     * EntityIds for all entries in this data structure.
     * 
     * @param data_key String key identifying this data in the EntityRegistry
     * @param registry Pointer to the EntityRegistry (must remain valid)
     * @pre registry != nullptr
     */
    void setIdentityContext(std::string const & data_key, EntityRegistry * registry) {
        _identity_data_key = data_key;
        _identity_registry = registry;
    }

    /**
     * @brief Rebuild EntityIds for all entries using the current identity context.
     * 
     * This method regenerates EntityIds for all data entries across all time frames.
     * If no identity context is set (_identity_registry is nullptr), all EntityIds
     * are reset to EntityId(0).
     * 
     * The EntityKind used depends on the derived class type:
     * - LineData uses EntityKind::LineEntity
     * - MaskData uses EntityKind::MaskEntity
     * - PointData uses EntityKind::PointEntity
     * 
     * @pre Identity context should be set via setIdentityContext before calling
     */
    void rebuildAllEntityIds() {
        // With SoA storage, we need to rebuild the entire storage with new EntityIds
        // This is an expensive operation - consider if it's really needed
        if (_storage.empty()) {
            return;
        }

        EntityKind const kind = getEntityKind();
        
        // Create a new storage and repopulate with correct EntityIds
        OwningRaggedStorage<TData> new_storage;
        new_storage.reserve(_storage.size());
        
        // Track local indices per time for EntityId generation
        std::map<TimeFrameIndex, int> time_local_indices;
        
        for (size_t i = 0; i < _storage.size(); ++i) {
            TimeFrameIndex const time = _storage.getTime(i);
            TData const& data = _storage.getData(i);
            
            int local_index = time_local_indices[time]++;
            EntityId entity_id = EntityId(0);
            if (_identity_registry) {
                entity_id = _identity_registry->ensureId(_identity_data_key, kind, time, local_index);
            }
            
            new_storage.append(time, data, entity_id);
        }
        
        _storage = std::move(new_storage);
    }

    // ========== Common Entity-based Operations ==========

    /**
     * @brief Add a data entry at a specific time with a specific entity ID
     *
     * This method is used internally for move operations to preserve entity IDs.
     * It directly adds an entry without generating a new EntityId.
     *
     * @param time The time to add the data at
     * @param data The data to add
     * @param entity_id The entity ID to assign to the entry
     * @param notify If true, observers will be notified of the change
     */
    void addEntryAtTime(TimeFrameIndex time, TData const & data, EntityId entity_id, NotifyObservers notify) {
        _storage.append(time, data, entity_id);
        if (notify == NotifyObservers::Yes) {
            notifyObservers();
        }
    }

    /**
     * @brief Copy entries with specific EntityIds to another RaggedTimeSeries
     *
     * Copies all entries that match the given EntityIds to the target.
     * The copied entries will get new EntityIds in the target based on the target's
     * identity context. Uses the copy_by_entity_ids helper function which properly
     * handles the differences in addAtTime APIs across derived classes.
     *
     * @tparam DerivedTarget The derived class type (LineData, MaskData, or PointData)
     * @param target The target to copy entries to
     * @param entity_ids Set of EntityIds to copy
     * @param notify If true, the target will notify its observers after the operation
     * @return The number of entries actually copied
     */
    template<typename DerivedTarget>
    std::size_t copyByEntityIds(DerivedTarget & target, 
                                std::unordered_set<EntityId> const & entity_ids, 
                                NotifyObservers notify) {
        std::size_t count = 0;
        // Iterate through storage in order to preserve ordering
        for (size_t i = 0; i < _storage.size(); ++i) {
            EntityId const eid = _storage.getEntityId(i);
            if (entity_ids.contains(eid)) {
                target.addAtTime(_storage.getTime(i), _storage.getData(i), NotifyObservers::No);
                ++count;
            }
        }
        if (notify == NotifyObservers::Yes && count > 0) {
            target.notifyObservers();
        }
        return count;
    }

    /**
     * @brief Move entries with specific EntityIds to another RaggedTimeSeries
     *
     * Moves all entries that match the given EntityIds to the target.
     * The moved entries will keep the same EntityIds in the target and be removed from source.
     * Uses the move_by_entity_ids helper function.
     *
     * @tparam DerivedTarget The derived class type (LineData, MaskData, or PointData)
     * @param target The target to move entries to
     * @param entity_ids Set of EntityIds to move
     * @param notify If true, both source and target will notify their observers after the operation
     * @return The number of entries actually moved
     */
    template<typename DerivedTarget>
    std::size_t moveByEntityIds(DerivedTarget & target, 
                                std::unordered_set<EntityId> const & entity_ids, 
                                NotifyObservers notify) {
        // Collect entries to move (iterate in order to preserve ordering)
        std::vector<std::tuple<TimeFrameIndex, TData, EntityId>> to_move;
        for (size_t i = 0; i < _storage.size(); ++i) {
            EntityId const eid = _storage.getEntityId(i);
            if (entity_ids.contains(eid)) {
                to_move.emplace_back(_storage.getTime(i), _storage.getData(i), eid);
            }
        }
        
        // Add to target (preserving EntityIds)
        for (auto const& [time, data, eid] : to_move) {
            target.addEntryAtTime(time, data, eid, NotifyObservers::No);
        }
        
        // Remove from source
        for (auto const& [time, data, eid] : to_move) {
            (void)time;
            (void)data;
            _storage.removeByEntityId(eid);
        }
        
        if (notify == NotifyObservers::Yes && !to_move.empty()) {
            target.notifyObservers();
            notifyObservers();
        }
        return to_move.size();
    }

    // ========== Entity Lookup Methods ==========

    /**
     * @brief Find the data associated with a specific EntityId.
     * 
     * This method provides reverse lookup from EntityId to the actual data,
     * supporting group-based visualization workflows.
     * 
     * @param entity_id The EntityId to look up
     * @return Optional containing a const reference to the data if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<std::reference_wrapper<TData const>> getDataByEntityId(EntityId entity_id) const {
        // Use O(1) storage lookup instead of registry-based lookup
        auto idx_opt = _storage.findByEntityId(entity_id);
        if (!idx_opt.has_value()) {
            return std::nullopt;
        }
        return std::cref(_storage.getData(*idx_opt));
    }

    /**
     * @brief Find the time frame index for a specific EntityId.
     * 
     * Returns the time frame associated with the given EntityId.
     * 
     * @param entity_id The EntityId to look up
     * @return Optional containing the TimeFrameIndex if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<TimeFrameIndex> getTimeByEntityId(EntityId entity_id) const {
        // Use O(1) storage lookup instead of registry-based lookup
        auto idx_opt = _storage.findByEntityId(entity_id);
        if (!idx_opt.has_value()) {
            return std::nullopt;
        }
        return _storage.getTime(*idx_opt);
    }

    /**
     * @brief Get all data that match the given EntityIds.
     * 
     * This method is optimized for batch lookup of multiple EntityIds,
     * useful for group-based operations.
     * 
     * @param entity_ids Vector of EntityIds to look up
     * @return Vector of pairs containing {EntityId, const reference to data} for found entities
     */
    [[nodiscard]] std::vector<std::pair<EntityId, std::reference_wrapper<TData const>>> getDataByEntityIds(std::vector<EntityId> const & entity_ids) const {
        std::vector<std::pair<EntityId, std::reference_wrapper<TData const>>> results;
        results.reserve(entity_ids.size());

        for (EntityId const entity_id: entity_ids) {
            auto data = getDataByEntityId(entity_id);
            if (data.has_value()) {
                results.emplace_back(entity_id, data.value());
            }
        }

        return results;
    }

    // ========== Mutable Data Access ==========

    /**
     * @brief Type alias for modification handle
     * 
     * This provides a unified type for RAII-style modification handles.
     * Derived classes can use this or define their own aliases for convenience.
     */
    using DataModifier = ModificationHandle<TData>;

    /**
     * @brief Get a mutable handle to data by EntityId.
     *
     * This method returns an RAII-style handle. The handle provides
     * pointer-like access to the underlying TData (Line2D, Mask2D, Point2D<float>).
     *
     * When the handle is destroyed (goes out of scope), the observers will be
     * automatically notified if requested.
     *
     * @param entity_id The EntityId to look up
     * @param notify Whether to notify observers when the handle is destroyed
     * @return Optional containing a DataModifier handle if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<DataModifier> getMutableData(EntityId entity_id, NotifyObservers notify) {
        // Use O(1) storage lookup
        auto idx_opt = _storage.findByEntityId(entity_id);
        if (!idx_opt.has_value()) {
            return std::nullopt;
        }

        TData & data_ref = _storage.getMutableData(*idx_opt);

        return DataModifier(data_ref, [this, notify]() { 
            if (notify == NotifyObservers::Yes) { 
                this->notifyObservers(); 
            } 
        });
    }

    /**
     * @brief Clear data by its EntityId
     * 
     * Removes the data entry associated with the given EntityId from the time series.
     * If this results in an empty time frame, the time frame is also removed.
     * 
     * @param entity_id The EntityId of the data to clear
     * @param notify Whether to notify observers after the operation
     * @return true if the data was found and cleared, false otherwise
     */
    [[nodiscard]] bool clearByEntityId(EntityId entity_id, NotifyObservers notify) {
        bool const removed = _storage.removeByEntityId(entity_id);
        if (removed && notify == NotifyObservers::Yes) {
            notifyObservers();
        }
        return removed;
    }

    /**
     * @brief Clear all data at a specific time with timeframe conversion
     * 
     * Removes all data entries at the specified time. If the time is from a different
     * timeframe, it will be converted to this data's timeframe first.
     * 
     * @param time_index_and_frame The time and timeframe to clear at
     * @param notify Whether to notify observers after the operation
     * @return true if data was found and cleared, false otherwise
     */
    [[nodiscard]] bool clearAtTime(TimeIndexAndFrame const & time_index_and_frame, NotifyObservers notify) {
        TimeFrameIndex const converted_time = convert_time_index(time_index_and_frame.index,
                                                                 time_index_and_frame.time_frame,
                                                                 _time_frame.get());
        return _clearAtTime(converted_time, notify);
    }

    // ========== Add Data Methods ==========

    /**
     * @brief Add data at a specific time (by copying).
     *
     * This overload is called when you pass an existing lvalue (e.g., a named variable).
     * It will create a copy of the data.
     * 
     * @param time The time to add the data at
     * @param data The data to add (will be copied)
     * @param notify Whether to notify observers after the operation
     */
    void addAtTime(TimeFrameIndex time, TData const & data, NotifyObservers notify) {
        auto [start, end] = _storage.getTimeRange(time);
        int const local_index = static_cast<int>(end - start);
        EntityId entity_id = EntityId(0);
        if (_identity_registry) {
            entity_id = _identity_registry->ensureId(_identity_data_key, getEntityKind(), time, local_index);
        }

        _storage.append(time, data, entity_id);

        if (notify == NotifyObservers::Yes) {
            notifyObservers();
        }
    }

    /**
     * @brief Add data at a specific time with timeframe conversion (by copying).
     * 
     * @param time_index_and_frame The time and timeframe to add at
     * @param data The data to add (will be copied)
     * @param notify Whether to notify observers after the operation
     */
    void addAtTime(TimeIndexAndFrame const & time_index_and_frame, TData const & data, NotifyObservers notify) {
        TimeFrameIndex const converted_time = convert_time_index(time_index_and_frame.index,
                                                                 time_index_and_frame.time_frame,
                                                                 _time_frame.get());
        addAtTime(converted_time, data, notify);
    }

    /**
     * @brief Add data at a specific time (by moving).
     *
     * This overload is called when you pass an rvalue (e.g., a temporary object
     * or the result of std::move()). It will move the data, avoiding a copy.
     * 
     * @param time The time to add the data at
     * @param data The data to add (will be moved)
     * @param notify Whether to notify observers after the operation
     */
    void addAtTime(TimeFrameIndex time, TData && data, NotifyObservers notify) {
        auto [start, end] = _storage.getTimeRange(time);
        int const local_index = static_cast<int>(end - start);
        EntityId entity_id = EntityId(0);
        if (_identity_registry) {
            entity_id = _identity_registry->ensureId(_identity_data_key, getEntityKind(), time, local_index);
        }

        _storage.append(time, std::move(data), entity_id);

        if (notify == NotifyObservers::Yes) {
            notifyObservers();
        }
    }

    /**
     * @brief Add data at a specific time with timeframe conversion (by moving).
     * 
     * @param time_index_and_frame The time and timeframe to add at
     * @param data The data to add (will be moved)
     * @param notify Whether to notify observers after the operation
     */
    void addAtTime(TimeIndexAndFrame const & time_index_and_frame, TData && data, NotifyObservers notify) {
        TimeFrameIndex const converted_time = convert_time_index(time_index_and_frame.index,
                                                                 time_index_and_frame.time_frame,
                                                                 _time_frame.get());
        addAtTime(converted_time, std::move(data), notify);
    }

    /**
     * @brief Construct a data entry in-place at a specific time.
     *
     * This method perfectly forwards its arguments to the
     * constructor of the TData (e.g., Line2D, Mask2D, Point2D<float>) object.
     * This is the most efficient way to add new data.
     *
     * @tparam TDataArgs The types of arguments for TData's constructor
     * @param time The time to add the data at
     * @param args The arguments to forward to TData's constructor
     */
    template<typename... TDataArgs>
    void emplaceAtTime(TimeFrameIndex time, TDataArgs &&... args) {
        auto [start, end] = _storage.getTimeRange(time);
        int const local_index = static_cast<int>(end - start);
        EntityId entity_id = EntityId(0);
        if (_identity_registry) {
            entity_id = _identity_registry->ensureId(_identity_data_key, getEntityKind(), time, local_index);
        }

        // Construct TData in-place then append
        _storage.append(time, TData(std::forward<TDataArgs>(args)...), entity_id);
    }

    /**
     * @brief Add a batch of data at a specific time by copying them.
     *
     * Appends the data entries to any already existing at that time.
     * 
     * @param time The time to add the data at
     * @param data_to_add Vector of data to add (will be copied)
     * @param notify Whether to notify observers after the operation
     */
    void addAtTime(TimeFrameIndex time, std::vector<TData> const & data_to_add, NotifyObservers notify) {
        if (data_to_add.empty()) {
            return;
        }

        auto [start, end] = _storage.getTimeRange(time);
        size_t const old_count = end - start;

        for (size_t i = 0; i < data_to_add.size(); ++i) {
            int const local_index = static_cast<int>(old_count + i);
            EntityId entity_id = EntityId(0);
            if (_identity_registry) {
                entity_id = _identity_registry->ensureId(_identity_data_key, getEntityKind(), time, local_index);
            }
            _storage.append(time, data_to_add[i], entity_id);
        }

        if (notify == NotifyObservers::Yes) {
            notifyObservers();
        }
    }

    /**
     * @brief Add a batch of data at a specific time by moving them.
     *
     * Appends the data entries to any already existing at that time.
     * The input vector will be left in a state with "empty" data entries.
     * 
     * @param time The time to add the data at
     * @param data_to_add Vector of data to add (will be moved)
     * @param notify Whether to notify observers after the operation
     */
    void addAtTime(TimeFrameIndex time, std::vector<TData> && data_to_add, NotifyObservers notify) {
        if (data_to_add.empty()) {
            return;
        }

        auto [start, end] = _storage.getTimeRange(time);
        size_t const old_count = end - start;

        for (size_t i = 0; i < data_to_add.size(); ++i) {
            int const local_index = static_cast<int>(old_count + i);
            EntityId entity_id = EntityId(0);
            if (_identity_registry) {
                entity_id = _identity_registry->ensureId(_identity_data_key, getEntityKind(), time, local_index);
            }
            _storage.append(time, std::move(data_to_add[i]), entity_id);
        }

        if (notify == NotifyObservers::Yes) {
            notifyObservers();
        }
    }

    // ========== Data Access Methods ==========

    /**
     * @brief Get the number of distinct time frames with data
     * 
     * @return The count of time frames containing at least one entry
     */
    [[nodiscard]] std::size_t getTimeCount() const {
        return _storage.getTimeCount();
    }

    /**
     * @brief Get the maximum number of entries at any single time frame
     * 
     * Scans all time frames and returns the size of the largest vector of entries.
     * This represents the maximum "width" of the ragged array structure.
     * 
     * @return The maximum number of entries at any time, or 0 if no data exists
     */
    [[nodiscard]] std::size_t getMaxEntriesAtAnyTime() const {
        std::size_t max_entries = 0;
        for (auto const& [time, range] : _storage.timeRanges()) {
            size_t const count = range.second - range.first;
            max_entries = std::max(max_entries, count);
        }
        return max_entries;
    }

    /**
     * @brief Get the total number of entries across all time frames
     * 
     * Sums the number of entries at each time frame to get the total count
     * of all data entries in this time series.
     * 
     * @return The total count of all entries
     */
    [[nodiscard]] std::size_t getTotalEntryCount() const {
        return _storage.size();
    }

    /**
     * @brief Get all times with data
     * 
     * Returns a view over the keys of the data map for zero-copy iteration.
     * 
     * @return A view of TimeFrameIndex keys
     */
    [[nodiscard]] auto getTimesWithData() const {
        return _storage.timeRanges() | std::views::keys;
    }

    // ========== Time-based Getters ==========

    public:
    [[nodiscard]] auto getAtTime(TimeFrameIndex time) const {
        auto [start, end] = _storage.getTimeRange(time);
        return std::views::iota(start, end) 
            | std::views::transform([this](size_t idx) -> TData const& {
                return _storage.getData(idx);
            });
    }

    [[nodiscard]] auto getAtTime(TimeFrameIndex time, TimeFrame const & source_timeframe) const {
        TimeFrameIndex const converted_time = convert_time_index(time,
                                                                 &source_timeframe,
                                                                 _time_frame.get());
        return getAtTime(converted_time);
    }

    [[nodiscard]] auto getAtTime(TimeIndexAndFrame const & time_index_and_frame) const {
        TimeFrameIndex const converted_time = convert_time_index(time_index_and_frame.index,
                                                                 time_index_and_frame.time_frame,
                                                                 _time_frame.get());
        return getAtTime(converted_time);
    }

    [[nodiscard]] auto getEntityIdsAtTime(TimeFrameIndex time) const {
        auto [start, end] = _storage.getTimeRange(time);
        return std::views::iota(start, end)
            | std::views::transform([this](size_t idx) {
                return _storage.getEntityId(idx);
            });
    }

    [[nodiscard]] auto getEntityIdsAtTime(TimeFrameIndex time, TimeFrame const & source_timeframe) const {
        TimeFrameIndex const converted_time = convert_time_index(time,
                                                                 &source_timeframe,
                                                                 _time_frame.get());
        return getEntityIdsAtTime(converted_time);
    }

    /**
     * @brief Get data entries with their associated times as a zero-copy range within a TimeFrameInterval
     *
     * Returns a filtered view of time-data entries pairs for times within the specified interval [start, end] (inclusive).
     * This method provides zero-copy access to the underlying DataEntry<TData> data structure.
     *
     * @param interval The TimeFrameInterval specifying the range [start, end] (inclusive)
     * @return A zero-copy view of time-data entries pairs for times within the specified interval
     */
    /**
     * @brief Get a filtered view of elements within a TimeFrameInterval
     *
     * Returns a filtered view of (Time, EntityId, TData const&) tuples for times 
     * within the specified interval [start, end] (inclusive).
     * This method provides zero-copy access to the underlying data.
     *
     * @param interval The TimeFrameInterval specifying the range [start, end] (inclusive)
     * @return A zero-copy filtered view of flattened_data() for times within the interval
     */
    [[nodiscard]] auto getElementsInRange(TimeFrameInterval const & interval) const {
        return flattened_data() 
            | std::views::filter([interval](auto const & tuple) {
                   auto const & time = std::get<0>(tuple);
                   return time >= interval.start && time <= interval.end;
               });
    }

    /**
     * @brief Get a filtered view of elements within a TimeFrameInterval with timeframe conversion
     *
     * Converts the time range from the source timeframe to the target timeframe (this data's timeframe)
     * and returns a filtered view of (Time, EntityId, TData const&) tuples for times within the 
     * converted interval range.
     * If the timeframes are the same, no conversion is performed.
     * This method provides zero-copy access to the underlying data.
     *
     * @param interval The TimeFrameInterval in the source timeframe specifying the range [start, end] (inclusive)
     * @param source_timeframe The timeframe that the interval is expressed in
     * @return A zero-copy filtered view of flattened_data() for times within the converted interval
     */
    [[nodiscard]] auto getElementsInRange(TimeFrameInterval const & interval,
                                         TimeFrame const & source_timeframe) const {
        // If the timeframes are the same object, no conversion is needed
        if (&source_timeframe == _time_frame.get()) {
            return getElementsInRange(interval);
        }

        // If either timeframe is null, fall back to original behavior
        if (!_time_frame) {
            return getElementsInRange(interval);
        }

        auto [target_start_index, target_end_index] = convertTimeFrameRange(interval.start,
                                                                            interval.end,
                                                                            source_timeframe,
                                                                            *_time_frame);

        return getElementsInRange(TimeFrameInterval(target_start_index, target_end_index));
    }

    // ========== Ranges & Views ==========

    /**
     * @brief A flattened view of (Time, DataEntry) pairs.
     * * This creates a zero-overhead 1D view of all entities across all times.
     * Crucially, it preserves the TimeFrameIndex for each entity.
     * * usage: for(auto [time, entry] : ts.elements()) { ... }
     */
    [[nodiscard]] auto elements() const {
        // Use flattened iteration over all storage indices
        return std::views::iota(size_t{0}, _storage.size())
            | std::views::transform([this](size_t idx) {
                return std::make_pair(
                    _storage.getTime(idx), 
                    DataEntry<TData>{_storage.getEntityId(idx), _storage.getData(idx)});
            });
    }

    /**
     * @brief A flattened view of (Time, EntityId, TData) tuples.
     * * Helper that unpacks the DataEntry for easier structured binding.
     */
    [[nodiscard]] auto flattened_data() const {
        return std::views::iota(size_t{0}, _storage.size())
            | std::views::transform([this](size_t idx) {
                return std::make_tuple(
                    _storage.getTime(idx), 
                    _storage.getEntityId(idx), 
                    std::cref(_storage.getData(idx)));
            });
    }

    /**
     * @brief Create a view of this time series.
     * This works for RaggedTimeSeries and any derived class (like PointData).
     */
    auto view() const;


protected:
    /**
     * @brief Get the EntityKind for this data type.
     * 
     * This method uses if constexpr to determine the correct EntityKind
     * based on the TData template parameter at compile time.
     * 
     * @return The EntityKind corresponding to TData
     */
    [[nodiscard]] static constexpr EntityKind getEntityKind() {
        // We need to match on the TData type
        // Forward declarations needed:
        // - Line2D, Mask2D, Point2D<float>
        
        if constexpr (std::is_same_v<TData, Line2D>) {
            return EntityKind::LineEntity;
        } else if constexpr (std::is_same_v<TData, Mask2D>) {
            return EntityKind::MaskEntity;
        } else if constexpr (std::is_same_v<TData, Point2D<float>>) {
            return EntityKind::PointEntity;
        } else {
            // Fallback for unknown types - this will cause a compile error
            // if an unsupported type is used
            static_assert(std::is_same_v<TData, Line2D> || 
                         std::is_same_v<TData, Mask2D> || 
                         std::is_same_v<TData, Point2D<float>>,
                         "TData must be Line2D, Mask2D, or Point2D<float>");
            return EntityKind::LineEntity; // Never reached, but needed for compilation
        }
    }

    /**
     * @brief Clear all data at a specific time (internal helper)
     * 
     * This is the implementation method that directly clears data at a given time
     * without any timeframe conversion. It's used by the public clearAtTime method
     * and can be used by derived classes.
     * 
     * @param time The time to clear data at
     * @param notify Whether to notify observers after the operation
     * @return true if data was found and cleared, false otherwise
     */
    [[nodiscard]] bool _clearAtTime(TimeFrameIndex time, NotifyObservers notify) {
        size_t const removed = _storage.removeAtTime(time);
        if (removed == 0) {
            return false;
        }
        
        if (notify == NotifyObservers::Yes) {
            notifyObservers();
        }
        return true;
    }

    // ========== Protected Member Variables ==========
    
    /// Storage for time series data using SoA layout
    OwningRaggedStorage<TData> _storage;
    
    /// Image size metadata
    ImageSize _image_size;
    
    /// Associated time frame for temporal indexing
    std::shared_ptr<TimeFrame> _time_frame{nullptr};
    
    /// Data key for EntityRegistry lookups
    std::string _identity_data_key;
    
    /// Pointer to EntityRegistry for automatic EntityId management
    EntityRegistry * _identity_registry{nullptr};
};

// RaggedTimeSeriesView wraps a RaggedTimeSeries and provides a proper range interface
// It caches the elements view to ensure begin()/end() return compatible iterators
template <typename TData>
class RaggedTimeSeriesView : public std::ranges::view_interface<RaggedTimeSeriesView<TData>> {
public:
    // The underlying elements view type from RaggedTimeSeries::elements()
    using ElementsView = decltype(std::declval<RaggedTimeSeries<TData> const&>().elements());
    
private:
    // We store the view by value to ensure iterator compatibility
    // mutable because begin()/end() are const but we need to lazily initialize
    mutable std::optional<ElementsView> _cached_view;
    RaggedTimeSeries<TData> const* _ts = nullptr;
    
    void ensureView() const {
        if (!_cached_view.has_value() && _ts != nullptr) {
            _cached_view.emplace(_ts->elements());
        }
    }
    
public:
    RaggedTimeSeriesView() = default;
    explicit RaggedTimeSeriesView(RaggedTimeSeries<TData> const& ts) : _ts(&ts) {}
    
    auto begin() const { 
        ensureView();
        return _cached_view->begin();
    }
    
    auto end() const { 
        ensureView();
        return _cached_view->end();
    }
    
    // Convenience method for explicit flattening
    [[nodiscard]] auto flatten() const { return _ts->elements(); }
    [[nodiscard]] auto elements() const { return _ts->elements(); }
    
    // Size information
    [[nodiscard]] std::size_t size() const { return _ts ? _ts->getTotalEntryCount() : 0; }
    [[nodiscard]] bool empty() const { return size() == 0; }
};

template <typename TData>
auto RaggedTimeSeries<TData>::view() const {
    return RaggedTimeSeriesView<TData>(*this);
}

#endif // RAGGED_TIME_SERIES_HPP
