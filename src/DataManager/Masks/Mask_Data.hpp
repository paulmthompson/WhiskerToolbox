#ifndef MASK_DATA_HPP
#define MASK_DATA_HPP

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "Entity/EntityTypes.hpp"

#include <cstddef>
#include <map>
#include <ranges>
#include <vector>

class EntityRegistry;

/**
 * @brief Structure holding a Mask2D and its associated EntityId
 */
struct MaskEntry {
    Mask2D mask;
    EntityId entity_id;
    
    MaskEntry() = default;
    MaskEntry(Mask2D m, EntityId id) : mask(std::move(m)), entity_id(id) {}
};


/**
 * @brief The MaskData class
 *
 * MaskData is used for 2D data where the collection of 2D points has *no* order
 * Compare to LineData where the collection of 2D points has an order
 *
 */
class MaskData : public ObserverData {
public:
    // ========== Constructors ==========
    MaskData() = default;

    // ========== Setters ==========

    /**
    * @brief Removes all masks at the specified time
    *
    * @param time The timestamp at which to clear masks
    * @param notify If true, observers will be notified of the change
    * @return True if masks existed at the specified time and were cleared, false otherwise
    */
    [[nodiscard]] bool clearAtTime(TimeFrameIndex time, bool notify = true);

    [[nodiscard]] bool clearAtTime(TimeIndexAndFrame const & time_index_and_frame, bool notify = true);

    /**
     * @brief Removes a mask at the specified time and index
     * 
     * @param time The timestamp at which to clear the mask
     * @param index The index of the mask to clear
     * @param notify If true, observers will be notified of the change
     */
    [[nodiscard]] bool clearAtTime(TimeFrameIndex time, size_t index, bool notify = true);

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

    /**
    * @brief Adds a new mask at the specified time using a vector of 2D points
    *
    * If masks already exist at the specified time, the new mask is added to the collection.
    * If no masks exist at that time, a new entry is created.
    *
    * @param time The timestamp at which to add the mask
    * @param mask Vector of 2D points defining the mask
    * @param notify If true, observers will be notified of the change (default: true)
    */
    void addAtTime(TimeFrameIndex time,
                       std::vector<Point2D<uint32_t>> mask,
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
    void addMaskEntryAtTime(TimeFrameIndex time, Mask2D const & mask, EntityId entity_id, bool notify = true);


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
                                                        TimeFrame const * source_timeframe,
                                                        TimeFrame const * mask_timeframe) const;

