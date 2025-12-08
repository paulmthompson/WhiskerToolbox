#ifndef POINT_DATA_BUILDER_HPP
#define POINT_DATA_BUILDER_HPP

#include "Points/Point_Data.hpp"
#include "CoreGeometry/points.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "constants.hpp"

#include <memory>
#include <vector>
#include <map>

/**
 * @brief Lightweight builder for PointData objects
 * 
 * Provides fluent API for constructing PointData test data with common
 * point patterns, without requiring DataManager.
 * 
 * @example Single point at one time
 * @code
 * auto point_data = PointDataBuilder()
 *     .withPoint(0, 5.0f, 10.0f)
 *     .build();
 * @endcode
 * 
 * @example Multiple points at different times
 * @code
 * auto point_data = PointDataBuilder()
 *     .withPoints(0, {{1.0f, 2.0f}, {3.0f, 4.0f}})
 *     .withPoint(10, 5.0f, 6.0f)
 *     .build();
 * @endcode
 */
class PointDataBuilder {
public:
    PointDataBuilder() = default;

    /**
     * @brief Add a single point at a specific time
     * @param time Time index
     * @param x X coordinate
     * @param y Y coordinate
     */
    PointDataBuilder& withPoint(int time, float x, float y) {
        m_points_by_time[TimeFrameIndex(time)].emplace_back(x, y);
        return *this;
    }

    /**
     * @brief Add a single point at a specific time (TimeFrameIndex version)
     * @param time Time index
     * @param x X coordinate
     * @param y Y coordinate
     */
    PointDataBuilder& withPoint(TimeFrameIndex time, float x, float y) {
        m_points_by_time[time].emplace_back(x, y);
        return *this;
    }

    /**
     * @brief Add a single point at a specific time (Point2D version)
     * @param time Time index
     * @param point Point to add
     */
    PointDataBuilder& withPoint(int time, const Point2D<float>& point) {
        m_points_by_time[TimeFrameIndex(time)].push_back(point);
        return *this;
    }

    /**
     * @brief Add multiple points at a specific time
     * @param time Time index
     * @param points Vector of points to add
     */
    PointDataBuilder& withPoints(int time, const std::vector<Point2D<float>>& points) {
        auto& vec = m_points_by_time[TimeFrameIndex(time)];
        vec.insert(vec.end(), points.begin(), points.end());
        return *this;
    }

    /**
     * @brief Add multiple points at a specific time (TimeFrameIndex version)
     * @param time Time index
     * @param points Vector of points to add
     */
    PointDataBuilder& withPoints(TimeFrameIndex time, const std::vector<Point2D<float>>& points) {
        auto& vec = m_points_by_time[time];
        vec.insert(vec.end(), points.begin(), points.end());
        return *this;
    }

    /**
     * @brief Add points at a specific time from separate x and y vectors
     * @param time Time index
     * @param x_coords X coordinates
     * @param y_coords Y coordinates (must match x_coords size)
     */
    PointDataBuilder& withCoords(int time, const std::vector<float>& x_coords, const std::vector<float>& y_coords) {
        size_t n = std::min(x_coords.size(), y_coords.size());
        auto& vec = m_points_by_time[TimeFrameIndex(time)];
        for (size_t i = 0; i < n; ++i) {
            vec.emplace_back(x_coords[i], y_coords[i]);
        }
        return *this;
    }

    /**
     * @brief Set image size for the point data
     * @param width Image width
     * @param height Image height
     */
    PointDataBuilder& withImageSize(uint32_t width, uint32_t height) {
        m_image_width = width;
        m_image_height = height;
        m_has_image_size = true;
        return *this;
    }

    /**
     * @brief Build the PointData
     * @return Shared pointer to constructed PointData
     */
    std::shared_ptr<PointData> build() const {
        auto point_data = std::make_shared<PointData>();
        
        for (const auto& [time, points] : m_points_by_time) {
            if (!points.empty()) {
                point_data->addAtTime(time, points, NotifyObservers::No);
            }
        }
        
        if (m_has_image_size) {
            point_data->setImageSize(ImageSize(m_image_width, m_image_height));
        }
        
        return point_data;
    }

private:
    std::map<TimeFrameIndex, std::vector<Point2D<float>>> m_points_by_time;
    uint32_t m_image_width = test_fixture_constants::DEFAULT_IMAGE_WIDTH;
    uint32_t m_image_height = test_fixture_constants::DEFAULT_IMAGE_HEIGHT;
    bool m_has_image_size = false;
};

#endif // POINT_DATA_BUILDER_HPP
