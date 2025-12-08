#ifndef LINE_DATA_BUILDER_HPP
#define LINE_DATA_BUILDER_HPP

#include "Lines/Line_Data.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <memory>
#include <vector>
#include <map>
#include <cmath>

/**
 * @brief Helper functions for creating common line shapes
 */
namespace line_shapes {

/**
 * @brief Create a horizontal line
 * @param x_start Starting x coordinate
 * @param x_end Ending x coordinate
 * @param y Y coordinate (constant)
 * @param num_points Number of points along the line
 */
inline Line2D horizontal(float x_start, float x_end, float y, size_t num_points = 4) {
    std::vector<Point2D<float>> points;
    float step = (x_end - x_start) / static_cast<float>(num_points - 1);
    for (size_t i = 0; i < num_points; ++i) {
        points.emplace_back(x_start + step * static_cast<float>(i), y);
    }
    return Line2D(points);
}

/**
 * @brief Create a vertical line
 * @param x X coordinate (constant)
 * @param y_start Starting y coordinate
 * @param y_end Ending y coordinate
 * @param num_points Number of points along the line
 */
inline Line2D vertical(float x, float y_start, float y_end, size_t num_points = 4) {
    std::vector<Point2D<float>> points;
    float step = (y_end - y_start) / static_cast<float>(num_points - 1);
    for (size_t i = 0; i < num_points; ++i) {
        points.emplace_back(x, y_start + step * static_cast<float>(i));
    }
    return Line2D(points);
}

/**
 * @brief Create a diagonal line at 45 degrees
 * @param x_start Starting x coordinate
 * @param y_start Starting y coordinate
 * @param length Length of the line
 * @param num_points Number of points along the line
 */
inline Line2D diagonal(float x_start, float y_start, float length, size_t num_points = 4) {
    std::vector<Point2D<float>> points;
    float step = length / static_cast<float>(num_points - 1);
    for (size_t i = 0; i < num_points; ++i) {
        float offset = step * static_cast<float>(i);
        points.emplace_back(x_start + offset, y_start + offset);
    }
    return Line2D(points);
}

/**
 * @brief Create a line from explicit points
 * @param x_coords X coordinates
 * @param y_coords Y coordinates (must match x_coords size)
 */
inline Line2D from_coords(const std::vector<float>& x_coords, const std::vector<float>& y_coords) {
    std::vector<Point2D<float>> points;
    size_t n = std::min(x_coords.size(), y_coords.size());
    for (size_t i = 0; i < n; ++i) {
        points.emplace_back(x_coords[i], y_coords[i]);
    }
    return Line2D(points);
}

/**
 * @brief Create a parabolic curve (y = ax^2 + bx + c)
 * @param x_start Starting x coordinate
 * @param x_end Ending x coordinate
 * @param a Quadratic coefficient
 * @param b Linear coefficient
 * @param c Constant coefficient
 * @param num_points Number of points along the curve
 */
inline Line2D parabola(float x_start, float x_end, float a, float b, float c, size_t num_points = 6) {
    std::vector<Point2D<float>> points;
    float step = (x_end - x_start) / static_cast<float>(num_points - 1);
    for (size_t i = 0; i < num_points; ++i) {
        float x = x_start + step * static_cast<float>(i);
        float y = a * x * x + b * x + c;
        points.emplace_back(x, y);
    }
    return Line2D(points);
}

/**
 * @brief Create a circular arc
 * @param center_x Center x coordinate
 * @param center_y Center y coordinate
 * @param radius Radius
 * @param start_angle Starting angle in radians
 * @param end_angle Ending angle in radians
 * @param num_points Number of points along the arc
 */
inline Line2D arc(float center_x, float center_y, float radius, 
                  float start_angle, float end_angle, size_t num_points = 8) {
    std::vector<Point2D<float>> points;
    float angle_step = (end_angle - start_angle) / static_cast<float>(num_points - 1);
    for (size_t i = 0; i < num_points; ++i) {
        float angle = start_angle + angle_step * static_cast<float>(i);
        points.emplace_back(center_x + radius * std::cos(angle), 
                           center_y + radius * std::sin(angle));
    }
    return Line2D(points);
}

/**
 * @brief Create an empty line
 */
inline Line2D empty() {
    return Line2D({});
}

} // namespace line_shapes

