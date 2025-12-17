#ifndef COREPLOTTING_COORDINATETRANSFORM_TIMERANGE_HPP
#define COREPLOTTING_COORDINATETRANSFORM_TIMERANGE_HPP

#include "TimeFrame/TimeFrame.hpp"
#include "ViewState.hpp"
#include <cstdint>
#include <algorithm>

namespace CorePlotting {

/**
 * @brief Bounds-aware time range for X-axis display
 * 
 * This struct manages the visible time window for time-series plots,
 * integrating with TimeFrame to enforce valid data bounds.
 * 
 * Unlike the legacy XAxis class which operated independently,
 * TimeRange respects the underlying TimeFrame bounds to prevent
 * scrolling/zooming beyond the available data range.
 * 
 * Key features:
 * - Automatic clamping to TimeFrame bounds during zoom/pan
 * - Centered zoom operations
 * - Width-based zoom control
 * - Immutable bounds (set at construction)
 * 
 * @note All time values are in TimeFrameIndex units (integer indices)
 */
struct TimeRange {
    /// Current visible range (can be modified)
    int64_t start{0};
    int64_t end{0};
    
    /// Hard limits from TimeFrame (immutable after construction)
    int64_t min_bound{0};
    int64_t max_bound{0};
    
    /**
     * @brief Default constructor creates an empty range
     */
    TimeRange() = default;
    
    /**
     * @brief Construct with explicit bounds
     * 
     * @param start_val Initial visible start
     * @param end_val Initial visible end
     * @param min_bound_val Minimum allowed value (inclusive)
     * @param max_bound_val Maximum allowed value (inclusive)
     */
    TimeRange(int64_t start_val, int64_t end_val, 
              int64_t min_bound_val, int64_t max_bound_val)
        : start(start_val), end(end_val)
        , min_bound(min_bound_val), max_bound(max_bound_val)
    {
        // Ensure initial range is valid
        clampToValidRange();
    }
    
    /**
     * @brief Construct from a TimeFrame's valid range
     * 
     * Sets both visible range and bounds from the TimeFrame's extent.
     * Initial visible range spans the entire TimeFrame.
     * 
     * @param tf TimeFrame to extract bounds from
     * @return TimeRange initialized to show entire TimeFrame
     */
    static TimeRange fromTimeFrame(TimeFrame const& tf) {
        int64_t count = tf.getTotalFrameCount();
        // TimeFrame indices are 0-based, so valid range is [0, count-1]
        return TimeRange(0, count - 1, 0, count - 1);
    }
    
    /**
     * @brief Set visible range with automatic clamping to bounds
     * 
     * The provided range will be adjusted to fit within [min_bound, max_bound].
     * If the range is too wide, it will be clamped to the maximum available.
     * If the range would extend beyond bounds, it will be shifted inward.
     * 
     * @param new_start Desired start of visible range
     * @param new_end Desired end of visible range (inclusive)
     */
    void setVisibleRange(int64_t new_start, int64_t new_end) {
        start = new_start;
        end = new_end;
        clampToValidRange();
    }
    
    /**
     * @brief Zoom centered on a point, respecting bounds
     * 
     * Attempts to set a new visible range of the specified width,
     * centered on the given point. If the resulting range would
     * exceed bounds, it is shifted and/or clamped.
     * 
     * @param center Center point for zoom (in TimeFrameIndex units)
     * @param range_width Desired width of visible range
     * @return Actual range width after bounds enforcement
     * 
     * @note If the requested range_width exceeds the total data bounds,
     *       it will be clamped to show the entire available range.
     */
    int64_t setCenterAndZoom(int64_t center, int64_t range_width) {
        // Ensure minimum width of 1
        range_width = std::max(int64_t(1), range_width);
        
        // Clamp range_width to available data
        int64_t max_width = max_bound - min_bound + 1;
        range_width = std::min(range_width, max_width);
        
        // Calculate centered range
        int64_t half_width = range_width / 2;
        int64_t new_start = center - half_width;
        int64_t new_end = new_start + range_width - 1;
        
        // Shift if we're outside bounds
        if (new_start < min_bound) {
            int64_t shift = min_bound - new_start;
            new_start += shift;
            new_end += shift;
        } else if (new_end > max_bound) {
            int64_t shift = new_end - max_bound;
            new_start -= shift;
            new_end -= shift;
        }
        
        start = new_start;
        end = new_end;
        
        // Final safety clamp
        clampToValidRange();
        
        return getWidth();
    }
    
    /**
     * @brief Get visible range width
     * @return Number of time indices in current visible range (inclusive count)
     */
    [[nodiscard]] int64_t getWidth() const { 
        return end - start + 1; 
    }
    
