#ifndef COREPLOTTING_COORDINATETRANSFORM_TIMEAXISCOORDINATES_HPP
#define COREPLOTTING_COORDINATETRANSFORM_TIMEAXISCOORDINATES_HPP

#include "TimeFrame/ClockTicks.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <cstdint>
#include <utility>

namespace CorePlotting {

/**
 * @brief Parameters for time axis coordinate conversions
 *
 * Bundles information for converting between canvas pixel coordinates and
 * physical time (clock ticks). Endpoints are absolute @c ClockTicks in the
 * master clock — the same scalar space as @c TimeFrame::getTimeAtIndex.
 *
 * Construct via explicit tick endpoints, or @c makeTimeAxisParams when the
 * visible window is expressed as master @c TimeFrameIndex values.
 */
struct TimeAxisParams {
    ClockTicks time_start{0};///< Start of visible time range (absolute clock ticks)
    ClockTicks time_end{0};  ///< End of visible time range (absolute clock ticks)
    int viewport_width_px{1};///< Canvas width in pixels

    /**
     * @brief Default constructor
     */
    TimeAxisParams() = default;

    /**
     * @brief Construct from explicit clock-tick endpoints
     * @param start Start time in absolute clock ticks
     * @param end End time in absolute clock ticks
     * @param width Viewport width in pixels
     */
    TimeAxisParams(ClockTicks start, ClockTicks end, int width)
        : time_start(start),
          time_end(end),
          viewport_width_px(width) {}

    /**
     * @brief Get the time span of the visible range in clock ticks
     * @return @c time_end - @c time_start
     */
    [[nodiscard]] int64_t getTimeSpan() const {
        return time_end - time_start;
    }
};

/**
 * @brief Parameters for Y-axis coordinate conversions
 *
 * Bundles information for converting between canvas Y pixel coordinates
 * and world Y coordinates. Accounts for viewport bounds and pan offset.
 *
 * Note: Unlike time axis (which maps to time values), Y-axis maps to
 * "world" coordinates. Converting world Y to actual data values requires
 * additional series-specific transforms (see worldYToAnalogValue).
 */
struct YAxisParams {
    float world_y_min{-1.0f}; ///< Minimum world Y coordinate
    float world_y_max{1.0f};  ///< Maximum world Y coordinate
    float pan_offset{0.0f};   ///< Vertical pan offset (applied to world bounds)
    int viewport_height_px{1};///< Canvas height in pixels

    /**
     * @brief Default constructor
     */
    YAxisParams() = default;

    /**
     * @brief Construct from explicit values
     * @param y_min Minimum world Y coordinate
     * @param y_max Maximum world Y coordinate
     * @param height Viewport height in pixels
     * @param pan Vertical pan offset (default 0)
     */
    YAxisParams(float y_min, float y_max, int height, float pan = 0.0f)
        : world_y_min(y_min),
          world_y_max(y_max),
          pan_offset(pan),
          viewport_height_px(height) {}

