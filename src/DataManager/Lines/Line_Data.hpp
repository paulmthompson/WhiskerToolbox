#ifndef LINE_DATA_HPP
#define LINE_DATA_HPP

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "Entity/EntityTypes.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <ranges>
#include <unordered_set>
#include <vector>

class EntityRegistry;

/**
 * @brief Structure holding a Line2D and its associated EntityId
 */
struct LineEntry {
    Line2D line;
    EntityId entity_id;

    LineEntry() = default;
    LineEntry(Line2D l, EntityId id)
        : line(std::move(l)),
          entity_id(id) {}
};

/*
 * @brief LineData
 *
 * LineData is used for storing 2D lines
 * Line data implies that the elements in the line have an order
 * Compare to MaskData where the elements in the mask have no order
 */
class LineData : public ObserverData {
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

    // ========== Setters ==========

    /**
     * @brief Clear all lines at a specific time
     * 
     * @param time The time to clear the lines at
     * @param notify If true, the observers will be notified
     */
    [[nodiscard]] bool clearAtTime(TimeFrameIndex time, bool notify = true);

    /**
     * @brief Clear a line at a specific time
     * 
     * @param time The time to clear the line at
     * @param line_id The id of the line to clear
     * @param notify If true, the observers will be notified
     */
    [[nodiscard]] bool clearAtTime(TimeFrameIndex time, int line_id, bool notify = true);

    using LineModifier = ModificationHandle<Line2D>;

    /**
     * @brief Get a mutable handle to a line by EntityId.
     *
     * This method returns an RAII-style handle. The handle provides
     * pointer-like access to the Line2D.
     *
     * When the handle is destroyed (goes out of scope), the LineData's
     * observers will be automatically notified.
     *
     * @param entity_id The EntityId to look up
     * @return Optional containing a LineModifier handle if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<LineModifier> getMutableLine(EntityId entity_id, bool notify = true);

    /**
     * @brief Add a line at a specific time
     * 
     * The line is defined by the x and y coordinates.
     * 
     * @param time The time to add the line at
     * @param x The x coordinates of the line
     * @param y The y coordinates of the line
     * @param notify If true, the observers will be notified
     */
    void addAtTime(TimeFrameIndex time, std::vector<float> const & x, std::vector<float> const & y, bool notify = true);

    /**
     * @brief Add a line at a specific time
     * 
     * The line is defined by the points.
     * 
     * @param time The time to add the line at
     * @param line The line to add
     * @param notify If true, the observers will be notified
     */
    void addAtTime(TimeFrameIndex time, std::vector<Point2D<float>> const & line, bool notify = true);

    /**
     * @brief Add a line at a specific time
     * 
     * The line is defined by the Line2D object.
     * 
     * @param time The time to add the line at
     * @param line The line to add
     * @param notify If true, the observers will be notified
     */
    void addAtTime(TimeFrameIndex time, Line2D const & line, bool notify = true);

    /**
     * @brief Add a point to a line at a specific time
     * 
     * The point is appended to the line.
     * 
     * @param time The time to add the point to the line at
     * @param line_id The id of the line to add the point to
     * @param point The point to add
     * @param notify If true, the observers will be notified
     */
    void addPointToLine(TimeFrameIndex time, int line_id, Point2D<float> point, bool notify = true);

    /**
     * @brief Add a line entry at a specific time with a specific entity ID
     * 
     * This method is used internally for move operations to preserve entity IDs.
     * 
     * @param time The time to add the line at
     * @param line The line to add
     * @param entity_id The entity ID to assign to the line
     * @param notify If true, the observers will be notified
     */
    void addEntryAtTime(TimeFrameIndex time, Line2D const & line, EntityId entity_id, bool notify = true);

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
     * @brief Get the lines at a specific time
     * 
     * If the time does not exist, an empty vector will be returned.
     * 
     * @param time The time to get the lines at
     * @return A vector of lines
     */
    [[nodiscard]] std::vector<Line2D> const & getAtTime(TimeFrameIndex time) const;

