#ifndef DATAVIEWER_LAYOUTCALCULATOR_HPP
#define DATAVIEWER_LAYOUTCALCULATOR_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations for DataManager types
class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;

/**
 * @brief Layout calculator for coordinating multiple data series visualization
 * 
 * Pure layout computation engine that calculates spatial positioning for multiple
 * time series with different data types. Handles coordinate allocation, global
 * scaling, and viewport management without storing series data.
 * 
 * ARCHITECTURE NOTE (Phase 0 Cleanup):
 * Renamed from PlottingManager to clarify single responsibility as a pure layout
 * calculator. Does NOT store series data - only computes positioning based on
 * series counts and configuration. See CorePlotting/ROADMAP.md Phase 0.
 */
struct LayoutCalculator {
    // Global scaling and positioning
    float global_zoom{1.0f};          ///< Global zoom factor applied to all series
    float global_vertical_scale{1.0f};///< Global vertical scaling
    float vertical_pan_offset{0.0f};  ///< Global vertical pan offset

    // Viewport configuration
    float viewport_y_min{-1.0f};///< Minimum Y coordinate of viewport in NDC
    float viewport_y_max{1.0f}; ///< Maximum Y coordinate of viewport in NDC

    // Series management - counts only, no data storage
    int total_analog_series{0}; ///< Number of analog series being displayed
    int total_digital_series{0};///< Number of digital series being displayed
    int total_event_series{0};  ///< Number of digital event series being displayed

    /**
     * @brief Calculate Y-coordinate allocation for an analog series
     * 
     * Determines the center Y coordinate and allocated height for a specific
     * analog series based on the total number of series and their arrangement.
     * 
     * @param series_index Index of the analog series (0-based)
     * @param allocated_center Output parameter for center Y coordinate
     * @param allocated_height Output parameter for allocated height
     */
    void calculateAnalogSeriesAllocation(int series_index,
                                         float & allocated_center,
                                         float & allocated_height) const;

    /**
     * @brief Set global zoom factor
     * 
     * @param zoom Zoom factor (1.0 = normal, >1.0 = zoomed in, <1.0 = zoomed out)
     */
    void setGlobalZoom(float zoom);

    /**
     * @brief Get current global zoom factor
     * 
     * @return Current zoom factor
     */
    [[nodiscard]] float getGlobalZoom() const;

    /**
     * @brief Set global vertical scale
     * 
     * @param scale Vertical scale factor
     */
    void setGlobalVerticalScale(float scale);

    /**
     * @brief Get current global vertical scale
     * 
     * @return Current vertical scale factor
     */
    [[nodiscard]] float getGlobalVerticalScale() const;

    /**
     * @brief Calculate Y-coordinate allocation for a digital interval series
     * 
     * Determines the center Y coordinate and allocated height for digital interval series.
     * By default, digital intervals extend across the full canvas height for maximum visibility.
     * 
     * @param series_index Index of the digital interval series (0-based)
     * @param allocated_center Output parameter for center Y coordinate
     * @param allocated_height Output parameter for allocated height
     */
    void calculateDigitalIntervalSeriesAllocation(int series_index,
                                                  float & allocated_center,
                                                  float & allocated_height) const;

    /**
     * @brief Calculate Y-coordinate allocation for a digital event series
     * 
     * Determines the center Y coordinate and allocated height for digital event series.
     * Allocation behavior depends on the plotting mode:
     * - FullCanvas mode: Uses full canvas height (like digital intervals)
     * - Stacked mode: Shares space with analog series (like analog time series)
     * 
     * @param series_index Index of the digital event series (0-based)
     * @param allocated_center Output parameter for center Y coordinate
     * @param allocated_height Output parameter for allocated height
     */
    void calculateDigitalEventSeriesAllocation(int series_index,
                                               float & allocated_center,
                                               float & allocated_height) const;

    /**
     * @brief Calculate global stacked allocation for mixed data types
     * 
     * Coordinates allocation between analog series and stacked digital event series.
     * This allows analog time series and digital events to share canvas space
     * proportionally when both are present.
     * 
     * @param analog_series_index Index of analog series (-1 if calculating for event)
     * @param event_series_index Index of digital event series (-1 if calculating for analog)
     * @param total_stackable_series Total number of series sharing the canvas
     * @param allocated_center Output parameter for center Y coordinate
     * @param allocated_height Output parameter for allocated height
     */
    void calculateGlobalStackedAllocation(int analog_series_index,
                                          int event_series_index,
                                          int total_stackable_series,
                                          float & allocated_center,
                                          float & allocated_height) const;

    /**
     * @brief Set vertical pan offset in normalized device coordinates
     * 
     * Sets the vertical panning offset that will be applied to all series equally.
     * Positive values pan upward, negative values pan downward.
     * Values are in normalized device coordinates (typical range: -2.0 to 2.0).
     * 
     * @param pan_offset Vertical pan offset in NDC
     */
    void setPanOffset(float pan_offset);

    /**
     * @brief Apply relative pan delta to current pan offset
     * 
     * Adds a delta to the current pan offset, useful for implementing
     * click-and-drag panning interactions.
     * 
     * @param pan_delta Change in pan offset (positive = upward)
     */
    void applyPanDelta(float pan_delta);

    /**
     * @brief Get current vertical pan offset
     * 
     * @return Current vertical pan offset in normalized device coordinates
     */
    [[nodiscard]] float getPanOffset() const;

    /**
     * @brief Reset pan offset to zero (no panning)
     */
    void resetPan();

    struct AnalogGroupChannelPosition {
        int channel_id{0};
        float x{0.0f};
        float y{0.0f};
    };

    void loadAnalogSpikeSorterConfiguration(std::string const & group_name,
                                            std::vector<AnalogGroupChannelPosition> const & positions);

    void clearAnalogGroupConfiguration(std::string const & group_name);

    /**
     * @brief Get allocation for a specific analog series key considering spike sorter configuration
     * 
     * @param key Series key
     * @param visible_keys All visible series keys (in display order)
     * @param allocated_center Output: Y center
     * @param allocated_height Output: allocated height
     * @return true if allocation was calculated, false otherwise
     */
    bool getAnalogSeriesAllocationForKey(std::string const & key,
                                         std::vector<std::string> const & visible_keys,
                                         float & allocated_center,
                                         float & allocated_height) const;

private:
    std::unordered_map<std::string, std::vector<AnalogGroupChannelPosition>> _analog_group_configs;
    
    static bool _extractGroupAndChannel(std::string const & key, std::string & group, int & channel_id);
    std::vector<std::string> _orderedVisibleAnalogKeysByConfig(std::vector<std::string> const & visible_keys) const;
};

// Legacy type alias for backward compatibility during transition
using PlottingManager = LayoutCalculator;

#endif// DATAVIEWER_LAYOUTCALCULATOR_HPP
