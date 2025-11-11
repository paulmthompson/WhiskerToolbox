#ifndef POINT_DATA_HPP
#define POINT_DATA_HPP

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/points.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "utils/RaggedTimeSeries.hpp"

#include <map>
#include <optional>
#include <ranges>
#include <unordered_set>
#include <vector>


using PointEntry = DataEntry<Point2D<float>>;

/**
 * @brief PointData
 *
 * PointData is used for storing 2D point collections at specific time frames.
 * Each time frame can contain multiple points, making it suitable for tracking
 * multiple features or keypoints over time.
 *
 * For example, keypoints for multiple body points could be a PointData object
 */
class PointData : public RaggedTimeSeries<Point2D<float>> {
public:
    // ========== Constructors ==========
    /**
     * @brief Default constructor
     * 
     * This constructor creates an empty PointData with no data
     */
    PointData() = default;

    /**
     * @brief Constructor for PointData from a map of TimeFrameIndex to Point2D<float>
     * 
     * @param data Map of TimeFrameIndex to Point2D<float>
     */
    explicit PointData(std::map<TimeFrameIndex, Point2D<float>> const & data);

    /**
     * @brief Constructor for PointData from a map of TimeFrameIndex to vector of Point2D<float>
     * 
     * @param data Map of TimeFrameIndex to vector of Point2D<float>
     */
    explicit PointData(std::map<TimeFrameIndex, std::vector<Point2D<float>>> const & data);

    // ========== Setters (Time-based) ==========

    /**
     * @brief Clear the points at a specific time
     * 
     * @param time The time to clear the points at
     * @param notify If true, the observers will be notified
     * @return True if the points were cleared, false if the time did not exist
     */
    [[nodiscard]] bool clearAtTime(TimeFrameIndex time, bool notify = true);

    /**
     * @brief Add a point at a specific time
     * 
     * This will add a single point at a specific time.
     * 
     * If the time does not exist, it will be created.
     * 
     * If the time already exists, the point will be added to the existing points.
     * 
     * @param time The time to add the point at
     * @param point The point to add
     * @param notify If true, the observers will be notified
     */
    void addAtTime(TimeFrameIndex time, Point2D<float> point, bool notify = true);

    /**
     * @brief Add a point entry at a specific time with a specific entity ID
     *
     * This method is used internally for move operations to preserve entity IDs.
     *
     * @param time The time to add the point at
     * @param point The point to add
     * @param entity_id The entity ID to assign to the point
     * @param notify If true, the observers will be notified
     */
    void addEntryAtTime(TimeFrameIndex time, Point2D<float> const & point, EntityId entity_id, NotifyObservers notify);


    /**
     * @brief Add a batch of points at a specific time by copying them.
     *
     * Appends the points to any already existing at that time.
     */
    void addAtTime(TimeFrameIndex time, std::vector<Point2D<float>> const & points_to_add);

    /**
     * @brief Add a batch of points at a specific time by moving them.
     *
     * Appends the points to any already existing at that time.
     * The input vector will be left in a state with "empty" points.
     */
    void addAtTime(TimeFrameIndex time, std::vector<Point2D<float>> && points_to_add);

    /**
     * @brief Overwrite a point at a specific time
     * 
     * This will overwrite a point at a specific time.
     * 
     * @param time The time to overwrite the point at
     * @param point The point to overwrite
     * @param notify If true, the observers will be notified
     */
    void overwritePointAtTime(TimeFrameIndex time, Point2D<float> point, bool notify = true);


    // ========== Setters (Entity-based) ==========

    using PointModifier = ModificationHandle<Point2D<float>>;

    [[nodiscard]] std::optional<PointModifier> getMutableData(EntityId entity_id, bool notify = true);

    /**
     * @brief Clear the points by entity ID
     * 
     * @param entity_id The entity ID to clear the points by
     * @param notify If true, the observers will be notified
     * @return True if the points were cleared, false if the entity ID did not exist
     */
    [[nodiscard]] bool clearByEntityId(EntityId entity_id, bool notify = true);

    // ========== Image Size ==========
    /*
    Image size is used to store the size of the image that the points belong to.
    */

