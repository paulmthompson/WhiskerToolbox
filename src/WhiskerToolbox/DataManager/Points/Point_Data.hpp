#ifndef POINT_DATA_HPP
#define POINT_DATA_HPP

#include "ImageSize/ImageSize.hpp"
#include "Observer/Observer_Data.hpp"
#include "Points/points.hpp"
#include "TimeFrame.hpp"

#include <map>
#include <ranges>
#include <vector>

/**
 * @brief PointData
 *
 * PointData is used for storing 2D point collections at specific time frames.
 * Each time frame can contain multiple points, making it suitable for tracking
 * multiple features or keypoints over time.
 *
 * For example, keypoints for multiple body points could be a PointData object
 */
class PointData : public ObserverData {
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
    
    // ========== Setters ==========

    /**
     * @brief Clear the points at a specific time
     * 
     * @param time The time to clear the points at
     * @param notify If true, the observers will be notified
     */
    void clearAtTime(TimeFrameIndex time, bool notify = true);

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
    void addPointAtTime(TimeFrameIndex time, Point2D<float> point, bool notify = true);

    /**
     * @brief Add multiple points at a specific time
     * 
     * This will add multiple points at a specific time.
     * 
     * If the time does not exist, it will be created.
     * 
     * If the time already exists, the points will be added to the existing points.
     * 
     * @param time The time to add the points at
     * @param points The points to add
     * @param notify If true, the observers will be notified
     */
    void addPointsAtTime(TimeFrameIndex time, std::vector<Point2D<float>> const & points, bool notify = true);

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

    /**
     * @brief Overwrite multiple points at a specific time
     * 
     * This will overwrite multiple points at a specific time.
     * 
     * @param time The time to overwrite the points at
     * @param points The points to overwrite
     * @param notify If true, the observers will be notified
     */
    void overwritePointsAtTime(TimeFrameIndex time, std::vector<Point2D<float>> const & points, bool notify = true);

    /**
     * @brief Overwrite multiple points at multiple times
     * 
     * This will overwrite multiple points at multiple times.
     * 
     * @param times The times to overwrite the points at
     * @param points The points to overwrite
     * @param notify If true, the observers will be notified
     */
    void overwritePointsAtTimes(
            std::vector<TimeFrameIndex> const & times,
            std::vector<std::vector<Point2D<float>>> const & points,
            bool notify = true);

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
     * @brief Get all times with data
     * 
     * TODO: This could return std::ranges::views::keys for zero copy
     * 
     * @return A vector of TimeFrameIndex
     */
    [[nodiscard]] std::vector<TimeFrameIndex> getTimesWithData() const;

    /**
     * @brief Get the points at a specific time
     * 
     * If the time does not exist, an empty vector will be returned.
     * 
     * @param time The time to get the points at
     * @return A vector of Point2D<float>
     */
    [[nodiscard]] std::vector<Point2D<float>> const & getPointsAtTime(TimeFrameIndex time) const;

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
            std::vector<Point2D<float>> const & points;
        };

        return _data | std::views::transform([](auto const & pair) {
                   return TimePointsPair{TimeFrameIndex(pair.first), pair.second};
               });
    }

    // ======= Move and Copy ==========

    /**
     * @brief Copy points from this PointData to another PointData for a time range
     * 
     * Copies all points within the specified time range [start_time, end_time] (inclusive)
     * to the target PointData. If points already exist at target times, the copied points
     * are added to the existing points.
     * 
     * @param target The target PointData to copy points to
     * @param start_time The starting time (inclusive)
     * @param end_time The ending time (inclusive)
     * @param notify If true, the target will notify its observers after the operation
     * @return The number of points actually copied
     */
    std::size_t copyTo(PointData& target, TimeFrameIndex start_time, TimeFrameIndex end_time, bool notify = true) const;

    /**
     * @brief Copy points from this PointData to another PointData for specific times
     * 
     * Copies all points at the specified times to the target PointData.
     * If points already exist at target times, the copied points are added to the existing points.
     * 
     * @param target The target PointData to copy points to
     * @param times Vector of specific times to copy (does not need to be sorted)
     * @param notify If true, the target will notify its observers after the operation
     * @return The number of points actually copied
     */
    std::size_t copyTo(PointData& target, std::vector<TimeFrameIndex> const& times, bool notify = true) const;

    /**
     * @brief Move points from this PointData to another PointData for a time range
     * 
     * Moves all points within the specified time range [start_time, end_time] (inclusive)
     * to the target PointData. Points are copied to target then removed from source.
     * If points already exist at target times, the moved points are added to the existing points.
     * 
     * @param target The target PointData to move points to
     * @param start_time The starting time (inclusive)
     * @param end_time The ending time (inclusive)
     * @param notify If true, both source and target will notify their observers after the operation
     * @return The number of points actually moved
     */
    std::size_t moveTo(PointData& target, TimeFrameIndex start_time, TimeFrameIndex end_time, bool notify = true);

    /**
     * @brief Move points from this PointData to another PointData for specific times
     * 
     * Moves all points at the specified times to the target PointData.
     * Points are copied to target then removed from source.
     * If points already exist at target times, the moved points are added to the existing points.
     * 
     * @param target The target PointData to move points to
     * @param times Vector of specific times to move (does not need to be sorted)
     * @param notify If true, both source and target will notify their observers after the operation
     * @return The number of points actually moved
     */
    std::size_t moveTo(PointData& target, std::vector<TimeFrameIndex> const& times, bool notify = true);

protected:
private:
    std::map<TimeFrameIndex, std::vector<Point2D<float>>> _data;
    std::vector<Point2D<float>> _empty{};
    ImageSize _image_size;
};

#endif// POINT_DATA_HPP
