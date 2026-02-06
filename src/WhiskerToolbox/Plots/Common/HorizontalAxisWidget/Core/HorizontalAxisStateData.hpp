#ifndef HORIZONTAL_AXIS_STATE_DATA_HPP
#define HORIZONTAL_AXIS_STATE_DATA_HPP

/**
 * @file HorizontalAxisStateData.hpp
 * @brief Data structures for horizontal axis functionality
 * 
 * This header defines the data structures used for horizontal axis range
 * in plot widgets. These structures can be included in plot state data structs
 * for serialization.
 */

/**
 * @brief Serializable data for horizontal axis settings
 * 
 * This struct contains all horizontal axis-related settings that can be
 * included in plot state data structs for serialization.
 */
struct HorizontalAxisStateData {
    double x_min = 0.0;   ///< X-axis minimum value (default: 0.0)
    double x_max = 100.0; ///< X-axis maximum value (default: 100.0)
};

#endif// HORIZONTAL_AXIS_STATE_DATA_HPP
