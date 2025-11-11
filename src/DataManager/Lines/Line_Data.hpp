#ifndef LINE_DATA_HPP
#define LINE_DATA_HPP

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "utils/RaggedTimeSeries.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <ranges>
#include <unordered_set>
#include <vector>


/**
 * @brief Structure holding a Line2D and its associated EntityId
 */
using LineEntry = DataEntry<Line2D>;

/*
 * @brief LineData
 *
 * LineData is used for storing 2D lines
 * Line data implies that the elements in the line have an order
 * Compare to MaskData where the elements in the mask have no order
 */
class LineData : public RaggedTimeSeries<Line2D> {
public:
    // ========== Constructors ==========
    /**
     * @brief Default constructor
     * 
     * This constructor creates an empty LineData with no data
     */
    LineData() = default;

    /**
     * @brief Move constructor
     */
    LineData(LineData && other) noexcept;

    /**
     * @brief Move assignment operator
     */
    LineData & operator=(LineData && other) noexcept;


    /**
     * @brief Constructor with data
     * 
     * This constructor creates a LineData with the given data
     * 
     * @param data The data to initialize the LineData with
     */
    explicit LineData(std::map<TimeFrameIndex, std::vector<Line2D>> const & data);

    // ========== Setters (Time-based) ==========


    [[nodiscard]] bool clearAtTime(TimeIndexAndFrame const & time_index_and_frame,
        NotifyObservers notify);

    /**
     * @brief Add a line at a specific time (by copying).
     *
     * This overload is called when you pass an existing lvalue (e.g., a named variable).
     * It will create a copy of the line.
     * 
     * @param notify Whether to notify observers after the operation
     */
    void addAtTime(TimeFrameIndex time, Line2D const & line, NotifyObservers notify);

    void addAtTime(TimeIndexAndFrame const & time_index_and_frame, Line2D const & line, NotifyObservers notify);

    /**
     * @brief Add a line at a specific time (by moving).
     *
     * This overload is called when you pass an rvalue (e.g., a temporary object
     * or the result of std::move()). It will move the line's data,
     * avoiding a copy.
     * 
     * @param notify Whether to notify observers after the operation
     */

    void addAtTime(TimeFrameIndex time, Line2D && line, NotifyObservers notify);
    
    void addAtTime(TimeIndexAndFrame const & time_index_and_frame, Line2D && line, NotifyObservers notify);

    /**
     * @brief Construct a data entry in-place at a specific time.
     *
     * This method perfectly forwards its arguments to the
     * constructor of the TData (e.g., Line2D) object.
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
            entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::LineEntity, time, local_index);
        }

        _data[time].emplace_back(entity_id, std::forward<TDataArgs>(args)...);
    }

    /**
     * @brief Add a batch of lines at a specific time by copying them.
     *
     * Appends the lines to any already existing at that time.
     */
    void addAtTime(TimeFrameIndex time, std::vector<Line2D> const & lines_to_add);

    /**
     * @brief Add a batch of lines at a specific time by moving them.
     *
     * Appends the lines to any already existing at that time.
     * The input vector will be left in a state with "empty" lines.
     */
    void addAtTime(TimeFrameIndex time, std::vector<Line2D> && lines_to_add);

    // ========== Setters (Entity-based) ==========


    using LineModifier = ModificationHandle<Line2D>;

    /**
     * @brief Get a mutable handle to a line by EntityId.
     *
     * This method returns an RAII-style handle. The handle provides
     * pointer-like access to the Line2D.
     *
     * When the handle is destroyed (goes out of scope), the LineData's
     * observers will be automatically notified if requested.
     *
     * @param entity_id The EntityId to look up
     * @param notify Whether to notify observers when the handle is destroyed
     * @return Optional containing a LineModifier handle if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<LineModifier> getMutableData(EntityId entity_id, NotifyObservers notify);

    /**
     * @brief Clear a line by its EntityId
     * 
     * @param entity_id The EntityId of the line to clear
     * @param notify Whether to notify observers after the operation
     * @return true if the line was found and cleared, false otherwise
     */
    [[nodiscard]] bool clearByEntityId(EntityId entity_id, NotifyObservers notify);

    /**
     * @brief Add a line entry at a specific time with a specific entity ID
     * 
     * This method is used internally for move operations to preserve entity IDs.
     * 
     * @param time The time to add the line at
     * @param line The line to add
     * @param entity_id The entity ID to assign to the line
     * @param notify Whether to notify observers after the operation
     */
    void addEntryAtTime(TimeFrameIndex time, Line2D const & line, EntityId entity_id, NotifyObservers notify);

    // ========== Image Size ==========

    /**
     * @brief Change the size of the canvas the line belongs to
     *
     * This will scale all lines in the data structure by the ratio of the
     * new size to the old size.
     *
     * @param image_size
     */
    void changeImageSize(ImageSize const & image_size);

    [[nodiscard]] ImageSize getImageSize() const { return _image_size; }
    void setImageSize(ImageSize const & image_size) { _image_size = image_size; }

    // ========== Getters ==========

    /**
    * @brief Get all line entries with their associated times as a zero-copy range
    *
    * This method provides zero-copy access to the underlying LineEntry data structure,
    * which contains both Line2D and EntityId information.
    *
    * @return A view of time-line entries pairs for all times
    */
    [[nodiscard]] auto getAllEntries() const {
        return _data | std::views::transform([](auto const & pair) {
                   // pair.second is a std::vector<DataEntry<TData>>&
                   // We create a non-owning span pointing to its data
                   return std::make_pair(
                           pair.first,
                           std::span<LineEntry const>{pair.second}// <-- The key part
                   );
               });
    }


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