/**
 * @brief Lightweight builder for LineData objects
 * 
 * Provides fluent API for constructing LineData test data with common
 * line shapes, without requiring DataManager.
 * 
 * @example Simple horizontal line
 * @code
 * auto line_data = LineDataBuilder()
 *     .atTime(0, line_shapes::horizontal(0, 10, 5))
 *     .build();
 * @endcode
 * 
 * @example Multiple lines at different times
 * @code
 * auto line_data = LineDataBuilder()
 *     .atTime(0, line_shapes::horizontal(0, 10, 5))
 *     .atTime(10, line_shapes::vertical(5, 0, 10))
 *     .build();
 * @endcode
 */
class LineDataBuilder {
public:
    LineDataBuilder() = default;

    /**
     * @brief Add a line at a specific time
     * @param time Time index
     * @param line Line2D to add
     */
    LineDataBuilder& atTime(int time, const Line2D& line) {
        m_lines_by_time[TimeFrameIndex(time)].push_back(line);
        return *this;
    }

    /**
     * @brief Add a line at a specific time (TimeFrameIndex version)
     * @param time Time index
     * @param line Line2D to add
     */
    LineDataBuilder& atTime(TimeFrameIndex time, const Line2D& line) {
        m_lines_by_time[time].push_back(line);
        return *this;
    }

    /**
     * @brief Add multiple lines at a specific time
     * @param time Time index
     * @param lines Vector of lines to add
     */
    LineDataBuilder& atTime(int time, const std::vector<Line2D>& lines) {
        auto& vec = m_lines_by_time[TimeFrameIndex(time)];
        vec.insert(vec.end(), lines.begin(), lines.end());
        return *this;
    }

    /**
     * @brief Add a horizontal line at a specific time
     * @param time Time index
     * @param x_start Starting x coordinate
     * @param x_end Ending x coordinate
     * @param y Y coordinate (constant)
     * @param num_points Number of points along the line
     */
    LineDataBuilder& withHorizontal(int time, float x_start, float x_end, float y, size_t num_points = 4) {
        return atTime(time, line_shapes::horizontal(x_start, x_end, y, num_points));
    }

    /**
     * @brief Add a vertical line at a specific time
     * @param time Time index
     * @param x X coordinate (constant)
     * @param y_start Starting y coordinate
     * @param y_end Ending y coordinate
     * @param num_points Number of points along the line
     */
    LineDataBuilder& withVertical(int time, float x, float y_start, float y_end, size_t num_points = 4) {
        return atTime(time, line_shapes::vertical(x, y_start, y_end, num_points));
    }

    /**
     * @brief Add a diagonal line at a specific time
     * @param time Time index
     * @param x_start Starting x coordinate
     * @param y_start Starting y coordinate
     * @param length Length of the line
     * @param num_points Number of points along the line
     */
    LineDataBuilder& withDiagonal(int time, float x_start, float y_start, float length, size_t num_points = 4) {
        return atTime(time, line_shapes::diagonal(x_start, y_start, length, num_points));
    }

    /**
     * @brief Add a line from explicit coordinates at a specific time
     * @param time Time index
     * @param x_coords X coordinates
     * @param y_coords Y coordinates
     */
    LineDataBuilder& withCoords(int time, const std::vector<float>& x_coords, const std::vector<float>& y_coords) {
        return atTime(time, line_shapes::from_coords(x_coords, y_coords));
    }

    /**
     * @brief Set image size for the line data
     * @param width Image width
     * @param height Image height
     */
    LineDataBuilder& withImageSize(uint32_t width, uint32_t height) {
        m_image_width = width;
        m_image_height = height;
        m_has_image_size = true;
        return *this;
    }

    /**
     * @brief Build the LineData
     * @return Shared pointer to constructed LineData
     */
    std::shared_ptr<LineData> build() const {
        auto line_data = std::make_shared<LineData>();
        
        for (const auto& [time, lines] : m_lines_by_time) {
            for (const auto& line : lines) {
                line_data->addAtTime(time, line, NotifyObservers::No);
            }
        }
        
        if (m_has_image_size) {
            line_data->setImageSize(ImageSize(m_image_width, m_image_height));
        }
        
        return line_data;
    }

private:
    std::map<TimeFrameIndex, std::vector<Line2D>> m_lines_by_time;
    uint32_t m_image_width = 800;
    uint32_t m_image_height = 600;
    bool m_has_image_size = false;
};

#endif // LINE_DATA_BUILDER_HPP
