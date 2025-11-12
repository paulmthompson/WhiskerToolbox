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

    // ========== Getters (Time-based) ==========

    /**
     * @brief Get the maximum number of points at any time
     * 
     * @return The maximum number of points
     */
    [[nodiscard]] std::size_t getMaxPoints() const;
};

#endif// POINT_DATA_HPP