    // ========== Getters (Time-based) ==========

    /**
     * @brief Get a zero-copy view of all data entries at a specific time.
     * @param time The time to get entries for.
     * @return A std::span over the entries. If time is not found,
     * returns an empty span.
     */
    [[nodiscard]] std::span<DataEntry<Line2D> const> getEntriesAtTime(TimeFrameIndex time) const {
        auto it = _data.find(time);
        if (it == _data.end()) {
            return _empty_entries;
        }
        return it->second;
    }

    /**
    * @brief Get a zero-copy view of just the data (e.g., Line2D) at a time.
    *
    * @param time The time to get the data at
    * @return A zero-copy view of the data at the time
    */
    [[nodiscard]] auto getAtTime(TimeFrameIndex time) const {
        return getEntriesAtTime(time) | std::views::transform(&DataEntry<Line2D>::data);
    }

    [[nodiscard]] auto getAtTime(TimeFrameIndex time, TimeFrame const & source_timeframe) const {
        TimeFrameIndex const converted_time = convert_time_index(time,
                                                                 &source_timeframe,
                                                                 _time_frame.get());
        return getEntriesAtTime(converted_time) | std::views::transform(&DataEntry<Line2D>::data);
    }

    [[nodiscard]] auto getEntityIdsAtTime(TimeFrameIndex time) const {
        return getEntriesAtTime(time) | std::views::transform(&DataEntry<Line2D>::entity_id);
    }

    [[nodiscard]] auto getEntityIdsAtTime(TimeFrameIndex time, TimeFrame const & source_timeframe) const {
        TimeFrameIndex const converted_time = convert_time_index(time,
                                                                 &source_timeframe,
                                                                 _time_frame.get());
        return getEntriesAtTime(converted_time) | std::views::transform(&DataEntry<Line2D>::entity_id);
    }

    /**
    * @brief Get line entries with their associated times as a zero-copy range within a TimeFrameInterval
    *
    * Returns a filtered view of time-line entries pairs for times within the specified interval [start, end] (inclusive).
    * This method provides zero-copy access to the underlying LineEntry data structure.
    *
    * @param interval The TimeFrameInterval specifying the range [start, end] (inclusive)
    * @return A zero-copy view of time-line entries pairs for times within the specified interval
    */
    [[nodiscard]] auto GetEntriesInRange(TimeFrameInterval const & interval) const {
        return getAllEntries() | std::views::filter([interval](auto const & pair) {
                   return pair.first >= interval.start && pair.first <= interval.end;
               });
    }


    /**
    * @brief Get line entries with their associated times as a zero-copy range within a TimeFrameInterval with timeframe conversion
    *
    * Converts the time range from the source timeframe to the target timeframe (this line data's timeframe)
    * and returns a filtered view of time-line entries pairs for times within the converted interval range.
    * If the timeframes are the same, no conversion is performed.
    * This method provides zero-copy access to the underlying LineEntry data structure.
    *
    * @param interval The TimeFrameInterval in the source timeframe specifying the range [start, end] (inclusive)
    * @param source_timeframe The timeframe that the interval is expressed in
    * @return A zero-copy view of time-line entries pairs for times within the converted interval range
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


    // ========== Entity Lookup Methods ==========

    /**
     * @brief Find the line data associated with a specific EntityId.
     * 
     * This method provides reverse lookup from EntityId to the actual line data,
     * supporting group-based visualization workflows.
     * 
     * @param entity_id The EntityId to look up
     * @return Optional containing the line data if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<std::reference_wrapper<Line2D const>> getDataByEntityId(EntityId entity_id) const;

    [[nodiscard]] std::optional<TimeFrameIndex> getTimeByEntityId(EntityId entity_id) const;

    /**
     * @brief Get all lines that match the given EntityIds.
     * 
     * This method is optimized for batch lookup of multiple EntityIds,
     * useful for group-based operations.
     * 
     * @param entity_ids Vector of EntityIds to look up
     * @return Vector of pairs containing {EntityId, Line2D} for found entities
     */
    [[nodiscard]] std::vector<std::pair<EntityId, std::reference_wrapper<Line2D const>>> getDataByEntityIds(std::vector<EntityId> const & entity_ids) const;


    // ========== Copy and Move ==========

    /**
     * @brief Copy lines with specific EntityIds to another LineData
     * 
     * Copies all lines that match the given EntityIds to the target LineData.
     * The copied lines will get new EntityIds in the target.
     * 
     * @param target The target LineData to copy lines to
     * @param entity_ids Vector of EntityIds to copy
     * @param notify Whether to notify the target's observers after the operation
     * @return The number of lines actually copied
     */
    std::size_t copyByEntityIds(LineData & target, std::unordered_set<EntityId> const & entity_ids, NotifyObservers notify);

    /**
     * @brief Move lines with specific EntityIds to another LineData
     * 
     * Moves all lines that match the given EntityIds to the target LineData.
     * The moved lines will get new EntityIds in the target and be removed from source.
     * 
     * @param target The target LineData to move lines to
     * @param entity_ids Vector of EntityIds to move
     * @param notify Whether to notify both source and target observers after the operation
     * @return The number of lines actually moved
     */
    std::size_t moveByEntityIds(LineData & target, std::unordered_set<EntityId> const & entity_ids, NotifyObservers notify);

private:
    inline static std::vector<DataEntry<Line2D>> const _empty_entries{};


    /**
     * @brief Clear all lines at a specific time
     * 
     * @param time The time to clear the lines at
     * @param notify Whether to notify observers after the operation
     */
    [[nodiscard]] bool _clearAtTime(TimeFrameIndex time, NotifyObservers notify);
};


#endif// LINE_DATA_HPP