        /**
     * @brief Get all masks with their associated times as a range
     *
     * @return A view of time-mask pairs for all times
     */
    [[nodiscard]] auto getAllAsRange() const {
        struct TimeMaskPair {
            TimeFrameIndex time;
            std::vector<Mask2D> masks; // Copy for backward compatibility with entry storage
        };

        return _data | std::views::transform([](auto const & pair) {
                   std::vector<Mask2D> masks;
                   masks.reserve(pair.second.size());
                   for (auto const & entry : pair.second) {
                       masks.push_back(entry.mask);
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
            std::vector<Mask2D> masks; // Copy out of entries
        };

        return _data 
            | std::views::filter([interval](auto const & pair) {
                return pair.first >= interval.start && pair.first <= interval.end;
              })
            | std::views::transform([](auto const & pair) {
                std::vector<Mask2D> masks;
                masks.reserve(pair.second.size());
                for (auto const & entry : pair.second) {
                    masks.push_back(entry.mask);
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
    * @param target_timeframe The timeframe that this mask data uses
    * @return A view of time-mask pairs for times within the converted interval range
    */
    [[nodiscard]] auto GetMasksInRange(TimeFrameInterval const & interval,
                                       std::shared_ptr<TimeFrame> source_timeframe,
                                       std::shared_ptr<TimeFrame> target_timeframe) const {
        // If the timeframes are the same object, no conversion is needed
        if (source_timeframe.get() == target_timeframe.get()) {
            return GetMasksInRange(interval);
        }

        // If either timeframe is null, fall back to original behavior
        if (!source_timeframe || !target_timeframe) {
            return GetMasksInRange(interval);
        }

        // Convert the time range from source timeframe to target timeframe
        // 1. Get the time values from the source timeframe
        auto start_time_value = source_timeframe->getTimeAtIndex(interval.start);
        auto end_time_value = source_timeframe->getTimeAtIndex(interval.end);

        // 2. Convert those time values to indices in the target timeframe
        auto target_start_index = target_timeframe->getIndexAtTime(static_cast<float>(start_time_value));
        auto target_end_index = target_timeframe->getIndexAtTime(static_cast<float>(end_time_value));

        // 3. Create converted interval and use the original function
        TimeFrameInterval target_interval{target_start_index, target_end_index};
        return GetMasksInRange(target_interval);
    }

    [[nodiscard]] size_t size() const {return _data.size();};

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
     * @brief Copy masks from this MaskData to another MaskData for a time interval
     * 
     * Copies all masks within the specified time interval [start, end] (inclusive)
     * to the target MaskData. If masks already exist at target times, the copied masks
     * are added to the existing masks.
     * 
     * @param target The target MaskData to copy masks to
     * @param interval The time interval to copy masks from (inclusive)
     * @param notify If true, the target will notify its observers after the operation
     * @return The number of masks actually copied
     */
    std::size_t copyTo(MaskData& target, TimeFrameInterval const & interval, bool notify = true) const;

    /**
     * @brief Copy masks from this MaskData to another MaskData for specific times
     * 
     * Copies all masks at the specified times to the target MaskData.
     * If masks already exist at target times, the copied masks are added to the existing masks.
     * 
     * @param target The target MaskData to copy masks to
     * @param times Vector of specific times to copy (does not need to be sorted)
     * @param notify If true, the target will notify its observers after the operation
     * @return The number of masks actually copied
     */
    std::size_t copyTo(MaskData& target, std::vector<TimeFrameIndex> const& times, bool notify = true) const;

    /**
     * @brief Move masks from this MaskData to another MaskData for a time interval
     * 
     * Moves all masks within the specified time interval [start, end] (inclusive)
     * to the target MaskData. Masks are copied to target then removed from source.
     * If masks already exist at target times, the moved masks are added to the existing masks.
     * 
     * @param target The target MaskData to move masks to
     * @param interval The time interval to move masks from (inclusive)
     * @param notify If true, both source and target will notify their observers after the operation
     * @return The number of masks actually moved
     */
    std::size_t moveTo(MaskData& target, TimeFrameInterval const & interval, bool notify = true);

    /**
     * @brief Move masks from this MaskData to another MaskData for specific times
     * 
     * Moves all masks at the specified times to the target MaskData.
     * Masks are copied to target then removed from source.
     * If masks already exist at target times, the moved masks are added to the existing masks.
     * 
     * @param target The target MaskData to move masks to
     * @param times Vector of specific times to move (does not need to be sorted)
     * @param notify If true, both source and target will notify their observers after the operation
     * @return The number of masks actually moved
     */
    std::size_t moveTo(MaskData& target, std::vector<TimeFrameIndex> const& times, bool notify = true);

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
    std::size_t copyMasksByEntityIds(MaskData & target, std::vector<EntityId> const & entity_ids, bool notify = true);

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
    std::size_t moveMasksByEntityIds(MaskData & target, std::vector<EntityId> const & entity_ids, bool notify = true);


    // ========== Time Frame ==========
    /**
     * @brief Set the time frame
     * 
     * @param time_frame The time frame to set
     */
    void setTimeFrame(std::shared_ptr<TimeFrame> time_frame) { _time_frame = time_frame; }

    /**
     * @brief Set identity context for automatic EntityId maintenance.
     */
    void setIdentityContext(std::string const & data_key, EntityRegistry * registry);

    /**
     * @brief Rebuild EntityIds for all masks using the current identity context.
     */
    void rebuildAllEntityIds();

    /**
     * @brief Get EntityIds aligned with masks at a specific time.
     */
    [[nodiscard]] std::vector<EntityId> const & getEntityIdsAtTime(TimeFrameIndex time) const;

    /**
     * @brief Get flattened EntityIds for all masks across all times.
     */
    [[nodiscard]] std::vector<EntityId> getAllEntityIds() const;

protected:
private:
    std::map<TimeFrameIndex, std::vector<MaskEntry>> _data;
    std::vector<Mask2D> _empty{};
    std::vector<EntityId> _empty_entity_ids{};
    mutable std::vector<Mask2D> _temp_masks{};
    mutable std::vector<EntityId> _temp_entity_ids{};
    ImageSize _image_size;
    std::shared_ptr<TimeFrame> _time_frame {nullptr};

    // Identity management
    std::string _identity_data_key;
    EntityRegistry * _identity_registry {nullptr};
};


#endif// MASK_DATA_HPP
