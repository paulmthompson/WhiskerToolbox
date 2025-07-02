#ifndef LINE_DATA_HPP
#define LINE_DATA_HPP

#include "ImageSize/ImageSize.hpp"
#include "Observer/Observer_Data.hpp"
#include "Points/points.hpp"
#include "TimeFrame.hpp"
#include "DigitalTimeSeries/interval_data.hpp"
#include "lines.hpp"

#include <cstddef>
#include <map>
#include <ranges>
#include <vector>

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
     * @brief Constructor with data
     * 
     * This constructor creates a LineData with the given data
     * 
     * @param data The data to initialize the LineData with
     */
    explicit LineData(std::map<TimeFrameIndex, std::vector<Line2D>> const & data);

    // ========== Setters ==========

    /**
     * @brief Clear a line at a specific time
     * 
     * @param time The time to clear the line at
     * @param line_id The id of the line to clear
     * @param notify If true, the observers will be notified
     */
    void clearLineAtTime(TimeFrameIndex time, int line_id, bool notify = true);

    /**
     * @brief Clear all lines at a specific time
     * 
     * @param time The time to clear the lines at
     * @param notify If true, the observers will be notified
     */
    void clearLinesAtTime(TimeFrameIndex time, bool notify = true);

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
    void addLineAtTime(TimeFrameIndex time, std::vector<float> const & x, std::vector<float> const & y, bool notify = true);

    /**
     * @brief Add a line at a specific time
     * 
     * The line is defined by the points.
     * 
     * @param time The time to add the line at
     * @param line The line to add
     * @param notify If true, the observers will be notified
     */
    void addLineAtTime(TimeFrameIndex time, std::vector<Point2D<float>> const & line, bool notify = true);

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
     * @brief Add a point to a line at a specific time
     * 
     * @param time The time to add the point to the line at
     * @param line_id The id of the line to add the point to
     * @param point The point to add
     * @param notify If true, the observers will be notified
     */
    void addPointToLineInterpolate(TimeFrameIndex time, int line_id, Point2D<float> point, bool notify = true);

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
    [[nodiscard]] std::vector<Line2D> const & getLinesAtTime(TimeFrameIndex time) const;

    /**
     * @brief Get the lines at a specific time with timeframe conversion
     * 
     * Converts the time index from the source timeframe to the target timeframe (this line data's timeframe)
     * and returns the lines at the converted time. If the timeframes are the same, no conversion is performed.
     * If the converted time does not exist, an empty vector will be returned.
     * 
     * @param time The time index in the source timeframe
     * @param source_timeframe The timeframe that the time index is expressed in
     * @param target_timeframe The timeframe that this line data uses
     * @return A vector of lines at the converted time
     */
    [[nodiscard]] std::vector<Line2D> const & getLinesAtTime(TimeFrameIndex time, 
                                                            std::shared_ptr<TimeFrame> source_timeframe,
                                                            std::shared_ptr<TimeFrame> target_timeframe) const;

     /**
    * @brief Get all lines with their associated times as a range
    *
    * @return A view of time-lines pairs for all times
    */
    [[nodiscard]] auto GetAllLinesAsRange() const {
        struct TimeLinesPair {
            TimeFrameIndex time;
            std::vector<Line2D> const & lines;
        };

        return _data | std::views::transform([](auto const & pair) {
                   return TimeLinesPair{TimeFrameIndex(pair.first), pair.second};
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
            std::vector<Line2D> const & lines;
        };

        return _data 
            | std::views::filter([interval](auto const & pair) {
                return pair.first >= interval.start && pair.first <= interval.end;
              })
            | std::views::transform([](auto const & pair) {
                return TimeLinesPair{pair.first, pair.second};
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
    * @param target_timeframe The timeframe that this line data uses
    * @return A view of time-lines pairs for times within the converted interval range
    */
    [[nodiscard]] auto GetLinesInRange(TimeFrameInterval const & interval,
                                       std::shared_ptr<TimeFrame> source_timeframe,
                                       std::shared_ptr<TimeFrame> target_timeframe) const {
        // If the timeframes are the same object, no conversion is needed
        if (source_timeframe.get() == target_timeframe.get()) {
            return GetLinesInRange(interval);
        }

        // If either timeframe is null, fall back to original behavior
        if (!source_timeframe || !target_timeframe) {
            return GetLinesInRange(interval);
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
        return GetLinesInRange(target_interval);
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
    std::size_t copyTo(LineData& target, TimeFrameInterval const & interval, bool notify = true) const;

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
    std::size_t copyTo(LineData& target, std::vector<TimeFrameIndex> const & times, bool notify = true) const;

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
    std::size_t moveTo(LineData& target, TimeFrameInterval const & interval, bool notify = true);

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
    std::size_t moveTo(LineData& target, std::vector<TimeFrameIndex> const & times, bool notify = true);

protected:
private:
    std::map<TimeFrameIndex, std::vector<Line2D>> _data;
    std::vector<Line2D> _empty{};
    ImageSize _image_size;
};


#endif// LINE_DATA_HPP
