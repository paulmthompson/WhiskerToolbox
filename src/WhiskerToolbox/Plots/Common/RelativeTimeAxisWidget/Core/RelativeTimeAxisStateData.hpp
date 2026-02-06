#ifndef RELATIVE_TIME_AXIS_STATE_DATA_HPP
#define RELATIVE_TIME_AXIS_STATE_DATA_HPP

/**
 * @file RelativeTimeAxisStateData.hpp
 * @brief Data structures for relative time axis functionality
 * 
 * This header defines the data structures used for relative time axis range
 * in plot widgets. These structures can be included in plot state data structs
 * for serialization.
 */

/**
 * @brief Serializable data for relative time axis settings
 * 
 * This struct contains all relative time axis-related settings that can be
 * included in plot state data structs for serialization.
 */
struct RelativeTimeAxisStateData {
    double min_range = -500.0;  ///< Minimum time range value (default: -500.0)
    double max_range = 500.0;   ///< Maximum time range value (default: 500.0)
};

#endif// RELATIVE_TIME_AXIS_STATE_DATA_HPP
