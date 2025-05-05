#ifndef POINT_DATA_HPP
#define POINT_DATA_HPP

#include "ImageSize/ImageSize.hpp"
#include "Observer/Observer_Data.hpp"
#include "points.hpp"

#include <iostream>
#include <map>
#include <string>
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
    void clearPointsAtTime(int time);

    void addPointAtTime(int time, float x, float y);
    void addPointsAtTime(int time, std::vector<Point2D<float>> const & points);

    void overwritePointAtTime(int time, float x, float y);
    void overwritePointsAtTime(int time, std::vector<Point2D<float>> const & points);

    template<typename T>
    void overwritePointsAtTimes(
            std::vector<T> const & times,
            std::vector<std::vector<Point2D<float>>> const & points) {
        if (times.size() != points.size()) {
            std::cout << "overwritePointsAtTimes: times and points must be the same size" << std::endl;
            return;
        }

        for (std::size_t i = 0; i < times.size(); i++) {
            _overwritePointsAtTime(times[i], points[i]);
        }
        notifyObservers();
    }

    [[nodiscard]] std::vector<int> getTimesWithPoints() const;

    [[nodiscard]] ImageSize getImageSize() const { return _image_size; }
    void setImageSize(ImageSize const & image_size) { _image_size = image_size; }

    [[nodiscard]] std::vector<Point2D<float>> const & getPointsAtTime(int time) const;

    [[nodiscard]] std::size_t getMaxPoints() const {
        std::size_t max_points = 1;
        for (auto const & [time, points]: _data) {
            if (points.size() > max_points) {
                max_points = points.size();
            }
        }
        return max_points;
    }

    [[nodiscard]] std::map<int, std::vector<Point2D<float>>> const & getData() const { return _data; };

protected:
private:
    std::map<int, std::vector<Point2D<float>>> _data;
    std::vector<Point2D<float>> _empty;

    ImageSize _image_size;

    void _clearPointsAtTime(int time);
    void _addPointAtTime(int time, float x, float y);
    void _addPointsAtTime(int time, std::vector<Point2D<float>> const & points);

    void _overwritePointAtTime(int time, float x, float y);
    void _overwritePointsAtTime(int time, std::vector<Point2D<float>> const & points);
};


#endif// POINT_DATA_HPP