    /**
     * @brief Get the effective Y range accounting for pan offset
     * @return Pair of (effective_y_min, effective_y_max)
     */
    [[nodiscard]] std::pair<float, float> getEffectiveRange() const {
        return {world_y_min + pan_offset, world_y_max + pan_offset};
    }
};

/**
 * @brief Build @c TimeAxisParams from a master-frame index window
 *
 * Converts inclusive master @c TimeFrameIndex bounds to absolute clock ticks
 * via @c TimeFrame::getTimeAtIndex.
 *
 * @pre @p start and @p end are valid indices into @p master_tf
 * @param start Inclusive window start as master @c TimeFrameIndex
 * @param end Inclusive window end as master @c TimeFrameIndex
 * @param master_tf Master time frame for index-to-tick conversion
 * @param viewport_width Viewport width in pixels
 * @return TimeAxisParams with tick endpoints ready for coordinate conversion
 */
[[nodiscard]] inline TimeAxisParams makeTimeAxisParams(
        TimeFrameIndex start,
        TimeFrameIndex end,
        TimeFrame const & master_tf,
        int viewport_width) {
    return TimeAxisParams(
            master_tf.getTimeAtIndex(start),
            master_tf.getTimeAtIndex(end),
            viewport_width);
}

/**
 * @brief Convert canvas X pixel coordinate to time coordinate
 *
 * Maps a pixel position on the canvas to the corresponding time value
 * based on the current visible time range.
 *
 * @param canvas_x X position in pixels (0 = left edge of canvas)
 * @param params Time axis parameters (time range and viewport width)
 * @return Time coordinate corresponding to canvas_x (floating point for sub-frame precision)
 *
 * @note Returns floating point to allow sub-frame precision for interpolation.
 *       Cast to int64_t if integer tick values are needed.
 *
 * Example:
 * @code
 * TimeAxisParams params(0, 1000, 800); // Time 0-1000, 800px wide canvas
 * float time = canvasXToTime(400.0f, params); // Returns 500.0f (middle of canvas)
 * @endcode
 */
[[nodiscard]] float canvasXToTime(float canvas_x, TimeAxisParams const & params);

/**
 * @brief Convert time coordinate to canvas X pixel coordinate
 *
 * Maps a time value to the corresponding pixel position on the canvas
 * based on the current visible time range.
 *
 * @param time Time coordinate
 * @param params Time axis parameters (time range and viewport width)
 * @return Canvas X position in pixels (0 = left edge)
 *
 * @note Times outside the visible range will return negative values or
 *       values greater than viewport_width_px.
 *
 * Example:
 * @code
 * TimeAxisParams params(0, 1000, 800); // Time 0-1000, 800px wide canvas
 * float x = timeToCanvasX(500.0f, params); // Returns 400.0f (middle of canvas)
 * @endcode
 */
[[nodiscard]] float timeToCanvasX(float time, TimeAxisParams const & params);

/**
 * @brief Convert time coordinate to Normalized Device Coordinate (NDC)
 *
 * Maps a time value to NDC range [-1, +1] for use with projection matrices.
 * Time at time_start maps to -1, time at time_end maps to +1.
 *
 * @param time Time coordinate
 * @param params Time axis parameters (time range)
 * @return NDC X coordinate in range [-1, +1]
 *
 * @note The viewport_width_px field is not used for this conversion.
 *
 * Example:
 * @code
 * TimeAxisParams params(100, 200, 800);
 * float ndc = timeToNDC(150.0f, params); // Returns 0.0f (center)
 * float ndc_left = timeToNDC(100.0f, params); // Returns -1.0f
 * float ndc_right = timeToNDC(200.0f, params); // Returns +1.0f
 * @endcode
 */
[[nodiscard]] float timeToNDC(float time, TimeAxisParams const & params);

/**
 * @brief Convert NDC X coordinate to time coordinate
 *
 * Inverse of timeToNDC. Maps NDC range [-1, +1] back to time coordinates.
 *
 * @param ndc_x NDC X coordinate (typically in range [-1, +1])
 * @param params Time axis parameters (time range)
 * @return Time coordinate
 *
 * Example:
 * @code
 * TimeAxisParams params(100, 200, 800);
 * float time = ndcToTime(0.0f, params); // Returns 150.0f (center)
 * @endcode
 */
[[nodiscard]] float ndcToTime(float ndc_x, TimeAxisParams const & params);

/**
 * @brief Calculate pixels per time unit
 *
 * Useful for determining how much screen space a time interval covers,
 * or for scaling glyphs/markers based on current zoom level.
 *
 * @param params Time axis parameters
 * @return Pixels per time unit (can be fractional)
 */
[[nodiscard]] float pixelsPerTimeUnit(TimeAxisParams const & params);

/**
 * @brief Calculate time units per pixel
 *
 * Inverse of pixelsPerTimeUnit. Useful for determining tolerance
 * values for hit testing (e.g., "click within 5 pixels" → "click within 5 * timeUnitsPerPixel).
 *
 * @param params Time axis parameters
 * @return Time units per pixel
 */
[[nodiscard]] float timeUnitsPerPixel(TimeAxisParams const & params);

// ============================================================================
// Y-Axis Coordinate Conversions
// ============================================================================

/**
 * @brief Convert canvas Y pixel coordinate to world Y coordinate
 *
 * Maps a pixel position on the canvas to the corresponding world Y value.
 * Canvas coordinates have origin at top-left with Y increasing downward.
 * World coordinates have Y increasing upward.
 *
 * @param canvas_y Y position in pixels (0 = top of canvas)
 * @param params Y axis parameters (world bounds, viewport height, pan offset)
 * @return World Y coordinate
 *
 * Example:
 * @code
 * YAxisParams params(-1.0f, 1.0f, 600); // World Y from -1 to 1, 600px tall canvas
 * float world_y = canvasYToWorldY(300.0f, params); // Returns 0.0f (middle)
 * float world_y_top = canvasYToWorldY(0.0f, params); // Returns 1.0f (top)
 * float world_y_bot = canvasYToWorldY(600.0f, params); // Returns -1.0f (bottom)
 * @endcode
 */
[[nodiscard]] float canvasYToWorldY(float canvas_y, YAxisParams const & params);

/**
 * @brief Convert world Y coordinate to canvas Y pixel coordinate
 *
 * Inverse of canvasYToWorldY. Maps world Y to canvas pixel position.
 *
 * @param world_y World Y coordinate
 * @param params Y axis parameters
 * @return Canvas Y position in pixels (0 = top of canvas)
 */
[[nodiscard]] float worldYToCanvasY(float world_y, YAxisParams const & params);

/**
 * @brief Convert world Y to Normalized Device Coordinate (NDC)
 *
 * Maps a world Y value to NDC range [-1, +1] for use with projection matrices.
 *
 * @param world_y World Y coordinate
 * @param params Y axis parameters (pan offset is applied)
 * @return NDC Y coordinate in range [-1, +1]
 */
[[nodiscard]] float worldYToNDC(float world_y, YAxisParams const & params);

/**
 * @brief Convert NDC Y coordinate to world Y coordinate
 *
 * Inverse of worldYToNDC.
 *
 * @param ndc_y NDC Y coordinate (typically in range [-1, +1])
 * @param params Y axis parameters
 * @return World Y coordinate
 */
[[nodiscard]] float ndcToWorldY(float ndc_y, YAxisParams const & params);

}// namespace CorePlotting

#endif// COREPLOTTING_COORDINATETRANSFORM_TIMEAXISCOORDINATES_HPP
