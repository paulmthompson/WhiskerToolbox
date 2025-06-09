#ifndef DATAVIEWER_PLOTTINGMANAGER_HPP
#define DATAVIEWER_PLOTTINGMANAGER_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>

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

    // Data coordinate system
    int total_data_points{0};  ///< Total number of data points in the dataset
    int visible_start_index{0};///< Start index of visible data range
    int visible_end_index{0};  ///< End index of visible data range

    // Series management
    int total_analog_series{0}; ///< Number of analog series being displayed
    int total_digital_series{0};///< Number of digital series being displayed

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
     * @brief Set the visible data range for projection calculations
     * 
     * @param start_index Start index of visible data
     * @param end_index End index of visible data
     */
    void setVisibleDataRange(int start_index, int end_index);

    /**
     * @brief Add an analog series to the plotting manager
     * 
     * Registers a new analog series and updates internal bookkeeping
     * for coordinate allocation.
     * 
     * @return Series index for the newly added series
     */
    int addAnalogSeries();

    /**
     * @brief Add a digital interval series to the plotting manager
     * 
     * Registers a new digital interval series and updates internal bookkeeping
     * for coordinate allocation. Digital intervals use full canvas height by default.
     * 
     * @return Series index for the newly added digital interval series
     */
    int addDigitalIntervalSeries();

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
};

#endif// DATAVIEWER_PLOTTINGMANAGER_HPP