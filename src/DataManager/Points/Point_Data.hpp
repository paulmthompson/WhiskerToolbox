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

    // ========== Image Size ==========
    /*
    Image size is used to store the size of the image that the points belong to.
    */

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
};

#endif// POINT_DATA_HPP
