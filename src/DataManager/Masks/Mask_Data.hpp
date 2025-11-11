#ifndef MASK_DATA_HPP
#define MASK_DATA_HPP

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "utils/RaggedTimeSeries.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <ranges>
#include <span>
#include <unordered_set>
#include <vector>

using MaskEntry = DataEntry<Mask2D>;


/**
 * @brief The MaskData class
 *
 * MaskData is used for 2D data where the collection of 2D points has *no* order
 * Compare to LineData where the collection of 2D points has an order
 *
 */
class MaskData : public RaggedTimeSeries<Mask2D> {
public:
    // ========== Constructors ==========
    MaskData() = default;

    // ========== Setters (Time-based) ==========

    [[nodiscard]] bool clearAtTime(TimeIndexAndFrame const & time_index_and_frame,
                                   NotifyObservers notify);




    void addAtTime(TimeFrameIndex time, Mask2D const & mask, bool notify = true);

    void addAtTime(TimeFrameIndex time, Mask2D && mask, bool notify = true);

    /**
     * @brief Construct a data entry in-place at a specific time.
     *
     * This method perfectly forwards its arguments to the
     * constructor of the TData (e.g., Mask2D) object.
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
            entity_id = _identity_registry->ensureId(_identity_data_key, EntityKind::MaskEntity, time, local_index);
        }

        _data[time].emplace_back(entity_id, std::forward<TDataArgs>(args)...);
    }

    /**
     * @brief Adds a new mask at the specified time using separate x and y coordinate arrays
    *
    * If masks already exist at the specified time, the new mask is added to the collection.
    * If no masks exist at that time, a new entry is created.
    *
    * @param time The timestamp at which to add the mask
    * @param x Vector of x-coordinates defining the mask points
    * @param y Vector of y-coordinates defining the mask points
    * @param notify If true, observers will be notified of the change (default: true)
    *
    * @note x and y vectors must be the same length, representing coordinate pairs
    */
    void addAtTime(TimeFrameIndex time,
                   std::vector<uint32_t> const & x,
                   std::vector<uint32_t> const & y,
                   bool notify = true);


    void addAtTime(TimeIndexAndFrame const & time_index_and_frame,
                   std::vector<Point2D<uint32_t>> mask,
                   bool notify = true);

    /**
     * @brief Adds a new mask at the specified time using separate x and y coordinate arrays (move version)
    *
    * This overload avoids copying the input vectors by moving them.
    * If masks already exist at the specified time, the new mask is added to the collection.
    * If no masks exist at that time, a new entry is created.
    *
    * @param time The timestamp at which to add the mask
    * @param x Vector of x-coordinates defining the mask points (will be moved)
    * @param y Vector of y-coordinates defining the mask points (will be moved)
    * @param notify If true, observers will be notified of the change (default: true)
    *
    * @note x and y vectors must be the same length, representing coordinate pairs
    */
    void addAtTime(TimeFrameIndex time,
                   std::vector<uint32_t> && x,
                   std::vector<uint32_t> && y,
                   bool notify = true);

    // ========== Setters (Entity-based) ==========

    /**
     * @brief Removes a mask with a specific EntityId
     *
     * @param entity_id The EntityId of the mask to remove
     * @param notify If true, observers will be notified of the change
     * @return True if the mask was found and removed, false otherwise
     */
    [[nodiscard]] bool clearByEntityId(EntityId entity_id, NotifyObservers notify);

    /**
     * @brief Add a mask entry at a specific time with a specific entity ID
     *
     * This method is used internally for move operations to preserve entity IDs.
     *
     * @param time The time to add the mask at
     * @param mask The mask to add
     * @param entity_id The entity ID to assign to the mask
     * @param notify If true, the observers will be notified
     */
    void addEntryAtTime(TimeFrameIndex time, Mask2D const & mask, EntityId entity_id, NotifyObservers notify);


    // ========== Getters ==========

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
     * @brief Get the masks at a specific time
     * 
     * @param time The time to get the masks at
     * @return A vector of Mask2D
     */
    [[nodiscard]] std::vector<Mask2D> const & getAtTime(TimeFrameIndex time) const;

    [[nodiscard]] std::vector<Mask2D> const & getAtTime(TimeIndexAndFrame const & time_index_and_frame) const;

