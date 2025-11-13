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

#include <algorithm>
#include <map>
#include <memory>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

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
        if (!_identity_registry) {
            // No registry: reset all EntityIds to 0
            for (auto & [t, entries]: _data) {
                (void) t;  // Suppress unused variable warning
                for (auto & entry: entries) {
                    entry.entity_id = EntityId(0);
                }
            }
            return;
        }

        // Determine the EntityKind based on TData type using if constexpr
        EntityKind kind = getEntityKind();

        // Rebuild EntityIds using the registry
        for (auto & [t, entries]: _data) {
            for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
                entries[static_cast<size_t>(i)].entity_id = 
                    _identity_registry->ensureId(_identity_data_key, kind, t, i);
            }
        }
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
        _data[time].emplace_back(entity_id, data);
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
        bool const should_notify = (notify == NotifyObservers::Yes);
        return copy_by_entity_ids(_data, target, entity_ids, should_notify,
                                  [](DataEntry<TData> const & entry) -> TData const & { return entry.data; });
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
        bool const should_notify = (notify == NotifyObservers::Yes);
        auto result = move_by_entity_ids(_data, target, entity_ids, should_notify,
                                         [](DataEntry<TData> const & entry) -> TData const & { return entry.data; });

        if (notify == NotifyObservers::Yes && result > 0) {
            notifyObservers();
        }

        return result;
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
        if (!_identity_registry) {
            return std::nullopt;
        }

        auto descriptor = _identity_registry->get(entity_id);
        if (!descriptor || descriptor->kind != getEntityKind() || descriptor->data_key != _identity_data_key) {
            return std::nullopt;
        }

        TimeFrameIndex const time{descriptor->time_value};
        int const local_index = descriptor->local_index;

        auto time_it = _data.find(time);
        if (time_it == _data.end()) {
            return std::nullopt;
        }

        if (local_index < 0 || static_cast<size_t>(local_index) >= time_it->second.size()) {
            return std::nullopt;
        }

        return std::cref(time_it->second[static_cast<size_t>(local_index)].data);
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
        if (!_identity_registry) {
            return std::nullopt;
        }

        auto descriptor = _identity_registry->get(entity_id);
        if (!descriptor) {
            return std::nullopt;
        }
        
        return TimeFrameIndex{descriptor->time_value};
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
        if (!_identity_registry) {
            return std::nullopt;
        }

        auto descriptor = _identity_registry->get(entity_id);
        if (!descriptor || descriptor->kind != getEntityKind() || descriptor->data_key != _identity_data_key) {
            return std::nullopt;
        }

        auto time_it = _data.find(TimeFrameIndex{descriptor->time_value});
        if (time_it == _data.end()) {
            return std::nullopt;
        }

        size_t local_index = static_cast<size_t>(descriptor->local_index);
        if (local_index >= time_it->second.size()) {
            return std::nullopt;
        }

        TData & data_ref = time_it->second[local_index].data;

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
        if (!_identity_registry) {
            return false;
        }

        auto descriptor = _identity_registry->get(entity_id);
        if (!descriptor || descriptor->kind != getEntityKind() || descriptor->data_key != _identity_data_key) {
            return false;
        }

        TimeFrameIndex const time{descriptor->time_value};
        int const local_index = descriptor->local_index;

        auto time_it = _data.find(time);
        if (time_it == _data.end()) {
            return false;
        }

        if (local_index < 0 || static_cast<size_t>(local_index) >= time_it->second.size()) {
            return false;
        }

        time_it->second.erase(time_it->second.begin() + static_cast<std::ptrdiff_t>(local_index));
        if (time_it->second.empty()) {
            _data.erase(time_it);
        }
        if (notify == NotifyObservers::Yes) {
            notifyObservers();
        }
        return true;
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
        int const local_index = static_cast<int>(_data[time].size());
        EntityId entity_id = EntityId(0);
        if (_identity_registry) {
            entity_id = _identity_registry->ensureId(_identity_data_key, getEntityKind(), time, local_index);
        }

        _data[time].emplace_back(entity_id, data);

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
        int const local_index = static_cast<int>(_data[time].size());
        EntityId entity_id = EntityId(0);
        if (_identity_registry) {
            entity_id = _identity_registry->ensureId(_identity_data_key, getEntityKind(), time, local_index);
        }

        _data[time].emplace_back(entity_id, std::move(data));

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
        int const local_index = static_cast<int>(_data[time].size());
        EntityId entity_id = EntityId(0);
        if (_identity_registry) {
            entity_id = _identity_registry->ensureId(_identity_data_key, getEntityKind(), time, local_index);
        }

        _data[time].emplace_back(entity_id, std::forward<TDataArgs>(args)...);
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

        // 1. Get (or create) the vector of entries for this time
        // This is our single map lookup.
        auto & entry_vec = _data[time];

        // 2. Reserve space once for high performance
        size_t const old_size = entry_vec.size();
        entry_vec.reserve(old_size + data_to_add.size());

        // 3. Loop and emplace new entries
        for (size_t i = 0; i < data_to_add.size(); ++i) {
            int const local_index = static_cast<int>(old_size + i);
            EntityId entity_id = EntityId(0);
            if (_identity_registry) {
                entity_id = _identity_registry->ensureId(_identity_data_key, getEntityKind(), time, local_index);
            }

            // Calls DataEntry(entity_id, data_to_add[i])
            // This will invoke the TData copy constructor
            entry_vec.emplace_back(entity_id, data_to_add[i]);
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

        // 1. Get (or create) the vector of entries for this time
        auto & entry_vec = _data[time];

        // 2. Reserve space once
        size_t const old_size = entry_vec.size();
        entry_vec.reserve(old_size + data_to_add.size());

        // 3. Loop and emplace new entries
        for (size_t i = 0; i < data_to_add.size(); ++i) {
            int const local_index = static_cast<int>(old_size + i);
            EntityId entity_id = EntityId(0);
            if (_identity_registry) {
                entity_id = _identity_registry->ensureId(_identity_data_key, getEntityKind(), time, local_index);
            }

            // Calls DataEntry(entity_id, std::move(data_to_add[i]))
            // This will invoke the TData MOVE constructor
            entry_vec.emplace_back(entity_id, std::move(data_to_add[i]));
        }

        if (notify == NotifyObservers::Yes) {
            notifyObservers();
        }
    }

    // ========== Data Access Methods ==========

    /**
     * @brief Get all times with data
     * 
     * Returns a view over the keys of the data map for zero-copy iteration.
     * 
     * @return A view of TimeFrameIndex keys
     */
    [[nodiscard]] auto getTimesWithData() const {
        return _data | std::views::keys;
    }

    /**
     * @brief Get all data entries with their associated times as a zero-copy range
     *
     * This method provides zero-copy access to the underlying DataEntry<TData> data structure,
     * which contains both the data (Line2D, Mask2D, Point2D<float>) and EntityId information.
     *
     * @return A view of time-entries pairs for all times, where entries are spans
     */
    [[nodiscard]] auto getAllEntries() const {
        return _data | std::views::transform([](auto const & pair) {
                   // pair.second is a std::vector<DataEntry<TData>>&
                   // We create a non-owning span pointing to its data
                   return std::make_pair(
                           pair.first,
                           std::span<DataEntry<TData> const>{pair.second});
               });
    }

    // ========== Time-based Getters ==========

    /**
     * @brief Get a zero-copy view of all data entries at a specific time.
     * @param time The time to get entries for.
     * @return A std::span over the entries. If time is not found,
     * returns an empty span.
     */
    [[nodiscard]] std::span<DataEntry<TData> const> getEntriesAtTime(TimeFrameIndex time) const {
        auto it = _data.find(time);
        if (it == _data.end()) {
            return _empty_entries;
        }
        return it->second;
    }

    [[nodiscard]] auto getAtTime(TimeFrameIndex time) const {
        return getEntriesAtTime(time) | std::views::transform(&DataEntry<TData>::data);
    }

    [[nodiscard]] auto getAtTime(TimeFrameIndex time, TimeFrame const & source_timeframe) const {
        TimeFrameIndex const converted_time = convert_time_index(time,
                                                                 &source_timeframe,
                                                                 _time_frame.get());
        return getEntriesAtTime(converted_time) | std::views::transform(&DataEntry<TData>::data);
    }

    [[nodiscard]] auto getAtTime(TimeIndexAndFrame const & time_index_and_frame) const {
        TimeFrameIndex const converted_time = convert_time_index(time_index_and_frame.index,
                                                                 time_index_and_frame.time_frame,
                                                                 _time_frame.get());
        return getEntriesAtTime(converted_time) | std::views::transform(&DataEntry<TData>::data);
    }

    [[nodiscard]] auto getEntityIdsAtTime(TimeFrameIndex time) const {
        return getEntriesAtTime(time) | std::views::transform(&DataEntry<TData>::entity_id);
    }

    [[nodiscard]] auto getEntityIdsAtTime(TimeFrameIndex time, TimeFrame const & source_timeframe) const {
        TimeFrameIndex const converted_time = convert_time_index(time,
                                                                 &source_timeframe,
                                                                 _time_frame.get());
        return getEntriesAtTime(converted_time) | std::views::transform(&DataEntry<TData>::entity_id);
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
    [[nodiscard]] auto GetEntriesInRange(TimeFrameInterval const & interval) const {
        return getAllEntries() | std::views::filter([interval](auto const & pair) {
                   return pair.first >= interval.start && pair.first <= interval.end;
               });
    }

    /**
     * @brief Get data entries with their associated times as a zero-copy range within a TimeFrameInterval with timeframe conversion
     *
     * Converts the time range from the source timeframe to the target timeframe (this data's timeframe)
     * and returns a filtered view of time-data entries pairs for times within the converted interval range.
     * If the timeframes are the same, no conversion is performed.
     * This method provides zero-copy access to the underlying DataEntry<TData> data structure.
     *
     * @param interval The TimeFrameInterval in the source timeframe specifying the range [start, end] (inclusive)
     * @param source_timeframe The timeframe that the interval is expressed in
     * @return A zero-copy view of time-data entries pairs for times within the converted interval range
     */
    [[nodiscard]] auto GetEntriesInRange(TimeFrameInterval const & interval,
                                         TimeFrame const & source_timeframe) const {
        // If the timeframes are the same object, no conversion is needed
        if (&source_timeframe == _time_frame.get()) {
            return GetEntriesInRange(interval);
        }

        // If either timeframe is null, fall back to original behavior
        if (!_time_frame) {
            return GetEntriesInRange(interval);
        }

        auto [target_start_index, target_end_index] = convertTimeFrameRange(interval.start,
                                                                            interval.end,
                                                                            source_timeframe,
                                                                            *_time_frame);

        return GetEntriesInRange(TimeFrameInterval(target_start_index, target_end_index));
    }


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
        auto it = _data.find(time);
        if (it != _data.end()) {
            _data.erase(it);
            if (notify == NotifyObservers::Yes) {
                notifyObservers();
            }
            return true;
        }
        return false;
    }

    // ========== Protected Member Variables ==========
    
    /// Storage for time series data: map from time to vector of entries
    std::map<TimeFrameIndex, std::vector<DataEntry<TData>>> _data;
    
    /// Image size metadata
    ImageSize _image_size;
    
    /// Associated time frame for temporal indexing
    std::shared_ptr<TimeFrame> _time_frame{nullptr};
    
    /// Data key for EntityRegistry lookups
    std::string _identity_data_key;
    
    /// Pointer to EntityRegistry for automatic EntityId management
    EntityRegistry * _identity_registry{nullptr};

    inline static std::vector<DataEntry<TData>> const _empty_entries{};
};

#endif // RAGGED_TIME_SERIES_HPP
