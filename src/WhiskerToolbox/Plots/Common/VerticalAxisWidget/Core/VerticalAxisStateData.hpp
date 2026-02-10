#ifndef VERTICAL_AXIS_STATE_DATA_HPP
#define VERTICAL_AXIS_STATE_DATA_HPP

/**
 * @file VerticalAxisStateData.hpp
 * @brief Data structures for vertical axis functionality
 * 
 * This header defines the data structures used for vertical axis range
 * in plot widgets. These structures can be included in plot state data structs
 * for serialization.
 */

/**
 * @brief Serializable data for vertical axis settings
 * 
 * This struct contains all vertical axis-related settings that can be
 * included in plot state data structs for serialization.
 */
struct VerticalAxisStateData {
    double y_min = 0.0;   ///< Y-axis minimum value (default: 0.0)
    double y_max = 100.0; ///< Y-axis maximum value (default: 100.0)
};

#endif// VERTICAL_AXIS_STATE_DATA_HPP