    /**
     * @brief Get the masks at a specific time with timeframe conversion
     * 
     * @param time The time to get the masks at
     * @param source_timeframe The timeframe that the time index is expressed in
     * @param mask_timeframe The timeframe that this mask data uses
     * @return A vector of Mask2D at the converted time
     */
    [[nodiscard]] std::vector<Mask2D> const & getAtTime(TimeFrameIndex time,
                                                        TimeFrame const & source_timeframe) const;

    /**
     * @brief Get all mask entries with their associated times as a zero-copy range
     *
     * This method provides zero-copy access to the underlying MaskEntry data structure,
     * which contains both Mask2D and EntityId information.
     *
     * @return A view of time-mask entries pairs for all times
     */
    [[nodiscard]] auto getAllEntries() const {
        return _data | std::views::transform([](auto const & pair) {
                   // pair.second is a std::vector<MaskEntry>&
                   // We create a non-owning span pointing to its data
                   return std::make_pair(
                           pair.first,
                           std::span<MaskEntry const>{pair.second});
               });
    }

    /**
     * @brief Get all masks with their associated times as a range
     *
     * @return A view of time-mask pairs for all times
     */
    [[nodiscard]] auto getAllAsRange() const {
        struct TimeMaskPair {
            TimeFrameIndex time;
            std::vector<Mask2D> masks;// Copy for backward compatibility with entry storage
        };

        return _data | std::views::transform([](auto const & pair) {
                   std::vector<Mask2D> masks;
                   masks.reserve(pair.second.size());
                   for (auto const & entry: pair.second) {
                       masks.push_back(entry.data);
                   }
                   return TimeMaskPair{pair.first, std::move(masks)};
               });
    };

    /**
    * @brief Get masks with their associated times as a range within a TimeFrameInterval
    *
    * Returns a filtered view of time-mask pairs for times within the specified interval [start, end] (inclusive).
    *
    * @param interval The TimeFrameInterval specifying the range [start, end] (inclusive)
    * @return A view of time-mask pairs for times within the specified interval
    */
    [[nodiscard]] auto GetMasksInRange(TimeFrameInterval const & interval) const {
        struct TimeMaskPair {
            TimeFrameIndex time;
            std::vector<Mask2D> masks;// Copy out of entries
        };

        return _data | std::views::filter([interval](auto const & pair) {
                   return pair.first >= interval.start && pair.first <= interval.end;
               }) |
               std::views::transform([](auto const & pair) {
                   std::vector<Mask2D> masks;
                   masks.reserve(pair.second.size());
                   for (auto const & entry: pair.second) {
                       masks.push_back(entry.data);
                   }
                   return TimeMaskPair{pair.first, std::move(masks)};
               });
    }

    /**
    * @brief Get masks with their associated times as a range within a TimeFrameInterval with timeframe conversion
    *
    * Converts the time range from the source timeframe to the target timeframe (this mask data's timeframe)
    * and returns a filtered view of time-mask pairs for times within the converted interval range.
    * If the timeframes are the same, no conversion is performed.
    *
    * @param interval The TimeFrameInterval in the source timeframe specifying the range [start, end] (inclusive)
    * @param source_timeframe The timeframe that the interval is expressed in
    * @return A view of time-mask pairs for times within the converted interval range
    */
    [[nodiscard]] auto GetMasksInRange(TimeFrameInterval const & interval,
                                       TimeFrame const & source_timeframe) const {
        // If the timeframes are the same object, no conversion is needed
        if (&source_timeframe == _time_frame.get()) {
            return GetMasksInRange(interval);
        }

        // If either timeframe is null, fall back to original behavior
        if (!_time_frame) {
            return GetMasksInRange(interval);
        }

        // Convert the time range from source timeframe to target timeframe
        // 1. Get the time values from the source timeframe
        auto start_time_value = source_timeframe.getTimeAtIndex(interval.start);
        auto end_time_value = source_timeframe.getTimeAtIndex(interval.end);

        // 2. Convert those time values to indices in the target timeframe
        auto target_start_index = _time_frame->getIndexAtTime(static_cast<float>(start_time_value));
        auto target_end_index = _time_frame->getIndexAtTime(static_cast<float>(end_time_value));

        // 3. Create converted interval and use the original function
        TimeFrameInterval target_interval{target_start_index, target_end_index};
        return GetMasksInRange(target_interval);
    }

    [[nodiscard]] size_t size() const { return _data.size(); };

