#ifndef POINT_DATA_HPP
#define POINT_DATA_HPP

#include "ImageSize/ImageSize.hpp"
#include "Observer/Observer_Data.hpp"
#include "points.hpp"

#include <map>
#include <ranges>
#include <vector>

/*
 * @brief PointData
 *
 * PointData is used for storing 2D points
 * There are specialized classes for handling groupings of points
 * LineData for ordered points, MaskData for unordered points
 * Both Mask and LineData assume that the points form a single object
 * A PointData object is used when the points are not part of a single object
 * For example, keypoints for multiple body points could be a PointData object
 */
class PointData : public ObserverData {
public:
    PointData() = default;
    explicit PointData(std::map<int, Point2D<float>> const & data);
    explicit PointData(std::map<int, std::vector<Point2D<float>>> data);
    void clearPointsAtTime(size_t time, bool notify = true);

    void addPointAtTime(size_t time, Point2D<float> point, bool notify = true);
    void addPointsAtTime(size_t time, std::vector<Point2D<float>> const & points, bool notify = true);

    void overwritePointAtTime(size_t time, Point2D<float> point, bool notify = true);
    void overwritePointsAtTime(size_t time, std::vector<Point2D<float>> const & points, bool notify = true);

    void overwritePointsAtTimes(
            std::vector<size_t> const & times,
            std::vector<std::vector<Point2D<float>>> const & points,
            bool notify = true);

    [[nodiscard]] std::vector<size_t> getTimesWithPoints() const;

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

    [[nodiscard]] std::vector<Point2D<float>> const & getPointsAtTime(size_t time) const;

    [[nodiscard]] std::size_t getMaxPoints() const;

    /**
    * @brief Get all points with their associated times as a range
    *
    * @return A view of time-points pairs for all times
    */
    [[nodiscard]] auto GetAllPointsAsRange() const {
        struct TimePointsPair {
            size_t time;
            std::vector<Point2D<float>> const & points;
        };

        return std::views::iota(size_t{0}, _time.size()) |
               std::views::transform([this](size_t i) {
                   return TimePointsPair{_time[i], _data[i]};
               });
    }

protected:
private:
    std::vector<std::vector<Point2D<float>>> _data;
    std::vector<size_t> _time; // represents index in clock signal
    std::vector<Point2D<float>> _empty;

    ImageSize _image_size;

};


#endif// POINT_DATA_HPP
