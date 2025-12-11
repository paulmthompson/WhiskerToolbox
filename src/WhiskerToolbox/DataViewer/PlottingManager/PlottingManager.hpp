#ifndef DATAVIEWER_PLOTTINGMANAGER_HPP
#define DATAVIEWER_PLOTTINGMANAGER_HPP

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
class TimeFrame;

/**
 * @brief Plotting manager for coordinating multiple data series visualization
 * 
 * Manages the display of multiple time series with different data types,
 * handles coordinate allocation, global scaling, and viewport management.
 * Intended to eventually replace VerticalSpaceManager.
 */
struct PlottingManager {
    // Global scaling and positioning
    float global_zoom{1.0f};          ///< Global zoom factor applied to all series
    float global_vertical_scale{1.0f};///< Global vertical scaling
    float vertical_pan_offset{0.0f};  ///< Global vertical pan offset

    // Viewport configuration
    float viewport_y_min{-1.0f};///< Minimum Y coordinate of viewport in NDC
    float viewport_y_max{1.0f}; ///< Maximum Y coordinate of viewport in NDC

    // Series management
    int total_analog_series{0}; ///< Number of analog series being displayed
    int total_digital_series{0};///< Number of digital series being displayed
    int total_event_series{0};  ///< Number of digital event series being displayed

    // Series storage for DataManager integration
    struct AnalogSeriesInfo {
        std::shared_ptr<AnalogTimeSeries> series;
        std::string key;
        std::string color;
        bool visible{true};
        std::string group_name;
        int channel_id{-1};
    };

    struct DigitalEventSeriesInfo {
        std::shared_ptr<DigitalEventSeries> series;
        std::string key;
        std::string color;
        bool visible{true};
    };

    struct DigitalIntervalSeriesInfo {
        std::shared_ptr<DigitalIntervalSeries> series;
        std::string key;
        std::string color;
        bool visible{true};
    };

    std::unordered_map<std::string, AnalogSeriesInfo> analog_series_map;
    std::unordered_map<std::string, DigitalEventSeriesInfo> digital_event_series_map;
    std::unordered_map<std::string, DigitalIntervalSeriesInfo> digital_interval_series_map;

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
     * @brief Add an analog series with DataManager integration
     * 
     * @param key Unique key for the series
     * @param series Shared pointer to AnalogTimeSeries data
     * @param color Color string for rendering (hex format)
     * @return Series index for the newly added series
     */
    int addAnalogSeries(std::string const & key,
                        std::shared_ptr<AnalogTimeSeries> series,
                        std::string const & color = "");

    /**
     * @brief Add a digital event series with DataManager integration
     * 
     * @param key Unique key for the series
     * @param series Shared pointer to DigitalEventSeries data
     * @param color Color string for rendering (hex format)
     * @return Series index for the newly added series
     */
    int addDigitalEventSeries(std::string const & key,
                              std::shared_ptr<DigitalEventSeries> series,
                              std::string const & color = "");

    /**
     * @brief Add a digital interval series with DataManager integration
     * 
     * @param key Unique key for the series
     * @param series Shared pointer to DigitalIntervalSeries data
     * @param color Color string for rendering (hex format)
     * @return Series index for the newly added series
     */
    int addDigitalIntervalSeries(std::string const & key,
                                 std::shared_ptr<DigitalIntervalSeries> series,
                                 std::string const & color = "");

    /**
     * @brief Remove an analog series by key
     * 
     * @param key Unique key for the series to remove
     * @return True if series was found and removed, false otherwise
     */
    bool removeAnalogSeries(std::string const & key);

    /**
     * @brief Remove a digital event series by key
     * 
     * @param key Unique key for the series to remove
     * @return True if series was found and removed, false otherwise
     */
    bool removeDigitalEventSeries(std::string const & key);

    /**
     * @brief Remove a digital interval series by key
     * 
     * @param key Unique key for the series to remove
     * @return True if series was found and removed, false otherwise
     */
    bool removeDigitalIntervalSeries(std::string const & key);

    /**
     * @brief Clear all series
     */
    void clearAllSeries();

    /**
     * @brief Get analog series info by key
     * 
     * @param key Unique key for the series
     * @return Pointer to AnalogSeriesInfo or nullptr if not found
     */
    AnalogSeriesInfo * getAnalogSeriesInfo(std::string const & key);

    /**
     * @brief Get digital event series info by key
     * 
     * @param key Unique key for the series
     * @return Pointer to DigitalEventSeriesInfo or nullptr if not found
     */
    DigitalEventSeriesInfo * getDigitalEventSeriesInfo(std::string const & key);

    /**
     * @brief Get digital interval series info by key
     * 
     * @param key Unique key for the series
     * @return Pointer to DigitalIntervalSeriesInfo or nullptr if not found
     */
    DigitalIntervalSeriesInfo * getDigitalIntervalSeriesInfo(std::string const & key);

    /**
     * @brief Set series visibility
     * 
     * @param key Unique key for the series
     * @param visible True to show series, false to hide
     */
    void setSeriesVisibility(std::string const & key, bool visible);

    /**
     * @brief Get all visible analog series keys
     * 
     * @return Vector of keys for visible analog series
     */
    std::vector<std::string> getVisibleAnalogSeriesKeys() const;

    /**
     * @brief Get all visible digital event series keys
     * 
     * @return Vector of keys for visible digital event series
     */
    std::vector<std::string> getVisibleDigitalEventSeriesKeys() const;

    /**
     * @brief Get all visible digital interval series keys
     * 
     * @return Vector of keys for visible digital interval series
     */
    std::vector<std::string> getVisibleDigitalIntervalSeriesKeys() const;

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

    /**
     * @brief Update series counts based on currently stored series
     * 
     * Recalculates total_analog_series, total_event_series, and total_digital_series
     * based on the current state of the series maps. Call this after adding/removing series.
     */
    void updateSeriesCounts();

    struct AnalogGroupChannelPosition {
        int channel_id{0};
        float x{0.0f};
        float y{0.0f};
    };

    void loadAnalogSpikeSorterConfiguration(std::string const & group_name,
                                            std::vector<AnalogGroupChannelPosition> const & positions);

    void clearAnalogGroupConfiguration(std::string const & group_name);

    bool getAnalogSeriesAllocationForKey(std::string const & key,
                                         float & allocated_center,
                                         float & allocated_height) const;

private:
    std::unordered_map<std::string, std::vector<AnalogGroupChannelPosition>> _analog_group_configs;

    static bool _extractGroupAndChannel(std::string const & key, std::string & group, int & channel_id);
    std::vector<std::string> _orderedVisibleAnalogKeysByConfig() const;
    /**
     * @brief Generate a default color for a series
     * 
     * @param series_index Index of the series
     * @return Hex color string
     */
    std::string generateDefaultColor(int series_index) const;
};

#endif// DATAVIEWER_PLOTTINGMANAGER_HPP