    // ========== Image Size ==========
    /**
     * @brief Change the size of the canvas the mask belongs to
     *
     * This will scale all mask in the data structure by the ratio of the
     * new size to the old size.
     *
     * @param image_size
     */
    void changeImageSize(ImageSize const & image_size);

    /**
     * @brief Get the image size
     * 
     * @return The image size
     */
    [[nodiscard]] ImageSize getImageSize() const { return _image_size; }

    /**
     * @brief Set the image size
     */
    void setImageSize(ImageSize const & image_size) { _image_size = image_size; }

    // ========== Copy and Move ==========

    /**
     * @brief Copy masks with specific EntityIds to another MaskData
     *
     * Copies all masks that match the given EntityIds to the target MaskData.
     * The copied masks will get new EntityIds in the target.
     *
     * @param target The target MaskData to copy masks to
     * @param entity_ids Vector of EntityIds to copy
     * @param notify If true, the target will notify its observers after the operation
     * @return The number of masks actually copied
     */
    std::size_t copyByEntityIds(MaskData & target, std::unordered_set<EntityId> const & entity_ids, NotifyObservers notify);

    /**
     * @brief Move masks with specific EntityIds to another MaskData
     *
     * Moves all masks that match the given EntityIds to the target MaskData.
     * The moved masks will keep the same EntityIds in the target and be removed from source.
     *
     * @param target The target MaskData to move masks to
     * @param entity_ids Vector of EntityIds to move
     * @param notify If true, both source and target will notify their observers after the operation
     * @return The number of masks actually moved
     */
    std::size_t moveByEntityIds(MaskData & target, std::unordered_set<EntityId> const & entity_ids, NotifyObservers notify);

    // ========== Entity Lookup Methods ==========

    /**
     * @brief Get EntityIds aligned with masks at a specific time.
     */
    [[nodiscard]] std::vector<EntityId> const & getEntityIdsAtTime(TimeFrameIndex time) const;

    /**
     * @brief Get flattened EntityIds for all masks across all times.
     */
    [[nodiscard]] std::vector<EntityId> getAllEntityIds() const;

    /**
     * @brief Find the mask data associated with a specific EntityId.
     * 
     * This method provides reverse lookup from EntityId to the actual mask data,
     * supporting group-based visualization workflows.
     * 
     * @param entity_id The EntityId to look up
     * @return Optional containing the mask data if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<Mask2D> getMaskByEntityId(EntityId entity_id) const;

    /**
     * @brief Find the time frame and local index for a specific EntityId.
     * 
     * Returns the time frame and local mask index (within that time frame)
     * associated with the given EntityId.
     * 
     * @param entity_id The EntityId to look up
     * @return Optional containing {time, local_index} if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<std::pair<TimeFrameIndex, int>> getTimeAndIndexByEntityId(EntityId entity_id) const;

    /**
     * @brief Get all masks that match the given EntityIds.
     * 
     * This method is optimized for batch lookup of multiple EntityIds,
     * useful for group-based operations.
     * 
     * @param entity_ids Vector of EntityIds to look up
     * @return Vector of pairs containing {EntityId, Mask2D} for found entities
     */
    [[nodiscard]] std::vector<std::pair<EntityId, Mask2D>> getMasksByEntityIds(std::vector<EntityId> const & entity_ids) const;

    /**
     * @brief Get time frame information for multiple EntityIds.
     * 
     * Returns time frame and local index information for multiple EntityIds,
     * useful for understanding the temporal distribution of entity groups.
     * 
     * @param entity_ids Vector of EntityIds to look up
     * @return Vector of tuples containing {EntityId, TimeFrameIndex, local_index} for found entities
     */
    [[nodiscard]] std::vector<std::tuple<EntityId, TimeFrameIndex, int>> getTimeInfoByEntityIds(std::vector<EntityId> const & entity_ids) const;

private:
    std::vector<Mask2D> _empty{};
    std::vector<EntityId> _empty_entity_ids{};
    mutable std::vector<Mask2D> _temp_masks{};
    mutable std::vector<EntityId> _temp_entity_ids{};


    /**
    * @brief Removes all masks at the specified time
    *
    * @param time The timestamp at which to clear masks
    * @param notify If true, observers will be notified of the change
    * @return True if masks existed at the specified time and were cleared, false otherwise
    */
    [[nodiscard]] bool _clearAtTime(TimeFrameIndex time, NotifyObservers notify);
};


#endif// MASK_DATA_HPP
