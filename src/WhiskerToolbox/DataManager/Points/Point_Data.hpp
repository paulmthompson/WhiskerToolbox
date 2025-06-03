#ifndef POINT_DATA_HPP
#define POINT_DATA_HPP

#include "ImageSize/ImageSize.hpp"
#include "Observer/Observer_Data.hpp"
#include "Points/points.hpp"

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
    PointData() = default;
    explicit PointData(std::map<int, Point2D<float>> const & data);
    explicit PointData(std::map<int, std::vector<Point2D<float>>> const & data);
    
    void clearAtTime(int time, bool notify = true);

    void addPointAtTime(int time, Point2D<float> point, bool notify = true);
    void addPointsAtTime(int time, std::vector<Point2D<float>> const & points, bool notify = true);

    void overwritePointAtTime(int time, Point2D<float> point, bool notify = true);
    void overwritePointsAtTime(int time, std::vector<Point2D<float>> const & points, bool notify = true);

    void overwritePointsAtTimes(
            std::vector<int> const & times,
            std::vector<std::vector<Point2D<float>>> const & points,
            bool notify = true);

    [[nodiscard]] std::vector<int> getTimesWithData() const;

    [[nodiscard]] ImageSize getImageSize() const { return _image_size; }
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

    [[nodiscard]] std::vector<Point2D<float>> const & getPointsAtTime(int time) const;

    [[nodiscard]] std::size_t getMaxPoints() const;

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
    std::size_t copyTo(PointData& target, int start_time, int end_time, bool notify = true) const;

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
    std::size_t copyTo(PointData& target, std::vector<int> const& times, bool notify = true) const;

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
    std::size_t moveTo(PointData& target, int start_time, int end_time, bool notify = true);

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
    std::size_t moveTo(PointData& target, std::vector<int> const& times, bool notify = true);

    /**
    * @brief Get all points with their associated times as a range
    *
    * @return A view of time-points pairs for all times
    */
    [[nodiscard]] auto GetAllPointsAsRange() const {
        struct TimePointsPair {
            int time;
            std::vector<Point2D<float>> const & points;
        };

        return _data | std::views::transform([](auto const & pair) {
                   return TimePointsPair{pair.first, pair.second};
               });
    }

protected:
private:
    std::map<int, std::vector<Point2D<float>>> _data;
    std::vector<Point2D<float>> _empty{};
    ImageSize _image_size;
};

#endif// POINT_DATA_HPP
