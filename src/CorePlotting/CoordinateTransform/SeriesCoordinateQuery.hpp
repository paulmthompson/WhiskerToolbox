#ifndef COREPLOTTING_COORDINATETRANSFORM_SERIESCOORDINATEQUERY_HPP
#define COREPLOTTING_COORDINATETRANSFORM_SERIESCOORDINATEQUERY_HPP

#include "../Layout/LayoutEngine.hpp"
#include "../Layout/SeriesLayout.hpp"
#include <cmath>
#include <optional>
#include <string>

namespace CorePlotting {

/**
 * @brief Result of querying which series contains a given world Y coordinate
 * 
 * When a user clicks or hovers at a world position, this struct describes
 * which series (if any) the position falls within, along with the
 * position relative to that series' allocated region.
 */
struct SeriesQueryResult {
    std::string series_key;         ///< Key identifying the series
    float series_local_y{0.0f};     ///< Y coordinate relative to series center
    float normalized_y{0.0f};       ///< Y position normalized to [-1, +1] within series height
    bool is_within_bounds{false};   ///< Whether point is strictly within allocated region
    int series_index{-1};           ///< Index of series in layout (for ordering)
    
    /**
     * @brief Default constructor creates an invalid result
     */
    SeriesQueryResult() = default;
    
    /**
     * @brief Construct with all values
     */
    SeriesQueryResult(std::string key, float local_y, float norm_y, bool within, int index)
        : series_key(std::move(key))
        , series_local_y(local_y)
        , normalized_y(norm_y)
        , is_within_bounds(within)
        , series_index(index) {}
};

/**
 * @brief Find which series (if any) contains the given world Y coordinate
 * 
 * Queries the layout response to find the series whose allocated region
 * contains the specified world Y coordinate. Optionally allows a tolerance
 * for selecting series near the boundary.
 * 
 * @param world_y World Y coordinate to query
 * @param layout_response Layout containing all series positions
 * @param tolerance Additional distance beyond series bounds to consider a match
 * @return SeriesQueryResult if a series is found, std::nullopt otherwise
 * 
 * @note If multiple series overlap (shouldn't happen with proper layout),
 *       returns the first matching series.
 * 
 * Example:
 * @code
 * LayoutResponse layout = engine.compute(request);
 * float world_y = screenToWorld(mouse_pos, ...).y;
 * 
 * if (auto result = findSeriesAtWorldY(world_y, layout)) {
 *     std::cout << "Clicked on series: " << result->series_key << std::endl;
 *     std::cout << "Position within series: " << result->series_local_y << std::endl;
 * }
 * @endcode
 */
[[nodiscard]] inline std::optional<SeriesQueryResult> findSeriesAtWorldY(
    float world_y,
    LayoutResponse const& layout_response,
    float tolerance = 0.0f)
{
    for (auto const& series_layout : layout_response.layouts) {
        float const y_center = series_layout.result.allocated_y_center;
        float const half_height = series_layout.result.allocated_height / 2.0f;
        
        float const y_min = y_center - half_height - tolerance;
        float const y_max = y_center + half_height + tolerance;
        
        if (world_y >= y_min && world_y <= y_max) {
            float const local_y = world_y - y_center;
            float const normalized = (half_height > 0.0f) 
                ? local_y / half_height 
                : 0.0f;
            
            // Check if strictly within bounds (no tolerance)
            bool const strictly_within = 
                world_y >= (y_center - half_height) && 
                world_y <= (y_center + half_height);
            
            return SeriesQueryResult(
                series_layout.series_id,
                local_y,
                normalized,
                strictly_within,
                series_layout.series_index
            );
        }
    }
    
    return std::nullopt;
}

/**
 * @brief Find the closest series to a given world Y coordinate
 * 
 * Unlike findSeriesAtWorldY which requires the point to be within a series,
 * this function always returns a result (unless layout is empty) by finding
 * the series whose center is closest to the query point.
 * 
 * @param world_y World Y coordinate to query
 * @param layout_response Layout containing all series positions
 * @return SeriesQueryResult for the closest series, std::nullopt if layout is empty
 * 
 * Example:
 * @code
 * // User clicked between two series - find the closest one
 * if (auto result = findClosestSeriesAtWorldY(world_y, layout)) {
 *     highlightSeries(result->series_key);
 * }
 * @endcode
 */
[[nodiscard]] inline std::optional<SeriesQueryResult> findClosestSeriesAtWorldY(
    float world_y,
    LayoutResponse const& layout_response)
{
    if (layout_response.layouts.empty()) {
        return std::nullopt;
    }
    
    SeriesLayout const* closest = nullptr;
    float min_distance = std::numeric_limits<float>::max();
    
    for (auto const& series_layout : layout_response.layouts) {
        float const distance = std::abs(world_y - series_layout.result.allocated_y_center);
        if (distance < min_distance) {
            min_distance = distance;
            closest = &series_layout;
        }
    }
    
    if (!closest) {
        return std::nullopt;
    }
    
    float const y_center = closest->result.allocated_y_center;
    float const half_height = closest->result.allocated_height / 2.0f;
    float const local_y = world_y - y_center;
    float const normalized = (half_height > 0.0f) ? local_y / half_height : 0.0f;
    
    bool const within_bounds = 
        world_y >= (y_center - half_height) && 
        world_y <= (y_center + half_height);
    
    return SeriesQueryResult(
        closest->series_id,
        local_y,
        normalized,
        within_bounds,
        closest->series_index
    );
}

/**
 * @brief Convert world Y coordinate to series-local Y coordinate
 * 
 * Simple utility to convert from world coordinates to a position
 * relative to a specific series' center. This is the first step in
 * converting to actual data values (the data object then interprets
 * the local Y based on its own scaling properties).
 * 
 * @param world_y World Y coordinate
 * @param series_layout Layout information for the target series
 * @return Y coordinate relative to series center
 * 
 * Example:
 * @code
 * float local_y = worldYToSeriesLocalY(world_y, layout);
 * // The data object then converts local_y to data value using its own scaling
 * float data_value = series->localYToDataValue(local_y);
 * @endcode
 */
[[nodiscard]] inline float worldYToSeriesLocalY(
    float world_y,
    SeriesLayout const& series_layout)
{
    return world_y - series_layout.result.allocated_y_center;
}

/**
 * @brief Convert series-local Y coordinate back to world Y coordinate
 * 
 * Inverse of worldYToSeriesLocalY.
 * 
 * @param local_y Y coordinate relative to series center
 * @param series_layout Layout information for the target series
 * @return World Y coordinate
 */
[[nodiscard]] inline float seriesLocalYToWorldY(
    float local_y,
    SeriesLayout const& series_layout)
{
    return local_y + series_layout.result.allocated_y_center;
}

/**
 * @brief Get the bounds of a series in world coordinates
 * 
 * Utility to extract the Y-axis bounds of a series' allocated region.
 * 
 * @param series_layout Layout information for the series
 * @return Pair of (y_min, y_max) in world coordinates
 */
[[nodiscard]] inline std::pair<float, float> getSeriesWorldBounds(
    SeriesLayout const& series_layout)
{
    float const y_center = series_layout.result.allocated_y_center;
    float const half_height = series_layout.result.allocated_height / 2.0f;
    return {y_center - half_height, y_center + half_height};
}

/**
 * @brief Check if a world Y coordinate is within a series' bounds
 * 
 * @param world_y World Y coordinate to check
 * @param series_layout Layout information for the series
 * @param tolerance Additional tolerance beyond strict bounds
 * @return true if world_y is within the series' allocated region
 */
[[nodiscard]] inline bool isWithinSeriesBounds(
    float world_y,
    SeriesLayout const& series_layout,
    float tolerance = 0.0f)
{
    auto [y_min, y_max] = getSeriesWorldBounds(series_layout);
    return world_y >= (y_min - tolerance) && world_y <= (y_max + tolerance);
}

/**
 * @brief Convert normalized series Y [-1, +1] to world Y
 * 
 * Given a normalized position within a series (where -1 is bottom edge,
 * +1 is top edge, and 0 is center), convert to world coordinates.
 * 
 * @param normalized_y Normalized position in range [-1, +1]
 * @param series_layout Layout information for the series
 * @return World Y coordinate
 */
[[nodiscard]] inline float normalizedSeriesYToWorldY(
    float normalized_y,
    SeriesLayout const& series_layout)
{
    float const half_height = series_layout.result.allocated_height / 2.0f;
    return series_layout.result.allocated_y_center + normalized_y * half_height;
}

/**
 * @brief Convert world Y to normalized series Y [-1, +1]
 * 
 * Inverse of normalizedSeriesYToWorldY. Returns 0 if series has zero height.
 * 
 * @param world_y World Y coordinate
 * @param series_layout Layout information for the series
 * @return Normalized position (0 at center, ±1 at edges)
 */
[[nodiscard]] inline float worldYToNormalizedSeriesY(
    float world_y,
    SeriesLayout const& series_layout)
{
    float const half_height = series_layout.result.allocated_height / 2.0f;
    if (half_height <= 0.0f) {
        return 0.0f;
    }
    float const local_y = world_y - series_layout.result.allocated_y_center;
    return local_y / half_height;
}

} // namespace CorePlotting

#endif // COREPLOTTING_COORDINATETRANSFORM_SERIESCOORDINATEQUERY_HPP