    /**
     * @brief Get the image size
     * 
     * @return The image size
     */
    [[nodiscard]] ImageSize getImageSize() const { return _image_size; }

    /**
     * @brief Set the image size
     * 
     * @param image_size The image size
     */
    void setImageSize(ImageSize const & image_size) { _image_size = image_size; }

    /**
     * @brief Change the size of the canvas the points belong to
     *
     * This will scale all points in the data structure by the ratio of the
     * new size to the old size.
     *
     * @param image_size
     */
    void changeImageSize(ImageSize const & image_size);

    // ========== Getters ==========

    /**
    * @brief Get all point entries with their associated times as a zero-copy range
    *
    * This method provides zero-copy access to the underlying PointEntry data structure,
    * which contains both Point2D<float> and EntityId information.
    *
    * @return A view of time-point entries pairs for all times
    */
    [[nodiscard]] auto getAllEntries() const {
        return _data | std::views::transform([](auto const & pair) {
                   // pair.second is a std::vector<DataEntry<TData>>&
                   // We create a non-owning span pointing to its data
                   return std::make_pair(
                           pair.first,
                           std::span<PointEntry const>{pair.second}// <-- The key part
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
    [[nodiscard]] std::span<DataEntry<Point2D<float>> const> getEntriesAtTime(TimeFrameIndex time) const {
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
        return getEntriesAtTime(time) | std::views::transform(&DataEntry<Point2D<float>>::data);
    }

    [[nodiscard]] auto getAtTime(TimeFrameIndex time, TimeFrame const & source_timeframe) const {
        TimeFrameIndex const converted_time = convert_time_index(time,
                                                                 &source_timeframe,
                                                                 _time_frame.get());
        return getEntriesAtTime(converted_time) | std::views::transform(&DataEntry<Point2D<float>>::data);
    }

    [[nodiscard]] auto getEntityIdsAtTime(TimeFrameIndex time) const {
        return getEntriesAtTime(time) | std::views::transform(&DataEntry<Point2D<float>>::entity_id);
    }

    [[nodiscard]] auto getEntityIdsAtTime(TimeFrameIndex time, TimeFrame const & source_timeframe) const {
        TimeFrameIndex const converted_time = convert_time_index(time,
                                                                 &source_timeframe,
                                                                 _time_frame.get());
        return getEntriesAtTime(converted_time) | std::views::transform(&DataEntry<Point2D<float>>::entity_id);
    }


    /**
     * @brief Get the maximum number of points at any time
     * 
     * @return The maximum number of points
     */
    [[nodiscard]] std::size_t getMaxPoints() const;

    /**
    * @brief Get all points with their associated times as a range
    *
    * @return A view of time-points pairs for all times
    */
    [[nodiscard]] auto GetAllPointsAsRange() const {
        struct TimePointsPair {
            TimeFrameIndex time;
            std::vector<Point2D<float>> points;// Copy for backward compatibility with entry storage
        };

        return _data | std::views::transform([](auto const & pair) {
                   std::vector<Point2D<float>> points;
                   points.reserve(pair.second.size());
                   for (auto const & entry: pair.second) {
                       points.push_back(entry.data);
                   }
                   return TimePointsPair{pair.first, std::move(points)};
               });
    }


    /**
    * @brief Get points with their associated times as a range within a TimeFrameInterval
    *
    * Returns a filtered view of time-points pairs for times within the specified interval [start, end] (inclusive).
    *
    * @param interval The TimeFrameInterval specifying the range [start, end] (inclusive)
    * @return A view of time-points pairs for times within the specified interval
    */
    [[nodiscard]] auto GetPointsInRange(TimeFrameInterval const & interval) const {
        struct TimePointsPair {
            TimeFrameIndex time;
            std::vector<Point2D<float>> points;// Copy for backward compatibility
        };

        return _data | std::views::filter([interval](auto const & pair) {
                   return pair.first >= interval.start && pair.first <= interval.end;
               }) |
               std::views::transform([](auto const & pair) {
                   std::vector<Point2D<float>> points;
                   points.reserve(pair.second.size());
                   for (auto const & entry: pair.second) {
                       points.push_back(entry.data);
                   }
                   return TimePointsPair{pair.first, std::move(points)};
               });
    }

    /**
    * @brief Get points with their associated times as a range within a TimeFrameInterval with timeframe conversion
    *
    * Converts the time range from the source timeframe to the target timeframe (this point data's timeframe)
    * and returns a filtered view of time-points pairs for times within the converted interval range.
    * If the timeframes are the same, no conversion is performed.
    *
    * @param interval The TimeFrameInterval in the source timeframe specifying the range [start, end] (inclusive)
    * @param source_timeframe The timeframe that the interval is expressed in
    * @return A view of time-points pairs for times within the converted interval range
    */
    [[nodiscard]] auto GetPointsInRange(TimeFrameInterval const & interval,
                                        TimeFrame const & source_timeframe) const {
        // If the timeframes are the same object, no conversion is needed
        if (&source_timeframe == _time_frame.get()) {
            return GetPointsInRange(interval);
        }

        // If our timeframe is null, fall back to original behavior
        if (!_time_frame) {
            return GetPointsInRange(interval);
        }

        // Convert the time range from source timeframe to target timeframe
        // 1. Get the time values from the source timeframe
        auto start_time_value = source_timeframe.getTimeAtIndex(interval.start);
        auto end_time_value = source_timeframe.getTimeAtIndex(interval.end);

        // 2. Convert those time values to indices in the point timeframe
        auto target_start_index = _time_frame->getIndexAtTime(static_cast<float>(start_time_value), false);
        auto target_end_index = _time_frame->getIndexAtTime(static_cast<float>(end_time_value));

        // 3. Create converted interval and use the original function
        TimeFrameInterval target_interval{target_start_index, target_end_index};
        return GetPointsInRange(target_interval);
    }

    // ========== Entity Lookup Methods ==========

    /**
     * @brief Find the point data associated with a specific EntityId.
     * 
     * This method provides reverse lookup from EntityId to the actual point data,
     * supporting group-based visualization workflows.
     * 
     * @param entity_id The EntityId to look up
     * @return Optional containing the point data if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<std::reference_wrapper<Point2D<float> const>> getDataByEntityId(EntityId entity_id) const;

    [[nodiscard]] std::optional<TimeFrameIndex> getTimeByEntityId(EntityId entity_id) const;

    /**
     * @brief Get all points that match the given EntityIds.
     * 
     * This method is optimized for batch lookup of multiple EntityIds,
     * useful for group-based operations.
     * 
     * @param entity_ids Vector of EntityIds to look up
     * @return Vector of pairs containing {EntityId, Point2D<float>} for found entities
     */
    [[nodiscard]] std::vector<std::pair<EntityId, std::reference_wrapper<Point2D<float> const>>> getDataByEntityIds(std::vector<EntityId> const & entity_ids) const;

    // ======= Move and Copy ==========

    /**
     * @brief Copy points with specific EntityIds to another PointData
     *
     * Copies all points that match the given EntityIds to the target PointData.
     * The copied points will get new EntityIds in the target.
     *
     * @param target The target PointData to copy points to
     * @param entity_ids Vector of EntityIds to copy
     * @param notify If true, the target will notify its observers after the operation
     * @return The number of points actually copied
     */
    std::size_t copyByEntityIds(PointData & target, std::unordered_set<EntityId> const & entity_ids, NotifyObservers notify);

    /**
     * @brief Move points with specific EntityIds to another PointData
     *
     * Moves all points that match the given EntityIds to the target PointData.
     * The moved points will get the same EntityIds in the target and be removed from source.
     *
     * @param target The target PointData to move points to
     * @param entity_ids Vector of EntityIds to move
     * @param notify If true, both source and target will notify their observers after the operation
     * @return The number of points actually moved
     */
    std::size_t moveByEntityIds(PointData & target, std::unordered_set<EntityId> const & entity_ids, NotifyObservers notify);

private:
    mutable std::vector<Point2D<float>> _temp_points{};
    mutable std::vector<EntityId> _temp_entity_ids{};


};

#endif// POINT_DATA_HPP
