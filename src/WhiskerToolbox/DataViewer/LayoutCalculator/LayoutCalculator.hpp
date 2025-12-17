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


    struct AnalogGroupChannelPosition {
        int channel_id{0};
        float x{0.0f};
        float y{0.0f};
    };

    void loadAnalogSpikeSorterConfiguration(std::string const & group_name,
                                            std::vector<AnalogGroupChannelPosition> const & positions);

    void clearAnalogGroupConfiguration(std::string const & group_name);

private:
    std::unordered_map<std::string, std::vector<AnalogGroupChannelPosition>> _analog_group_configs;
    
    static bool _extractGroupAndChannel(std::string const & key, std::string & group, int & channel_id);
    std::vector<std::string> _orderedVisibleAnalogKeysByConfig(std::vector<std::string> const & visible_keys) const;
};


#endif// DATAVIEWER_LAYOUTCALCULATOR_HPP