    /**
     * @brief Get the lines at a specific time with timeframe conversion
     * 
     * Converts the time index from the source timeframe to the target timeframe (this line data's timeframe)
     * and returns the lines at the converted time. If the timeframes are the same, no conversion is performed.
     * If the converted time does not exist, an empty vector will be returned.
     * 
     * @param time The time index in the source timeframe
     * @param source_timeframe The timeframe that the time index is expressed in
     * @return A vector of lines at the converted time
     */
    [[nodiscard]] std::vector<Line2D> const & getAtTime(TimeFrameIndex time,
                                                        TimeFrame const & source_timeframe) const;

    /**
     * @brief Get EntityIds aligned with lines at a specific time.
     */
    [[nodiscard]] std::vector<EntityId> const & getEntityIdsAtTime(TimeFrameIndex time) const;

    [[nodiscard]] std::vector<EntityId> const & getEntityIdsAtTime(TimeFrameIndex time,
                                                                   TimeFrame const & source_timeframe) const;

    /**
     * @brief Get flattened EntityIds for all lines across all times.
     */
    [[nodiscard]] std::vector<EntityId> getAllEntityIds() const;

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
    [[nodiscard]] std::optional<Line2D> getLineByEntityId(EntityId entity_id) const;

    /**
     * @brief Get a mutable reference to a line by EntityId.
     * 
     * This method allows direct modification of a line while preserving its EntityId.
     * The returned reference points to the actual line data in the LineData structure.
     * 
     * @param entity_id The EntityId to look up
     * @return Optional containing a mutable reference to the line if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<std::reference_wrapper<Line2D>> getMutableLineByEntityId(EntityId entity_id);

    /**
     * @brief Find the time frame and local index for a specific EntityId.
     * 
     * Returns the time frame and local line index (within that time frame)
     * associated with the given EntityId.
     * 
     * @param entity_id The EntityId to look up
     * @return Optional containing {time, local_index} if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<std::pair<TimeFrameIndex, int>> getTimeAndIndexByEntityId(EntityId entity_id) const;

    /**
     * @brief Get all lines that match the given EntityIds.
     * 
     * This method is optimized for batch lookup of multiple EntityIds,
     * useful for group-based operations.
     * 
     * @param entity_ids Vector of EntityIds to look up
     * @return Vector of pairs containing {EntityId, Line2D} for found entities
     */
    [[nodiscard]] std::vector<std::pair<EntityId, Line2D>> getLinesByEntityIds(std::vector<EntityId> const & entity_ids) const;

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

    /**
    * @brief Get zero-copy view of Line2D objects at a specific time
    *
    * @param time The time to get lines for
    * @return A view that iterates over Line2D objects only
    */
    [[nodiscard]] std::vector<std::reference_wrapper<Line2D const>> getLinesViewAtTime(TimeFrameIndex time) const {
        std::vector<std::reference_wrapper<Line2D const>> result;

        auto it = _data.find(time);
        if (it != _data.end()) {
            result.reserve(it->second.size());
            for (auto const & entry: it->second) {
                result.emplace_back(std::cref(entry.line));
            }
        }

        return result;
    }

    /**
    * @brief Get zero-copy view of EntityId objects at a specific time
    *
    * @param time The time to get entity IDs for
    * @return A view that iterates over EntityId objects only
    */
    [[nodiscard]] std::vector<std::reference_wrapper<EntityId const>> getEntityIdsViewAtTime(TimeFrameIndex time) const {
        std::vector<std::reference_wrapper<EntityId const>> result;

        auto it = _data.find(time);
        if (it != _data.end()) {
            result.reserve(it->second.size());
            for (auto const & entry: it->second) {
                result.emplace_back(std::cref(entry.entity_id));
            }
        }

        return result;
    }

    /**
    * @brief Get all lines with their associated times as a range
    *
    * @return A view of time-lines pairs for all times
    */
    [[nodiscard]] auto GetAllLinesAsRange() const {
        struct TimeLinesPair {
            TimeFrameIndex time;
            std::vector<Line2D> lines;// Note: This creates a copy for backward compatibility
        };

        return _data | std::views::transform([](auto const & pair) {
                   std::vector<Line2D> lines;
                   lines.reserve(pair.second.size());
                   for (auto const & entry: pair.second) {
                       lines.push_back(entry.line);
                   }
                   return TimeLinesPair{pair.first, std::move(lines)};
               });
    }

