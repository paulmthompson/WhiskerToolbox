#ifndef COREPLOTTING_COORDINATETRANSFORM_TIMERANGE_HPP
#define COREPLOTTING_COORDINATETRANSFORM_TIMERANGE_HPP

#include <cstdint>
#include <utility>

namespace CorePlotting {

/**
 * @brief View state for time-series plots with real-time/streaming paradigm
 * 
 * This struct manages the visualization state for time-series plotting widgets
 * like OpenGLWidget/DataViewer. It is fundamentally different from the general
 * ViewState used for spatial plots:
 * 
 * **Architectural Distinction:**
 * 
 * | Aspect         | ViewState (Spatial)     | TimeSeriesViewState (Real-time)  |
 * |----------------|-------------------------|----------------------------------|
 * | Buffer scope   | All data loaded once    | Only visible time window         |
 * | X zoom         | MVP transform           | Triggers buffer rebuild          |
 * | X pan          | MVP transform           | External (scrollbar, sync)       |
 * | Y zoom/pan     | MVP transform           | MVP transform                    |
 * | Use case       | Static spatial data     | Real-time streaming              |
 * 
 * **Time Window (X-axis):**
 * - `time_start` and `time_end` define which data is loaded into GPU buffers
 * - Changing the time window triggers a buffer rebuild (not just MVP change)
 * - No bounds enforcement—values outside data range simply show blank space
 * - X panning is typically disabled in the widget (controlled externally)
 * 
 * **Y-axis State:**
 * - Y zoom/pan is purely MVP-based (no buffer changes)
 * - `vertical_pan_offset` allows interactive scrolling
 * - `global_zoom` and `global_vertical_scale` scale all series uniformly
 * 
 * @note Time values are in TimeFrameIndex units (integer indices into TimeFrame)
 */
struct TimeSeriesViewState {
    // =========================================================================
    // Time Window (X-axis) - Defines buffer scope
    // =========================================================================
    
    /// Start of visible time window (TimeFrameIndex units)
    /// Determines left edge of data loaded into buffers
    int64_t time_start{0};
    
    /// End of visible time window (TimeFrameIndex units, inclusive)
    /// Determines right edge of data loaded into buffers
    int64_t time_end{1000};
    
    // =========================================================================
    // Y-axis State (MVP-only, no buffer changes)
    // =========================================================================
    
    /// Minimum Y in normalized device coordinates (bottom of viewport)
    float y_min{-1.0f};
    
    /// Maximum Y in normalized device coordinates (top of viewport)
    float y_max{1.0f};
    
    /// Vertical pan offset in NDC units (positive = pan up)
    float vertical_pan_offset{0.0f};
    
    // =========================================================================
    // Global Scale Factors
    // =========================================================================
    
    /// Global zoom factor applied to all series (affects amplitude scaling)
    float global_zoom{1.0f};
    
    /// Global vertical scale factor applied uniformly to all series
    float global_vertical_scale{1.0f};
    
    // =========================================================================
    // Constructors
    // =========================================================================
    
    /**
     * @brief Default constructor
     * 
     * Creates a view state with default time window [0, 1000] and
     * standard Y bounds [-1, 1].
     */
    TimeSeriesViewState() = default;
    
    /**
     * @brief Construct with explicit time window
     * 
     * @param start Start of visible time window
     * @param end End of visible time window (inclusive)
     */
    TimeSeriesViewState(int64_t start, int64_t end)
        : time_start(start), time_end(end) {}
    
    // =========================================================================
    // Time Window Methods
    // =========================================================================
    
    /**
     * @brief Get visible time window width
     * @return Number of time indices in current visible range (inclusive count)
     */
    [[nodiscard]] int64_t getTimeWidth() const { 
        return time_end - time_start + 1; 
    }
    
    /**
     * @brief Get center of visible time window
     * @return Center time index (rounded down for odd widths)
     */
    [[nodiscard]] int64_t getTimeCenter() const {
        return time_start + (time_end - time_start) / 2;
    }
    
    /**
     * @brief Set time window centered on a point with specified width
     * 
     * This is the primary method for changing the visible time range.
     * Unlike the old TimeRange class, no bounds clamping is performed—
     * if the window extends beyond available data, those areas simply
     * render as blank space.
     * 
     * @param center Center point for the window
     * @param width Desired width of visible range (minimum 1)
     */
    void setTimeWindow(int64_t center, int64_t width) {
        // Ensure minimum width of 1
        if (width < 1) {
            width = 1;
        }
        
        int64_t const half_width = width / 2;
        time_start = center - half_width;
        time_end = time_start + width - 1;
    }
    
    /**
     * @brief Set time window with explicit start and end
     * 
     * @param start Start of visible time window
     * @param end End of visible time window (inclusive)
     */
    void setTimeRange(int64_t start, int64_t end) {
        time_start = start;
        time_end = end;
        
        // Ensure start <= end
        if (time_start > time_end) {
            std::swap(time_start, time_end);
        }
    }
    
    // =========================================================================
    // Y-axis Methods
    // =========================================================================
    
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