    /**
     * @brief Get center of visible range
     * @return Center time index (rounded down for odd widths)
     */
    [[nodiscard]] int64_t getCenter() const {
        return start + (end - start) / 2;
    }
    
    /**
     * @brief Check if a time index is within the visible range
     * @param time_index Index to check
     * @return true if time_index is in [start, end] (inclusive)
     */
    [[nodiscard]] bool contains(int64_t time_index) const {
        return time_index >= start && time_index <= end;
    }
    
    /**
     * @brief Check if the visible range is at the lower bound limit
     */
    [[nodiscard]] bool isAtMinBound() const {
        return start <= min_bound;
    }
    
    /**
     * @brief Check if the visible range is at the upper bound limit
     */
    [[nodiscard]] bool isAtMaxBound() const {
        return end >= max_bound;
    }
    
    /**
     * @brief Get the total available data range
     * @return Width of the entire bounded region
     */
    [[nodiscard]] int64_t getTotalBoundedWidth() const {
        return max_bound - min_bound + 1;
    }

private:
    /**
     * @brief Internal helper to enforce bounds invariants
     * 
     * Ensures that:
     * 1. start >= min_bound
     * 2. end <= max_bound
     * 3. start <= end
     * 4. If range too wide, clamp to full bounds
     */
    void clampToValidRange() {
        // First ensure start and end are ordered
        if (start > end) {
            std::swap(start, end);
        }
        
        // Clamp to hard bounds
        start = std::clamp(start, min_bound, max_bound);
        end = std::clamp(end, min_bound, max_bound);
        
        // If we still violate ordering (can happen if bounds are tight)
        if (start > end) {
            end = start;
        }
    }
};

/**
 * @brief Extended ViewState for time-series plots
 * 
 * Combines the standard ViewState (Y-axis camera control) with
 * TimeRange (X-axis time-aware bounds management).
 * 
 * Usage pattern:
 * - ViewState zoom/pan operations apply to Y-axis only
 * - X-axis zoom/pan handled through time_range methods
 * - Projection matrix incorporates both ViewState and TimeRange
 * 
 * This separation allows independent control of spatial (Y) and
 * temporal (X) visualization parameters.
 * 
 * Y-AXIS STATE (Phase 4.7):
 * The y_min, y_max, and vertical_pan_offset fields replace the legacy
 * OpenGLWidget::_yMin, _yMax, _verticalPanOffset members. These are
 * used for:
 * - Projection matrix Y bounds (y_min, y_max in NDC)
 * - Pan offset for interactive vertical scrolling
 * - Global scaling factors applied uniformly to all series
 */
struct TimeSeriesViewState {
    ViewState view_state;     ///< Y-axis camera state (general)
    TimeRange time_range;     ///< X-axis time bounds
    
    // Y-axis viewport bounds (typically -1 to +1 in NDC)
    float y_min{-1.0f};       ///< Minimum Y in normalized device coordinates
    float y_max{1.0f};        ///< Maximum Y in normalized device coordinates
    
    // Y-axis pan offset (interactive vertical scrolling)
    float vertical_pan_offset{0.0f};  ///< Vertical pan in NDC units
    
    // Global scale factors (applied uniformly to all series)
    float global_zoom{1.0f};           ///< Global zoom factor for all series
    float global_vertical_scale{1.0f}; ///< Global vertical scaling factor
    
    /**
     * @brief Construct with default ViewState and TimeRange
     */
    TimeSeriesViewState() = default;
    
    /**
     * @brief Construct from TimeFrame
     * 
     * Initializes time_range from TimeFrame bounds.
     * ViewState must be configured separately based on Y-axis data.
     */
    explicit TimeSeriesViewState(TimeFrame const& tf)
        : time_range(TimeRange::fromTimeFrame(tf)) {}
    
    /**
     * @brief Apply vertical pan delta
     * 
     * Adjusts the vertical_pan_offset by the given amount.
     * Positive values pan upward, negative values pan downward.
     * 
     * @param delta Change in pan offset (in NDC units)
     */
    void applyVerticalPanDelta(float delta) {
        vertical_pan_offset += delta;
    }
    
    /**
     * @brief Reset vertical pan to centered
     */
    void resetVerticalPan() {
        vertical_pan_offset = 0.0f;
    }
    
    /**
     * @brief Get effective Y bounds after pan offset
     * 
     * Returns the Y range accounting for the current pan offset.
     * Used for computing the actual visible Y region.
     */
    [[nodiscard]] std::pair<float, float> getEffectiveYBounds() const {
        return {y_min - vertical_pan_offset, y_max - vertical_pan_offset};
    }
};

} // namespace CorePlotting

#endif // COREPLOTTING_COORDINATETRANSFORM_TIMERANGE_HPP