    /**
    * @brief Get all line entries with their associated times as a zero-copy range
    *
    * This method provides zero-copy access to the underlying LineEntry data structure,
    * which contains both Line2D and EntityId information.
    *
    * @return A view of time-line entries pairs for all times
    */
    [[nodiscard]] auto GetAllLineEntriesAsRange() const {
        struct TimeLineEntriesPair {
            TimeFrameIndex time;
            std::vector<LineEntry> const & entries;
        };

        return _data | std::views::transform([](auto const & pair) {
                   return TimeLineEntriesPair{pair.first, pair.second};
               });
    }


    /**
    * @brief Get lines with their associated times as a range within a TimeFrameInterval
    *
    * Returns a filtered view of time-lines pairs for times within the specified interval [start, end] (inclusive).
    *
    * @param interval The TimeFrameInterval specifying the range [start, end] (inclusive)
    * @return A view of time-lines pairs for times within the specified interval
    */
    [[nodiscard]] auto GetLinesInRange(TimeFrameInterval const & interval) const {
        struct TimeLinesPair {
            TimeFrameIndex time;
            std::vector<Line2D> lines;// Copy for backward compatibility
        };

        return _data | std::views::filter([interval](auto const & pair) {
                   return pair.first >= interval.start && pair.first <= interval.end;
               }) |
               std::views::transform([](auto const & pair) {
                   std::vector<Line2D> lines;
                   lines.reserve(pair.second.size());
                   for (auto const & entry: pair.second) {
                       lines.push_back(entry.line);
                   }
                   return TimeLinesPair{pair.first, std::move(lines)};
               });
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
    [[nodiscard]] auto GetLineEntriesInRange(TimeFrameInterval const & interval) const {
        struct TimeLineEntriesPair {
            TimeFrameIndex time;
            std::vector<LineEntry> const & entries;
        };

        return _data | std::views::filter([interval](auto const & pair) {
                   return pair.first >= interval.start && pair.first <= interval.end;
               }) |
               std::views::transform([](auto const & pair) {
                   return TimeLineEntriesPair{pair.first, pair.second};
               });
    }

    /**
    * @brief Get lines with their associated times as a range within a TimeFrameInterval with timeframe conversion
    *
    * Converts the time range from the source timeframe to the target timeframe (this line data's timeframe)
    * and returns a filtered view of time-lines pairs for times within the converted interval range.
    * If the timeframes are the same, no conversion is performed.
    *
    * @param interval The TimeFrameInterval in the source timeframe specifying the range [start, end] (inclusive)
    * @param source_timeframe The timeframe that the interval is expressed in
    * @return A view of time-lines pairs for times within the converted interval range
    */
    [[nodiscard]] auto GetLinesInRange(TimeFrameInterval const & interval,
                                       TimeFrame const & source_timeframe) const {
        // If the timeframes are the same object, no conversion is needed
        if (&source_timeframe == _time_frame.get()) {
            return GetLinesInRange(interval);
        }

        // If either timeframe is null, fall back to original behavior
        if (!_time_frame) {
            return GetLinesInRange(interval);
        }

        auto [target_start_index, target_end_index] = convertTimeFrameRange(interval.start,
            interval.end,
            source_timeframe,
            *_time_frame);

        return GetLinesInRange(TimeFrameInterval(target_start_index, target_end_index));
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
    [[nodiscard]] auto GetLineEntriesInRange(TimeFrameInterval const & interval,
                                             TimeFrame const & source_timeframe) const {
        // If the timeframes are the same object, no conversion is needed
        if (&source_timeframe == _time_frame.get()) {
            return GetLineEntriesInRange(interval);
        }

        // If either timeframe is null, fall back to original behavior
        if (!_time_frame) {
            return GetLineEntriesInRange(interval);
        }

        auto [target_start_index, target_end_index] = convertTimeFrameRange(interval.start,
                                                                            interval.end,
                                                                            source_timeframe,
                                                                            *_time_frame);

        return GetLineEntriesInRange(TimeFrameInterval(target_start_index, target_end_index));
    }

    // ========== Copy and Move ==========

    /**
     * @brief Copy lines from this LineData to another LineData for a time interval
     * 
     * Copies all lines within the specified time interval [start, end] (inclusive)
     * to the target LineData. If lines already exist at target times, the copied lines
     * are added to the existing lines.
     * 
     * @param target The target LineData to copy lines to
     * @param interval The time interval to copy lines from (inclusive)
     * @param notify If true, the target will notify its observers after the operation
     * @return The number of lines actually copied
     */
    std::size_t copyTo(LineData & target, TimeFrameInterval const & interval, bool notify = true) const;

    /**
     * @brief Copy lines from this LineData to another LineData for specific times
     * 
     * Copies all lines at the specified times to the target LineData.
     * If lines already exist at target times, the copied lines are added to the existing lines.
     * 
     * @param target The target LineData to copy lines to
     * @param times Vector of specific times to copy (does not need to be sorted)
     * @param notify If true, the target will notify its observers after the operation
     * @return The number of lines actually copied
     */
    std::size_t copyTo(LineData & target, std::vector<TimeFrameIndex> const & times, bool notify = true) const;

    /**
     * @brief Move lines from this LineData to another LineData for a time interval
     * 
     * Moves all lines within the specified time interval [start, end] (inclusive)
     * to the target LineData. Lines are copied to target then removed from source.
     * If lines already exist at target times, the moved lines are added to the existing lines.
     * 
     * @param target The target LineData to move lines to
     * @param interval The time interval to move lines from (inclusive)
     * @param notify If true, both source and target will notify their observers after the operation
     * @return The number of lines actually moved
     */
    std::size_t moveTo(LineData & target, TimeFrameInterval const & interval, bool notify = true);

    /**
     * @brief Move lines from this LineData to another LineData for specific times
     * 
     * Moves all lines at the specified times to the target LineData.
     * Lines are copied to target then removed from source.
     * If lines already exist at target times, the moved lines are added to the existing lines.
     * 
     * @param target The target LineData to move lines to
     * @param times Vector of specific times to move (does not need to be sorted)
     * @param notify If true, both source and target will notify their observers after the operation
     * @return The number of lines actually moved
     */
    std::size_t moveTo(LineData & target, std::vector<TimeFrameIndex> const & times, bool notify = true);

    /**
     * @brief Copy lines with specific EntityIds to another LineData
     * 
     * Copies all lines that match the given EntityIds to the target LineData.
     * The copied lines will get new EntityIds in the target.
     * 
     * @param target The target LineData to copy lines to
     * @param entity_ids Vector of EntityIds to copy
     * @param notify If true, the target will notify its observers after the operation
     * @return The number of lines actually copied
     */
    std::size_t copyByEntityIds(LineData & target, std::unordered_set<EntityId> const & entity_ids, bool notify = true);

    /**
     * @brief Move lines with specific EntityIds to another LineData
     * 
     * Moves all lines that match the given EntityIds to the target LineData.
     * The moved lines will get new EntityIds in the target and be removed from source.
     * 
     * @param target The target LineData to move lines to
     * @param entity_ids Vector of EntityIds to move
     * @param notify If true, both source and target will notify their observers after the operation
     * @return The number of lines actually moved
     */
    std::size_t moveByEntityIds(LineData & target, std::unordered_set<EntityId> const & entity_ids, bool notify = true);

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
     * @brief Rebuild EntityIds for all lines using the current identity context.
     */
    void rebuildAllEntityIds();

protected:
private:
    std::map<TimeFrameIndex, std::vector<LineEntry>> _data;
    std::vector<Line2D> _empty{};
    std::vector<EntityId> _empty_entity_ids{};
    mutable std::vector<Line2D> _temp_lines{};       // For getAtTime compatibility
    mutable std::vector<EntityId> _temp_entity_ids{};// For getEntityIdsAtTime compatibility
    ImageSize _image_size;
    std::shared_ptr<TimeFrame> _time_frame{nullptr};

    // Identity management
    std::string _identity_data_key;
    EntityRegistry * _identity_registry{nullptr};
};


#endif// LINE_DATA_HPP
