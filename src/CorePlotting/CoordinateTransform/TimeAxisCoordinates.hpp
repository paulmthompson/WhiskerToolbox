#ifndef COREPLOTTING_COORDINATETRANSFORM_TIMEAXISCOORDINATES_HPP
#define COREPLOTTING_COORDINATETRANSFORM_TIMEAXISCOORDINATES_HPP

#include "TimeRange.hpp"
#include <cstdint>

namespace CorePlotting {

/**
 * @brief Parameters for time axis coordinate conversions
 * 
 * This struct bundles all necessary information for converting between
 * canvas pixel coordinates and time coordinates. It is designed to be
 * lightweight and easily constructed from TimeRange and viewport dimensions.
 * 
 * All time values are in the same units as TimeRange (typically TimeFrameIndex).
 */
struct TimeAxisParams {
    int64_t time_start{0};      ///< Start of visible time range (from TimeRange.start)
    int64_t time_end{0};        ///< End of visible time range (from TimeRange.end)
    int viewport_width_px{1};   ///< Canvas width in pixels
    
    /**
     * @brief Default constructor
     */
    TimeAxisParams() = default;
    
    /**
     * @brief Construct from explicit values
     * @param start Start time coordinate
     * @param end End time coordinate
     * @param width Viewport width in pixels
     */
    TimeAxisParams(int64_t start, int64_t end, int width)
        : time_start(start), time_end(end), viewport_width_px(width) {}
    
    /**
     * @brief Construct from TimeRange and viewport width
     * @param range TimeRange providing visible time bounds
     * @param width Viewport width in pixels
     */
    TimeAxisParams(TimeRange const& range, int width)
        : time_start(range.start), time_end(range.end), viewport_width_px(width) {}
    
    /**
     * @brief Get the time span of the visible range
     * @return time_end - time_start
     */
    [[nodiscard]] int64_t getTimeSpan() const {
        return time_end - time_start;
    }
};

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
 *       Cast to int64_t if integer time indices are needed.
 * 
 * Example:
 * @code
 * TimeAxisParams params(0, 1000, 800); // Time 0-1000, 800px wide canvas
 * float time = canvasXToTime(400.0f, params); // Returns 500.0f (middle of canvas)
 * @endcode
 */
[[nodiscard]] inline float canvasXToTime(float canvas_x, TimeAxisParams const& params) {
    if (params.viewport_width_px <= 0) {
        return static_cast<float>(params.time_start);
    }
    
    float const normalized_x = canvas_x / static_cast<float>(params.viewport_width_px);
    float const time_span = static_cast<float>(params.time_end - params.time_start);
    
    return static_cast<float>(params.time_start) + normalized_x * time_span;
}

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
[[nodiscard]] inline float timeToCanvasX(float time, TimeAxisParams const& params) {
    float const time_span = static_cast<float>(params.time_end - params.time_start);
    
    if (time_span <= 0.0f) {
        return 0.0f;
    }
    
    float const normalized_x = (time - static_cast<float>(params.time_start)) / time_span;
    return normalized_x * static_cast<float>(params.viewport_width_px);
}

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
[[nodiscard]] inline float timeToNDC(float time, TimeAxisParams const& params) {
    float const time_span = static_cast<float>(params.time_end - params.time_start);
    
    if (time_span <= 0.0f) {
        return 0.0f;
    }
    
    // Map [time_start, time_end] to [-1, +1]
    float const normalized = (time - static_cast<float>(params.time_start)) / time_span;
    return 2.0f * normalized - 1.0f;
}

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
[[nodiscard]] inline float ndcToTime(float ndc_x, TimeAxisParams const& params) {
    // Map [-1, +1] to [time_start, time_end]
    float const normalized = (ndc_x + 1.0f) / 2.0f;
    float const time_span = static_cast<float>(params.time_end - params.time_start);
    
    return static_cast<float>(params.time_start) + normalized * time_span;
}

/**
 * @brief Calculate pixels per time unit
 * 
 * Useful for determining how much screen space a time interval covers,
 * or for scaling glyphs/markers based on current zoom level.
 * 
 * @param params Time axis parameters
 * @return Pixels per time unit (can be fractional)
 */
[[nodiscard]] inline float pixelsPerTimeUnit(TimeAxisParams const& params) {
    float const time_span = static_cast<float>(params.time_end - params.time_start);
    
    if (time_span <= 0.0f) {
        return 0.0f;
    }
    
    return static_cast<float>(params.viewport_width_px) / time_span;
}

/**
 * @brief Calculate time units per pixel
 * 
 * Inverse of pixelsPerTimeUnit. Useful for determining tolerance
 * values for hit testing (e.g., "click within 5 pixels" → "click within 5 * timeUnitsPerPixel").
 * 
 * @param params Time axis parameters
 * @return Time units per pixel
 */
[[nodiscard]] inline float timeUnitsPerPixel(TimeAxisParams const& params) {
    if (params.viewport_width_px <= 0) {
        return 0.0f;
    }
    
    float const time_span = static_cast<float>(params.time_end - params.time_start);
    return time_span / static_cast<float>(params.viewport_width_px);
}

/**
 * @brief Convenience method to create TimeAxisParams from TimeRange
 * 
 * @param range TimeRange containing visible time bounds
 * @param viewport_width Viewport width in pixels
 * @return TimeAxisParams ready for coordinate conversion
 */
[[nodiscard]] inline TimeAxisParams makeTimeAxisParams(TimeRange const& range, int viewport_width) {
    return TimeAxisParams(range, viewport_width);
}

} // namespace CorePlotting

#endif // COREPLOTTING_COORDINATETRANSFORM_TIMEAXISCOORDINATES_HPP
